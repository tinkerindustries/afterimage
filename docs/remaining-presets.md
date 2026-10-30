# Remaining presets to fix

This list is a snapshot of one scan of the preset collection. It goes stale as fixes land, so regenerate it rather than editing it by hand:

- `build/milk_shader_check presets -v` lists shaders that fail to translate or compile.
- `build/milk_preset_check presets -v` lists equation code that fails to compile.
- `find presets -name '*.milk' -print0 | sort -z | xargs -0 build/milk_render - 5 256` prints each preset's mean brightness, contrast and saturation after 5 seconds of synthesised audio, starting from noise.

The render classes use these thresholds on that output: near black is mean brightness under 0.02, white is over 0.95, grey is saturation under 0.08, and flat is contrast under 0.02. They flag frames worth a look. Some grey presets are monochrome in MilkDrop too, so check the preview (the `.jpg` beside the `.milk`) before treating one as broken. `build/milk_render sheet.png 5 384 <preset.milk>...` puts previews and renders side by side.

Presets that have no preview .jpg cannot be judged by eye, and many in these lists are probably fine. Three things produce false flags:

- The synthesiser's waveform is smooth and low in treble. Custom waves that scale the waveform sample (`x = .5+.3*sin(sample*20)*(value1*10)`) draw a ring where real music draws blobs. Rendering with `--audio <file>` gives them real input.
- `sampler_randNN` textures pick a random image per preset load, so a preset can render grey in one run and coloured in the next.
- Some presets are chaotic or start from a state that only MilkDrop's previous preset provides.

Paths are relative to `presets/favorite_presets_2021_01_03/`. Each preset appears in one render section: near black first, then white, then grey, then flat.

## Shaders that fail to translate (0 presets)

Every shader translates, compiles as Metal and links. `build/milk_shader_check presets -v` lists any that stop doing so.

## Equation code that fails to compile (2 presets)

MilkDrop joins equation lines with nothing between them, so a line that ends without a `;` runs into the next and the statement is dropped. `build/milk_preset_check presets -v` lists them. None go non-finite in 300 frames.

- high_frequency/suksma - Hexcollie - Julian Carnival - shimmy dumb grid dogmaklyasm nz+5
- high_frequency/suksma - Hexcollie - Julian Carnival - shimmy dumb grid dogmaklyasm nz+6

## Renders near black (67 presets)

Mostly black after 5 seconds.

