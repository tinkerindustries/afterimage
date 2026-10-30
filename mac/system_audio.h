#pragma once

#include <CoreAudio/CoreAudio.h>

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace Milk {

// Captures everything the Mac is playing through a Core Audio process tap and hands it over as
// left and right samples. The audio thread writes into a ring and the render thread drains it.
class SystemAudioTap {
public:
    SystemAudioTap();
    ~SystemAudioTap();

    bool start(std::string& error);
    void stop();
    bool running() const { return m_proc != nullptr; }
    double sampleRate() const { return m_sampleRate; }

    // Copies up to `max` samples that arrived since the last call. Older samples are dropped
    // when the reader falls more than the ring's length behind.
    int read(float* left, float* right, int max);

private:
    void push(const AudioBufferList* input);

    static constexpr uint64_t RING_SIZE = 1 << 16;

    AudioObjectID m_tap = kAudioObjectUnknown;
    AudioObjectID m_device = kAudioObjectUnknown;
    AudioDeviceIOProcID m_proc = nullptr;
    double m_sampleRate = 48000.0;
    std::vector<float> m_ringL;
    std::vector<float> m_ringR;
    std::atomic<uint64_t> m_written{0};
    uint64_t m_read = 0;
};

} // namespace Milk
