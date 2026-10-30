#include "system_audio.h"

#import <CoreAudio/AudioHardwareTapping.h>
#import <CoreAudio/CATapDescription.h>
#import <Foundation/Foundation.h>

#include <algorithm>

namespace Milk {

namespace {

AudioObjectPropertyAddress globalAddress(AudioObjectPropertySelector selector) {
    return AudioObjectPropertyAddress{selector, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain};
}

std::string statusError(const char* what, OSStatus status) {
    return std::string(what) + " failed with OSStatus " + std::to_string(status);
}

NSString* defaultOutputUid(std::string& error) {
    AudioObjectID device = kAudioObjectUnknown;
    UInt32 size = sizeof(device);
    AudioObjectPropertyAddress address = globalAddress(kAudioHardwarePropertyDefaultSystemOutputDevice);
    OSStatus status = AudioObjectGetPropertyData(kAudioObjectSystemObject, &address, 0, nullptr, &size, &device);
    if (status != noErr) {
        error = statusError("reading the default output device", status);
        return nil;
    }
    CFStringRef uid = nullptr;
    size = sizeof(uid);
    address = globalAddress(kAudioDevicePropertyDeviceUID);
    status = AudioObjectGetPropertyData(device, &address, 0, nullptr, &size, &uid);
    if (status != noErr) {
        error = statusError("reading the output device's UID", status);
        return nil;
    }
    return (__bridge_transfer NSString*)uid;
}

} // namespace

SystemAudioTap::SystemAudioTap() : m_ringL(RING_SIZE, 0.0f), m_ringR(RING_SIZE, 0.0f) {}

SystemAudioTap::~SystemAudioTap() { stop(); }

bool SystemAudioTap::start(std::string& error) {
    if (running()) return true;

    CATapDescription* description = [[CATapDescription alloc] initStereoGlobalTapButExcludeProcesses:@[]];
    description.UUID = [NSUUID UUID];
    description.name = @"Milk system audio";
    description.privateTap = YES;
    description.muteBehavior = CATapUnmuted;
    OSStatus status = AudioHardwareCreateProcessTap(description, &m_tap);
    if (status != noErr) {
        error = statusError("AudioHardwareCreateProcessTap", status);
        stop();
        return false;
    }

    AudioStreamBasicDescription format{};
    UInt32 size = sizeof(format);
    AudioObjectPropertyAddress address = globalAddress(kAudioTapPropertyFormat);
    status = AudioObjectGetPropertyData(m_tap, &address, 0, nullptr, &size, &format);
    if (status != noErr) {
        error = statusError("reading the tap format", status);
        stop();
        return false;
    }
    if (format.mFormatID != kAudioFormatLinearPCM || !(format.mFormatFlags & kAudioFormatFlagIsFloat) ||
        format.mBitsPerChannel != 32) {
        error = "the tap delivers a format other than 32-bit float PCM";
        stop();
        return false;
    }
    m_sampleRate = format.mSampleRate;

    NSString* outputUid = defaultOutputUid(error);
    if (outputUid == nil) {
        stop();
        return false;
    }
    NSDictionary* aggregate = @{
        @kAudioAggregateDeviceNameKey: @"Milk system audio",
        @kAudioAggregateDeviceUIDKey: [NSUUID UUID].UUIDString,
        @kAudioAggregateDeviceMainSubDeviceKey: outputUid,
        @kAudioAggregateDeviceIsPrivateKey: @YES,
        @kAudioAggregateDeviceIsStackedKey: @NO,
        @kAudioAggregateDeviceTapAutoStartKey: @YES,
        @kAudioAggregateDeviceSubDeviceListKey: @[@{@kAudioSubDeviceUIDKey: outputUid}],
        @kAudioAggregateDeviceTapListKey: @[@{
            @kAudioSubTapDriftCompensationKey: @YES,
            @kAudioSubTapUIDKey: description.UUID.UUIDString,
        }],
    };
    status = AudioHardwareCreateAggregateDevice((__bridge CFDictionaryRef)aggregate, &m_device);
    if (status != noErr) {
        error = statusError("AudioHardwareCreateAggregateDevice", status);
        stop();
        return false;
    }

    dispatch_queue_t queue = dispatch_queue_create("milk.system-audio", DISPATCH_QUEUE_SERIAL);
    status = AudioDeviceCreateIOProcIDWithBlock(&m_proc, m_device, queue,
        ^(const AudioTimeStamp*, const AudioBufferList* input, const AudioTimeStamp*, AudioBufferList*, const AudioTimeStamp*) {
            push(input);
        });
    if (status != noErr) {
        m_proc = nullptr;
        error = statusError("AudioDeviceCreateIOProcIDWithBlock", status);
        stop();
        return false;
    }
    status = AudioDeviceStart(m_device, m_proc);
    if (status != noErr) {
        error = statusError("AudioDeviceStart", status);
        stop();
        return false;
    }
    m_read = m_written.load(std::memory_order_acquire);
    return true;
}

void SystemAudioTap::stop() {
    if (m_device != kAudioObjectUnknown && m_proc != nullptr) {
        AudioDeviceStop(m_device, m_proc);
        AudioDeviceDestroyIOProcID(m_device, m_proc);
    }
    m_proc = nullptr;
    if (m_device != kAudioObjectUnknown) AudioHardwareDestroyAggregateDevice(m_device);
    m_device = kAudioObjectUnknown;
    if (m_tap != kAudioObjectUnknown) AudioHardwareDestroyProcessTap(m_tap);
    m_tap = kAudioObjectUnknown;
}

// Takes the first two channels as left and right, whether they arrive interleaved in one buffer or
// as separate buffers. A mono stream feeds both.
void SystemAudioTap::push(const AudioBufferList* input) {
    if (input == nullptr || input->mNumberBuffers == 0) return;
    const AudioBuffer& first = input->mBuffers[0];
    const UInt32 firstChannels = std::max<UInt32>(first.mNumberChannels, 1);
    const UInt32 frames = first.mDataByteSize / (sizeof(float) * firstChannels);
    const float* l = static_cast<const float*>(first.mData);
    const float* r = l;
    UInt32 lStride = firstChannels;
    UInt32 rStride = firstChannels;
    UInt32 rOffset = firstChannels > 1 ? 1 : 0;
    if (firstChannels == 1 && input->mNumberBuffers > 1 && input->mBuffers[1].mData != nullptr &&
        input->mBuffers[1].mDataByteSize >= frames * sizeof(float)) {
        r = static_cast<const float*>(input->mBuffers[1].mData);
        rStride = std::max<UInt32>(input->mBuffers[1].mNumberChannels, 1);
        rOffset = 0;
    }
    if (l == nullptr) return;
    uint64_t written = m_written.load(std::memory_order_relaxed);
    for (UInt32 f = 0; f < frames; ++f) {
        m_ringL[written % RING_SIZE] = l[f * lStride];
        m_ringR[written % RING_SIZE] = r[f * rStride + rOffset];
        ++written;
    }
    m_written.store(written, std::memory_order_release);
}

int SystemAudioTap::read(float* left, float* right, int max) {
    const uint64_t written = m_written.load(std::memory_order_acquire);
    if (written - m_read > RING_SIZE) m_read = written - RING_SIZE;
    const int count = static_cast<int>(std::min<uint64_t>(written - m_read, static_cast<uint64_t>(max)));
    // Keep the newest samples when there are more than fit.
    const uint64_t start = written - count;
    for (int i = 0; i < count; ++i) {
        left[i] = m_ringL[(start + i) % RING_SIZE];
        right[i] = m_ringR[(start + i) % RING_SIZE];
    }
    m_read = written;
    return count;
}

} // namespace Milk