| Preset | Mean | Contrast | Saturation | Preview |
|---|---|---|---|---|
| $$$ Royal - Mashup (402) | 0.004 | 0.018 | 0.191 | yes |
| $$$ Royal - Mashup (489) | 0.005 | 0.030 | 0.295 | yes |
| $$$ Royal - Mashup (492) | 0.001 | 0.020 | 0.004 | yes |
| 101 | 0.008 | 0.036 | 0.071 | yes |
| 235 | 0.014 | 0.051 | 0.264 | yes |
| 236 | 0.014 | 0.051 | 0.264 | yes |
| Adam Eatit Mashup FX 2 martin - disco mix + Lodus + Geiss + Ludicrous speed + Baked Ft another AdamFX M9 | 0.015 | 0.049 | 0.740 | yes |
| BrainStain-Blackwidow | 0.016 | 0.005 | 0.296 | yes |
| BrainStain-re entry | 0.017 | 0.061 | 0.312 | no |
| EoS+Phat - spectrum bubble new colors - orb sth go 4-ret brothers organs | 0.000 | 0.000 | 0.000 | yes |
| Flexi + bdrv - ultramix #46 test [all your base] | 0.000 | 0.000 | 0.000 | yes |
| Flexi - spatialist | 0.000 | 0.000 | 0.000 | yes |
| Fumbling_Foo + En D & Martin - Mandelverse 31 | 0.000 | 0.007 | 0.002 | yes |
| Fumbling_Foo - Star Forge v7 (HAKAN mash-up) [[pixel theme]] 0-8 v1 | 0.010 | 0.042 | 0.018 | yes |
| Geiss - 3 layers (Minefield Mix) | 0.019 | 0.077 | 0.285 | no |
| Geiss - Dancing Spirits (Bright) | 0.010 | 0.038 | 0.067 | yes |
| Geiss - Flexi - Redi Jedi - Shifter - Phat - Rovastar - 120287110113(457) - orb sth go 4-ret cataclysmic sound | 0.000 | 0.000 | 0.000 | yes |
| Geiss - Iris | 0.011 | 0.017 | 0.052 | no |
| Goody - Sonic Infection | 0.000 | 0.000 | 0.000 | no |
| LuxXx - FearThisStorm | 0.001 | 0.005 | 0.012 | yes |
| Martin - Pixies Party (Hakan mash-up) 2-3 | 0.016 | 0.070 | 0.153 | yes |
| Martin - wire dance | 0.004 | 0.021 | 0.083 | no |
| PHAT HD FX 2 FiShbRaiN  -  Flexi  ft AdamFX Aderrasi , Martin n Rova - Witchcraft Take OverFX  | 0.008 | 0.049 | 0.029 | yes |
| RLP + Geiss - Bass Zoom Redux (3 Layers Minefield Bass Reaction) | 0.003 | 0.017 | 0.148 | yes |
| TonyMilkdrop - Angels Of Glory [Flexi - to get tasered and to like it + techstyle] --- Isosceles edit | 0.008 | 0.024 | 0.046 | yes |
| TonyMilkdrop - Tricolors [Flexi - away with it + $this shall not retain] --- Isosceled edit | 0.015 | 0.107 | 0.015 | yes |
| Tripgnosis - Firesticks | 0.018 | 0.050 | 0.908 | yes |
| _Mig_025 | 0.016 | 0.083 | 0.189 | no |
| _Mig_120 | 0.003 | 0.018 | 0.000 | no |
| amandio c - iterative - squares 4 | 0.018 | 0.118 | 0.012 | yes |
| bowel trounce dumring | 0.014 | 0.051 | 0.671 | yes |
| cope - soar (v2.0) | 0.001 | 0.004 | 0.087 | no |
| fiShbRaiN - witchcraft (ritual dance remix) | 0.005 | 0.039 | 0.076 | yes |
| flexi - lorenz chaser (Jelly V5-5) newborns of satan | 0.003 | 0.038 | 0.015 | yes |
| goddamn nice - umbradg | 0.002 | 0.022 | 0.035 | yes |
| high_frequency/178 | 0.000 | 0.000 | 0.000 | yes |
| high_frequency/Geiss - Flexi - Redi Jedi - Shifter - Phat - Rovastar - 120287110113(457) - orb sth go 4-ret cataclysmi | 0.000 | 0.000 | 0.000 | yes |
| high_frequency/Pithlit - Under Pressure - salt bag collection 1 vs sbc2 | 0.017 | 0.017 | 0.801 | yes |
| high_frequency/amandio c - green machinery 1a hell if i could nz+2 why not, right3 royal weddings are for satanists | 0.007 | 0.050 | 0.037 | yes |
| high_frequency/amandio c - tendrils reflected 1 'nacqtuelle | 0.000 | 0.003 | 0.029 | yes |
| high_frequency/shifter - spincycle(remix) - love at home - electroplasmic pissbowel | 0.001 | 0.009 | 0.026 | yes |
| high_frequency/shifter - spincycle(remix) - love at home - electroplasmic pissbowel roam3- | 0.000 | 0.001 | 0.000 | yes |
| high_frequency/suksma - Chemlock - Spot (Zylot & Aderrasi equations) martin stahlregen - dremp - meds | 0.003 | 0.020 | 0.063 | yes |
| high_frequency/suksma - pus on my cat - hate shade | 0.000 | 0.000 | 0.000 | yes |
| high_frequency/undulant --- Isosceles edit | 0.002 | 0.021 | 0.014 | yes |
| if it's good enough for me to save, it's dead enough for you to re-m flx nz+ | 0.000 | 0.000 | 0.000 | yes |
| lemuel - the search for god is absurdit is if everyone dies alone | 0.011 | 0.054 | 0.199 | yes |
| martin - chain breaker | 0.019 | 0.023 | 0.889 | no |
| nearly unavoidable isolatory strategy flx roams | 0.000 | 0.000 | 0.000 | yes |
| shell robot --- Isosceles edit | 0.000 | 0.000 | 0.000 | yes |
| shell robot nz+ psiqolojiq | 0.009 | 0.022 | 0.427 | yes |
| shifter + flexi - american pie 5 nz+ m3 w3 | 0.013 | 0.012 | 0.991 | yes |
| shifter - fractal grinder (opalescent) | 0.000 | 0.001 | 0.048 | yes |
| shifter - liquid circuitry - suksma defilement | 0.010 | 0.016 | 0.576 | yes |
| shifter - snakeskin (edged) | 0.011 | 0.083 | 0.229 | yes |
| shifter - spincycle c | 0.015 | 0.020 | 0.701 | no |
| suksma - DemonLD_-_Future_domination - mash0000 - sclera torture | 0.018 | 0.071 | 0.090 | yes |
| suksma - autoerotic solipsistic omniversal meme - convoluted debt saturation | 0.012 | 0.050 | 0.164 | yes |
| suksma - biotoxins on strings - busy highway ignores | 0.003 | 0.053 | 0.000 | yes |
| suksma - dotes hostile undertake - antiborg | 0.000 | 0.000 | 0.000 | yes |
| suksma - ed geining hateops - flx DddDddDddDdd die | 0.007 | 0.028 | 0.158 | yes |
| suksma - ed geining hateops - flx beagle | 0.007 | 0.041 | 0.125 | yes |
| suksma - ed geining hateops - flx hate shade | 0.002 | 0.022 | 0.125 | yes |
| suksma - ed geining hateops - flx palette massacrow | 0.003 | 0.001 | 0.926 | yes |
| suksma - ed geining hateops - flx sg5 ink stink you dink | 0.011 | 0.044 | 0.175 | yes |
| suksma - shifter - spincycle + labor of others | 0.014 | 0.006 | 0.607 | yes |
| va ultramix - 154 [chaos] | 0.010 | 0.014 | 0.000 | yes |

## Renders white (42 presets)

Nearly all white after 5 seconds.

| Preset | Mean | Contrast | Saturation | Preview |
|---|---|---|---|---|
| $$$ Royal - Mashup (383) | 1.000 | 0.000 | 0.000 | yes |
| 153 | 0.998 | 0.027 | 0.002 | yes |
| Cope - Passage (mandala mix) | 0.998 | 0.026 | 0.002 | no |
| Geiss - Mash-Up 4 (Jelly) - orb tg riddle lampart dubstanence | 1.000 | 0.000 | 0.000 | yes |
| Hexcollie, Aderassi, ORB, baked n 3dragons - Moving target | 0.986 | 0.087 | 0.011 | yes |
| LuxXx - The Feeder | 1.000 | 0.004 | 0.000 | yes |
| LuxXx - The Feeder II | 1.000 | 0.004 | 0.000 | yes |
| LuxXx - Token Beats | 1.000 | 0.004 | 0.000 | yes |
| Martin N AdamFX Infusion = Phat+Yin+EoS_Mandala Chaser Ft AdamFX n Martin - The Beast Mandala Chaser FX H | 0.979 | 0.087 | 0.026 | yes |
| ORB - Inferno | 0.987 | 0.051 | 0.065 | no |
| ORB - Supernova Meltdown | 1.000 | 0.000 | 0.000 | no |
| Rovastar + Loadus + Geiss - FractalDrop (Triple Mix) | 0.966 | 0.145 | 0.011 | no |
| Rovastar + Loadus + Geiss - Tone-mapped FractalDrop 7c | 0.974 | 0.077 | 0.008 | no |
| Rovastar + Loadus - FractalDrop (Active Sparks Mix) Steady | 0.999 | 0.004 | 0.008 | no |
| Serge + martin - soma | 0.991 | 0.048 | 0.009 | yes |
| Serge circles009 | 0.967 | 0.091 | 0.068 | yes |
| Syst3mFailur - satanic ring_Phat+EoS_DIEty_FREEquency_Remix | 0.990 | 0.018 | 0.037 | yes |
| The NG + Stahlregen & Boz + EoS + Geiss + Phat + Rovastar + Zylot + Flexi + martin + Fishbrain - Machine Code (In The Wind Tunnel) | 0.973 | 0.075 | 0.038 | yes |
| Waltra - Control Freak | 1.000 | 0.001 | 0.001 | yes |
| Waltra - Emitter | 1.000 | 0.000 | 0.000 | yes |
| Waltra - Phantom Spectrum 2 | 1.000 | 0.001 | 0.001 | yes |
| Waltra - Phantom UltraMATTER | 0.995 | 0.021 | 0.014 | yes |
| Zylot - Color Of Music | 1.000 | 0.000 | 0.000 | no |
| Zylot - Crossing Over (Geiss Tweak) | 0.996 | 0.033 | 0.006 | no |
| bdrv + al shifter - phasm mx | 1.000 | 0.000 | 0.000 | yes |
| custom1 | 0.982 | 0.016 | 0.152 | yes |
| fiShbRaiN - betelguese (hyperdrive remix) - hebba | 1.000 | 0.001 | 0.000 | yes |
| fiShbRaiN - betelguese3 aa | 1.000 | 0.002 | 0.000 | yes |
| fleekus flokus - violent and insane homicidal air molecules2 - shf fab we are the info flow, not | 1.000 | 0.012 | 0.001 | yes |
| hey horse teeth girl | 1.000 | 0.000 | 0.000 | yes |
| high_frequency/BEST OF ADAMFX 2 Flexi + Geiss - Bipolar vs reaction diffusion + Deamon Lord + EOS 23 | 0.992 | 0.031 | 0.026 | yes |
| high_frequency/fleekus flokus - violent and insane homicidal air molecules2 - shf fab we are the info flow, not the structure of space time | 1.000 | 0.012 | 0.001 | yes |
| high_frequency/rationalizing insanity using only mainstream religion nz+ barf bowl | 0.976 | 0.043 | 0.100 | yes |
| high_frequency/shifter - phasm wraith (fps+ mix) | 1.000 | 0.000 | 0.000 | yes |
| martin - starfield sectors | 0.991 | 0.051 | 0.010 | yes |
| martin - starfield sectors (Ati fix) | 0.991 | 0.051 | 0.010 | no |
| shifter + flexi - liquid circuitry from the fractalism hive - orb tg lampart revision overdue | 1.000 | 0.000 | 0.000 | yes |
| suksma - diploautomatic autoimmunity vs passivematic aggpressive - crab ride | 0.985 | 0.049 | 0.040 | yes |
| suksma - ed geining hateops - flx shibbalibbadi | 0.963 | 0.086 | 0.031 | yes |
| suksma - newspaper | 0.982 | 0.105 | 0.030 | yes |
| xtramartin (844) | 0.991 | 0.051 | 0.010 | yes |
| yin - 102 - Through the ether (The separation) - Bitcore Tweak | 0.972 | 0.033 | 0.050 | yes |

## Renders grey (86 presets)

Almost no colour.

| Preset | Mean | Contrast | Saturation | Preview |
|---|---|---|---|---|
| $$$ Royal - Mashup (191) | 0.090 | 0.149 | 0.000 | yes |
| A HD ADAMFX Creation  !!FT  Martin + flexi +  Rovastar MDropFX Collaboration E Adam HD INFECTIONFX A | 0.781 | 0.390 | 0.015 | yes |
| A MilkKing Recreation FT Martin  - Violet Flash ft AdamFX n Geiss - Spiral Tunnels in FX  | 0.755 | 0.315 | 0.030 | yes |
| Aderrasi - Ghast (Full Circle Mix) | 0.078 | 0.230 | 0.014 | no |
| Aderrasi - Ghast (Quarter Past Ghast Mix) | 0.097 | 0.171 | 0.069 | no |
| Bdrv et AL Aderrasi - Accelerator (Orbital Migraine) bdvr xxx2_1 - mrt sth | 0.187 | 0.289 | 0.000 | yes |
| Cope-WIP - Strange Attractor 2 | 0.758 | 0.264 | 0.000 | no |
| EVET - Approaching Singularity | 0.816 | 0.181 | 0.072 | yes |
| EVET- Esotic & Rozzer - Slazer Eyes | 0.936 | 0.096 | 0.030 | yes |
| Fed + Geiss - Cauldron painterly 5 - over dots --- Isosceles edit1 | 0.023 | 0.130 | 0.067 | yes |
| Flexi + Rovastar + suksma - Fractopia [sth] | 0.150 | 0.233 | 0.000 | yes |
| Flexi - disruptor [vision of eggs] | 0.391 | 0.428 | 0.000 | yes |
| Flexi - operation acid storm [featuring geiss' rayleigh scattering finish] | 0.048 | 0.073 | 0.000 | yes |
| Geiss - Cosmic Dust 2 - quasistatic noise dual pane window | 0.187 | 0.152 | 0.052 | no |
| Geiss - Digitunnel | 0.068 | 0.150 | 0.076 | no |
| Geiss - Eggs | 0.104 | 0.186 | 0.058 | no |
| Geiss - Game of Life | 0.129 | 0.240 | 0.000 | no |
| Geiss - Hurricane | 0.074 | 0.233 | 0.003 | no |
| Geiss - Kaleidoscope 1 | 0.059 | 0.194 | 0.052 | no |
| Geiss - Pistons | 0.127 | 0.240 | 0.020 | no |
| Geiss - Rose 3 | 0.641 | 0.191 | 0.078 | no |
| Geiss - Rose 4 | 0.083 | 0.184 | 0.044 | no |
| Geiss - Skin Dots Multi-layer 3 | 0.580 | 0.359 | 0.000 | no |
| Geiss - Swirlie 4 | 0.402 | 0.162 | 0.032 | no |
| Geiss - Thumb Drum | 0.424 | 0.164 | 0.000 | no |
| Goody - Unstable Sonic Reactor - Final | 0.824 | 0.292 | 0.067 | no |
| Halfbreak - Noise-volume Negative | 0.650 | 0.271 | 0.064 | yes |
| Jc - Snowflake | 0.109 | 0.265 | 0.034 | yes |
| Martin - Diabolo | 0.121 | 0.087 | 0.077 | no |
| Milk Artist At our Best - FED - SlowFast Ft AdamFX n Martin - HD CosmoFX | 0.749 | 0.004 | 0.000 | yes |
| ORB - Stahl - Glass Ocean | 0.470 | 0.009 | 0.025 | yes |
| Rovastar - Fractopia (Upspoken Mix) | 0.794 | 0.383 | 0.008 | no |
| Serge + 158 002 | 0.829 | 0.255 | 0.015 | yes |
| Serge + martin - crystal palace006 | 0.337 | 0.321 | 0.065 | yes |
| Stahlregen & Aderrasi + bdrv + EoS + flexi + Geiss + krash + shifter + TEcHNO - Liquid Star (RMX)_1 | 0.029 | 0.024 | 0.000 | yes |
| Stahlregen & Boz - Machine Code (Reaction Diffusion)_1 - Isosceles Tweak 06 | 0.468 | 0.161 | 0.000 | yes |
| Stahlregen & Boz - Machine Code (Reaction Diffusion)_1 - Isosceles Tweak 08 | 0.528 | 0.220 | 0.000 | yes |
| Stahlregen & Boz - Machine Code (Reaction Diffusion)_1 - Isosceles Tweak 10 | 0.430 | 0.159 | 0.000 | yes |
| Stahlregen - Old School, Baby! (Black and White Mash) | 0.282 | 0.269 | 0.000 | yes |
| TobiasWolfBoi - The Pit | 0.210 | 0.181 | 0.031 | no |
| TonyMilkdrop - Angels Of Glory [Flexi - an excuse taken back + techstyle] --- Isosceles edit | 0.240 | 0.344 | 0.059 | yes |
| TonyMilkdrop - Angels Of Glory [Flexi - arrogance amplifier + techstyle] --- Isosceles edit | 0.611 | 0.431 | 0.003 | yes |
| Tripgnosis - Antiphotonic Dual | 0.194 | 0.375 | 0.025 | yes |
| Waltra - Cosmic Wind | 0.885 | 0.220 | 0.055 | yes |
| _Aderrasi - Wanderer in Curved Space - mash0000 - faclempt kibitzing meshuggana schmaltz (Geiss greyscale mix) | 0.131 | 0.169 | 0.000 | no |
| _Geiss - Confetti (Kaleidoscope Mix) | 0.055 | 0.206 | 0.000 | no |
| _Mig_146 | 0.429 | 0.105 | 0.000 | no |
| _____________Geiss - Myriad Spirals - cruise mix | 0.448 | 0.092 | 0.066 | no |
| amandio c - hat track - mrt sth | 0.283 | 0.333 | 0.000 | yes |
| amandio c - hat track - mrt sth3 | 0.682 | 0.354 | 0.000 | yes |
| amandio c - iterative - squares 3 | 0.020 | 0.102 | 0.026 | yes |
| amandio c - pulse - sth | 0.655 | 0.373 | 0.000 | yes |
| amandio c - typhoon season (inside) - sth | 0.596 | 0.396 | 0.000 | yes |
| blood red raw holy - sth absolutely blazed lying naked hidden in the savannah | 0.447 | 0.416 | 0.000 | yes |
| cope + flexi - colorful marble (ghost mix) | 0.090 | 0.261 | 0.062 | yes |
| cope - mandelbrot (32 iterations) - mrt mangler | 0.054 | 0.180 | 0.012 | yes |
| fiShbRaiN + Flexi - witchcraft unleashed [00 the template] + shifter - turing tape - stochastic difference engine | 0.183 | 0.075 | 0.026 | yes |
| fiShbRaiN + Flexi - witchcraft unleashed [00 the template] + shifter - turing tape - zit gist | 0.139 | 0.063 | 0.001 | yes |
| fiShbRaiN - betelguese (hyperdrive remix) - as blue as our new second sun | 0.374 | 0.150 | 0.000 | yes |
| fiShbRaiN - the adventures of prismo jenkins | 0.908 | 0.077 | 0.054 | yes |
| fishbrain and flexi - box of tricks [hexcraft 09 - polar] - mrt sth | 0.262 | 0.342 | 0.000 | yes |
| flexi - blame moebius | 0.424 | 0.493 | 0.000 | no |
| flexi - lorenz attractor | 0.028 | 0.101 | 0.051 | no |
| hexcollie - Black Hole | 0.523 | 0.330 | 0.060 | yes |
| high_frequency/170 | 0.136 | 0.258 | 0.038 | yes |
| high_frequency/BEST OF ADAMFX 2 Flexi + Geiss - Bipolar vs reaction diffusion + Aderassi Phat Zylot4 --- Isosceles edit | 0.295 | 0.311 | 0.076 | yes |
| high_frequency/Flexi - self similarity - i love flexi a lot (apple IIe crt whores) nz | 0.033 | 0.172 | 0.009 | yes |
| high_frequency/Halfbreak + Flexi and goody + geiss + benski - Sparkly LSB Feedback' | 0.609 | 0.433 | 0.064 | yes |
| high_frequency/blood red raw holy - btking the taboo | 0.855 | 0.305 | 0.000 | yes |
| high_frequency/shifter - smoked glass - i can't afford to mash this | 0.356 | 0.454 | 0.000 | yes |
| high_frequency/suksma - m-martin w-geiss wp-m c-m - opulaesqueuants pomcromise | 0.940 | 0.131 | 0.062 | yes |
| martin - gate to moria | 0.367 | 0.213 | 0.007 | no |
| martin - move this body | 0.152 | 0.202 | 0.065 | no |
| shifter - phasm wraith - yi qing short term long term dance | 0.273 | 0.195 | 0.000 | yes |
| shifter - spasmic comet neon - bleeding mandellard | 0.035 | 0.080 | 0.064 | yes |
| stahlregen & fishbrain + flexi + geiss + shifter - stonecraft (mashup 19c) | 0.633 | 0.075 | 0.072 | yes |
| suksma - biotoxins on strings - thyghmaster oilstrip lifedecay | 0.249 | 0.249 | 0.000 | yes |
| suksma - biotoxins on strings - walk away + flx per vtx equ | 0.306 | 0.263 | 0.000 | yes |
| suksma - chemosynthetic nosferatu - grey budget | 0.068 | 0.159 | 0.002 | yes |
| suksma - ed geining hateops - flx granimal | 0.865 | 0.108 | 0.053 | yes |
| suksma - flexi coheres vacuum energy - brujchump | 0.534 | 0.446 | 0.016 | yes |
| suksma - pus on my cat - fun bags --- Isosceles edit | 0.613 | 0.446 | 0.046 | yes |
| suksma - yin - 395 - Artificial Inspiration (brainstorming - outward) + martin + flexi + stahlregen - el | 0.112 | 0.290 | 0.004 | yes |
| suksma - yin - 395 - Artificial Inspiration (brainstorming - outward) + martin + flexi + stahlregen - elephant mating - maggotron | 0.112 | 0.290 | 0.004 | yes |
| various - whirygig03 - flx rigidigeridoolittle' - 2 chokmah wisdom grey zodiac neptune twos | 0.398 | 0.336 | 0.000 | yes |
| xtramartin (83) | 0.788 | 0.129 | 0.048 | yes |

## Renders flat (7 presets)

A single flat tone with almost no detail.

| Preset | Mean | Contrast | Saturation | Preview |
|---|---|---|---|---|
| BrainStain- boiling-mix2(redi jedi full carb mix) | 0.254 | 0.006 | 0.272 | yes |
| EVET - Scanazoic --- Isosceles edit | 0.140 | 0.019 | 0.645 | yes |
| EVET - SmoulderingScape --- Isosceles edit | 0.037 | 0.016 | 1.000 | yes |
| Geiss - Explosion 3 nz+ | 0.131 | 0.007 | 1.000 | yes |
| Redi Jedi - pladman gets high at wal-mart | 0.301 | 0.007 | 1.000 | yes |
| flexi + geiss - bipolar vs reaction diffusion [antirotating reflection]_Phat_SeizureBox | 0.782 | 0.001 | 0.764 | yes |
| flexi - grind my glitch up [342] | 0.825 | 0.009 | 1.000 | yes |

## Comparison with MilkDrop captures

The `milkdrop-captures` branch holds 512x512 captures from Milkwave 4.1 (a DirectX 9 port of MilkDrop 2) of 170 presets, taken twice. `tools/compare_frames.py` scores how far two images are from each other, 0 for identical. Presets use random numbers, so MilkDrop is not identical to itself: its two runs score a median of 0.24 against each other. Rendered with `MILK_BANDS=1`, to use Milkwave's audio analysis, our frames score a median of 0.25 against the first run. 126 of 170 are within 0.05 of MilkDrop's own repeatability, and 89 are closer to MilkDrop than it is to itself.

These presets score more than 0.2 above their own repeat-run score, so MilkDrop produces much the same frame each time and ours differs. Each is worth reading against its captures.

| Preset | MilkDrop vs itself | Ours vs MilkDrop |
|---|---|---|
| Jc - Snowflake | 0.09 | 0.59 |
| Flexi - against all ods | 0.00 | 0.49 |
| $$$ Royal - Mashup (96) | 0.23 | 0.61 |
| Fumbling_Foo + En D & Martin - Mandelverse 31 | 0.27 | 0.65 |
| high_frequency__rationalizing insanity using only mainstream religion nz+ barf bowl | 0.33 | 0.71 |
| fiShbRaiN - betelguese (hyperdrive remix) - as blue as our new second sun | 0.16 | 0.49 |
| high_frequency__Flexi - self similarity - i love flexi a lot (apple IIe crt whores) nz | 0.16 | 0.49 |
| flexi + geiss - bipolar vs reaction diffusion [antirotating reflection]_Phat_SeizureBox | 0.27 | 0.59 |
| Serge + 158 001 | 0.12 | 0.37 |
| Serge + 158 000 | 0.14 | 0.38 |
| martin - city lights | 0.13 | 0.37 |
| $$$ Royal - Mashup (377) | 0.08 | 0.30 |
| $$$ Royal - Mashup (158) | 0.23 | 0.44 |
| Goody - Aurora Totalis | 0.25 | 0.45 |

## Mismatches seen on contact sheets

These render with normal brightness but look clearly different from their MilkDrop previews.

- greazed up humility - cable knit sveat: fine grey noise instead of a coloured burst.
- suksma - lick salad dry --- Isosceles edit: purple swirl instead of blue clouds.
- stahlregen - old school, baby! (spiral ornament): dark with thin lines instead of large green blocks.
- RLP + Geiss - Bass Zoom Redux (3 Layers Minefield Bass Reaction): near black instead of bright lavender.
- TonyMilkdrop - Tricolors [Flexi - away with it + $this shall not retain] --- Isosceled edit: black with one small object instead of a full-screen image.
- Flexi + Rovastar + suksma - Fractopia [sth]: grey instead of yellow and purple. Its comp shader reads two random textures, so the colour depends on which images were picked.
- TonyMilkdrop - Angels Of Glory [Flexi - baby blue + $this shall not retain] --- Isosceles edit: the same structure as the preview, in brown instead of blue.
- shifter - spun sugar - smooth cheese: dim red wisps instead of a bright white burst.
- suksma - breaking the beam only to hump the tower --- Isosceles edit: red and orange instead of pale yellow and white.

`flexi - julia island [output stage]` renders flat magenta. It has no preview to compare against.
