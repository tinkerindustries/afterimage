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

MilkDrop joins equation lines with nothing between them, so a line that ends without a `;` runs into the next and the whole block is dropped. `build/milk_preset_check presets -v` lists them. None go non-finite in 300 frames.

- high_frequency/suksma - Hexcollie - Julian Carnival - shimmy dumb grid dogmaklyasm nz+5
- high_frequency/suksma - Hexcollie - Julian Carnival - shimmy dumb grid dogmaklyasm nz+6

## Renders near black (65 presets)

Mostly black after 5 seconds.

| Preset | Mean | Contrast | Saturation | Preview |
|---|---|---|---|---|
| $$$ Royal - Mashup (191) | 0.009 | 0.044 | 0.000 | yes |
| $$$ Royal - Mashup (402) | 0.003 | 0.018 | 0.112 | yes |
| $$$ Royal - Mashup (489) | 0.005 | 0.030 | 0.249 | yes |
| $$$ Royal - Mashup (492) | 0.001 | 0.020 | 0.003 | yes |
| 101 | 0.000 | 0.000 | 0.000 | yes |
| 235 | 0.013 | 0.051 | 0.216 | yes |
| 236 | 0.013 | 0.051 | 0.216 | yes |
| Adam Eatit Mashup FX 2 martin - disco mix + Lodus + Geiss + Ludicrous speed + Baked Ft another AdamFX M9 | 0.015 | 0.049 | 0.744 | yes |
| Aderrasi - Ghast Entity | 0.013 | 0.028 | 0.218 | no |
| BrainStain-re entry | 0.017 | 0.061 | 0.316 | no |
| EoS+Phat - spectrum bubble new colors - orb sth go 4-ret brothers organs | 0.000 | 0.000 | 0.000 | yes |
| Fed + Geiss - Cauldron painterly 5 - over dots --- Isosceles edit1 | 0.012 | 0.099 | 0.015 | yes |
| Flexi - spatialist | 0.000 | 0.000 | 0.000 | yes |
| Fumbling_Foo + En D & Martin - Mandelverse 31 | 0.003 | 0.022 | 0.013 | yes |
| Fumbling_Foo - Star Forge v7 (HAKAN mash-up) [[pixel theme]] 0-8 v1 | 0.010 | 0.043 | 0.018 | yes |
| Geiss - Cosmic Dust 2 - quasistatic noise desat trails | 0.009 | 0.018 | 0.078 | no |
| Geiss - Dancing Spirits (Bright) | 0.010 | 0.039 | 0.067 | yes |
| Geiss - Flexi - Redi Jedi - Shifter - Phat - Rovastar - 120287110113(457) - orb sth go 4-ret cataclysmic sound | 0.000 | 0.000 | 0.000 | yes |
| Geiss - Game of Life 3 | 0.000 | 0.000 | 0.000 | no |
| Geiss - Mosaic Octopus 7 | 0.014 | 0.066 | 0.051 | no |
| Goody - Sonic Infection | 0.000 | 0.000 | 0.000 | no |
| Martin - Pixies Party (Hakan mash-up) 2-3 | 0.016 | 0.070 | 0.141 | yes |
| Martin - wire dance | 0.008 | 0.032 | 0.116 | no |
| ORB - Quicksand Lab | 0.009 | 0.017 | 0.612 | no |
| PHAT HD FX 2 FiShbRaiN  -  Flexi  ft AdamFX Aderrasi , Martin n Rova - Witchcraft Take OverFX  | 0.008 | 0.049 | 0.029 | yes |
| RLP + Geiss - Bass Zoom Redux (3 Layers Minefield Bass Reaction) | 0.004 | 0.017 | 0.155 | yes |
| Stahlregen & Geiss + ORB - Etheral Waves (All-Spark RMX) - sth shd | 0.011 | 0.046 | 0.110 | yes |
| TonyMilkdrop - Angels Of Glory [Flexi - to get tasered and to like it + techstyle] --- Isosceles edit | 0.007 | 0.024 | 0.051 | yes |
| TonyMilkdrop - Tricolors [Flexi - away with it + $this shall not retain] --- Isosceled edit | 0.017 | 0.114 | 0.018 | yes |
| Tripgnosis - Firesticks | 0.018 | 0.050 | 0.913 | yes |
| _Geiss - Confetti (Kaleidoscope Mix) | 0.012 | 0.093 | 0.000 | no |
| _Mig_120 | 0.004 | 0.018 | 0.000 | no |
| amandio c - iterative - squares 4 | 0.018 | 0.118 | 0.012 | yes |
| amandio c - log tower 1 pig vomit 2uqle4 dorqstre l-egrunth | 0.019 | 0.096 | 0.078 | yes |
| bowel trounce dumring | 0.014 | 0.051 | 0.668 | yes |
| eschatonal procrastination kery | 0.008 | 0.036 | 0.054 | yes |
| fiShbRaiN - witchcraft (ritual dance remix) | 0.005 | 0.039 | 0.072 | yes |
| flexi - lorenz chaser (Jelly V5-5) newborns of satan | 0.003 | 0.040 | 0.010 | yes |
| goddamn nice - umbradg | 0.002 | 0.022 | 0.030 | yes |
| high_frequency/178 | 0.000 | 0.000 | 0.000 | yes |
| high_frequency/Geiss - Flexi - Redi Jedi - Shifter - Phat - Rovastar - 120287110113(457) - orb sth go 4-ret cataclysmi | 0.000 | 0.000 | 0.000 | yes |
| high_frequency/Pithlit - Under Pressure - salt bag collection 1 vs sbc2 | 0.018 | 0.019 | 0.796 | yes |
| high_frequency/amandio c - green machinery 1a hell if i could nz+2 why not, right3 royal weddings are for satanists | 0.007 | 0.049 | 0.037 | yes |
| high_frequency/amandio c - tendrils reflected 1 'nacqtuelle | 0.000 | 0.003 | 0.029 | yes |
| high_frequency/shifter - spincycle(remix) - love at home - electroplasmic pissbowel | 0.001 | 0.009 | 0.025 | yes |
| high_frequency/shifter - spincycle(remix) - love at home - electroplasmic pissbowel roam3- | 0.000 | 0.001 | 0.000 | yes |
| high_frequency/suksma - Chemlock - Spot (Zylot & Aderrasi equations) martin stahlregen - dremp - meds | 0.014 | 0.043 | 0.190 | yes |
| lemuel - the search for god is absurdit is if everyone dies alone | 0.009 | 0.048 | 0.173 | yes |
| martin - chain breaker | 0.019 | 0.023 | 0.891 | no |
| shadowharlequin + flexi - lovely shiney squares [originally bipolar] | 0.014 | 0.027 | 0.405 | yes |
| shell robot --- Isosceles edit | 0.005 | 0.003 | 0.457 | yes |
| shell robot nz+ psiqolojiq | 0.015 | 0.020 | 0.978 | yes |
| shifter - fractal grinder (opalescent) | 0.013 | 0.012 | 0.321 | yes |
| shifter - liquid circuitry - suksma defilement | 0.010 | 0.016 | 0.569 | yes |
| shifter - spincycle c | 0.015 | 0.020 | 0.712 | no |
| suksma - DemonLD_-_Future_domination - mash0000 - sclera torture | 0.018 | 0.071 | 0.090 | yes |
| suksma - autoerotic solipsistic omniversal meme - convoluted debt saturation | 0.012 | 0.050 | 0.165 | yes |
| suksma - biotoxins on strings - busy highway ignores | 0.013 | 0.111 | 0.000 | yes |
| suksma - ed geining hateops - flx DddDddDddDdd die | 0.007 | 0.029 | 0.162 | yes |
| suksma - ed geining hateops - flx beagle | 0.006 | 0.040 | 0.112 | yes |
| suksma - ed geining hateops - flx hate shade | 0.002 | 0.017 | 0.130 | yes |
| suksma - ed geining hateops - flx palette massacrow | 0.009 | 0.002 | 0.624 | yes |
| suksma - ed geining hateops - flx sg5 ink stink you dink | 0.012 | 0.045 | 0.221 | yes |
| suksma - shifter - spincycle + labor of others | 0.014 | 0.006 | 0.614 | yes |
| va ultramix - 154 [chaos] | 0.010 | 0.014 | 0.000 | yes |

## Renders white (37 presets)

Nearly all white after 5 seconds.

| Preset | Mean | Contrast | Saturation | Preview |
|---|---|---|---|---|
| $$$ Royal - Mashup (383) | 1.000 | 0.000 | 0.000 | yes |
| 153 | 0.998 | 0.027 | 0.002 | yes |
| Cope - Passage (mandala mix) | 0.988 | 0.061 | 0.016 | no |
| Geiss - Mash-Up 4 (Jelly) - orb tg riddle lampart dubstanence | 1.000 | 0.000 | 0.000 | yes |
| Hexcollie - X warp harsh - klive | 0.962 | 0.070 | 0.161 | yes |
| Hexcollie, Aderassi, ORB, baked n 3dragons - Moving target | 0.986 | 0.087 | 0.011 | yes |
| LuxXx - The Feeder | 1.000 | 0.004 | 0.000 | yes |
| LuxXx - The Feeder II | 1.000 | 0.004 | 0.000 | yes |
| LuxXx - Token Beats | 1.000 | 0.005 | 0.000 | yes |
| Martin N AdamFX Infusion = Phat+Yin+EoS_Mandala Chaser Ft AdamFX n Martin - The Beast Mandala Chaser FX H | 0.980 | 0.086 | 0.025 | yes |
| ORB - Cloud Burst | 1.000 | 0.000 | 0.000 | yes |
| ORB - Supernova Meltdown | 1.000 | 0.000 | 0.000 | no |
| Rovastar + Loadus + Geiss - FractalDrop (Triple Mix) | 0.963 | 0.154 | 0.012 | no |
| Rovastar + Loadus + Geiss - Tone-mapped FractalDrop 7c | 0.974 | 0.077 | 0.008 | no |
| Rovastar + Loadus - FractalDrop (Active Sparks Mix) Steady | 0.999 | 0.005 | 0.007 | no |
| Serge circles009 | 0.979 | 0.066 | 0.046 | yes |
| Waltra - Control Freak | 1.000 | 0.001 | 0.002 | yes |
| Waltra - Phantom Spectrum 2 | 0.999 | 0.005 | 0.003 | yes |
| Waltra - Phantom UltraMATTER | 0.997 | 0.015 | 0.009 | yes |
| Zylot - Crossing Over (Geiss Tweak) | 0.997 | 0.032 | 0.005 | no |
| _Mig_082_version2 | 0.952 | 0.146 | 0.078 | no |
| bdrv + al shifter - phasm mx | 1.000 | 0.000 | 0.000 | yes |
| custom1 | 0.982 | 0.016 | 0.150 | yes |
| fiShbRaiN - betelguese (hyperdrive remix) - hebba | 1.000 | 0.001 | 0.000 | yes |
| fiShbRaiN - betelguese3 aa | 1.000 | 0.002 | 0.000 | yes |
| hey horse teeth girl | 1.000 | 0.000 | 0.000 | yes |
| high_frequency/BEST OF ADAMFX 2 Flexi + Geiss - Bipolar vs reaction diffusion + Deamon Lord + EOS 23 | 0.993 | 0.032 | 0.013 | yes |
| high_frequency/rationalizing insanity using only mainstream religion nz+ barf bowl | 0.978 | 0.040 | 0.093 | yes |
| high_frequency/shifter - phasm wraith (fps+ mix) | 1.000 | 0.000 | 0.000 | yes |
| martin - starfield sectors | 0.991 | 0.049 | 0.009 | yes |
| martin - starfield sectors (Ati fix) | 0.991 | 0.049 | 0.009 | no |
| shifter + flexi - liquid circuitry from the fractalism hive - orb tg lampart revision overdue | 1.000 | 0.000 | 0.000 | yes |
| suksma - diploautomatic autoimmunity vs passivematic aggpressive - crab ride | 0.995 | 0.019 | 0.017 | yes |
| suksma - ed geining hateops - flx shibbalibbadi | 0.964 | 0.085 | 0.029 | yes |
| suksma - newspaper | 0.982 | 0.105 | 0.029 | yes |
| xtramartin (844) | 0.991 | 0.049 | 0.009 | yes |
| yin - 102 - Through the ether (The separation) - Bitcore Tweak | 0.971 | 0.033 | 0.051 | yes |

## Renders grey (111 presets)

Almost no colour.

| Preset | Mean | Contrast | Saturation | Preview |
|---|---|---|---|---|
| A HD ADAMFX Creation  !!FT  Martin + flexi +  Rovastar MDropFX Collaboration E Adam HD INFECTIONFX A | 0.781 | 0.389 | 0.015 | yes |
| A MilkKing Recreation FT Martin  - Violet Flash ft AdamFX n Geiss - Spiral Tunnels in CosmicFX Dust  | 0.024 | 0.076 | 0.047 | yes |
| A MilkKing Recreation FT Martin  - Violet Flash ft AdamFX n Geiss - Spiral Tunnels in FX  | 0.464 | 0.310 | 0.070 | yes |
| Aderrasi - Ghast (Full Circle Mix) | 0.122 | 0.144 | 0.015 | no |
| Aderrasi - Ghast (Quarter Past Ghast Mix) | 0.104 | 0.166 | 0.067 | no |
| Aderrasi - Kevlar Abyss | 0.088 | 0.031 | 0.015 | no |
| Bdrv et AL Aderrasi - Accelerator (Orbital Migraine) bdvr xxx2_1 - mrt sth | 0.031 | 0.132 | 0.000 | yes |
| Cope-WIP - Strange Attractor 2 | 0.751 | 0.261 | 0.000 | no |
| Flexi + Rovastar + suksma - Fractopia [sth] | 0.190 | 0.236 | 0.000 | yes |
| Flexi - disruptor [vision of eggs] | 0.393 | 0.429 | 0.000 | yes |
| Flexi - operation acid storm [featuring geiss' rayleigh scattering finish] | 0.084 | 0.103 | 0.000 | yes |
| Geiss - Casino | 0.190 | 0.074 | 0.074 | no |
| Geiss - Cosmic Dust 2 | 0.212 | 0.191 | 0.072 | no |
| Geiss - Cosmic Dust 2 - quasistatic noise desat trails 2 | 0.226 | 0.126 | 0.052 | no |
| Geiss - Cosmic Dust 2 - quasistatic noise dual pane window | 0.141 | 0.098 | 0.023 | no |
| Geiss - Digitunnel | 0.069 | 0.150 | 0.063 | no |
| Geiss - Eggs | 0.188 | 0.150 | 0.011 | no |
| Geiss - Electric Storm Half-Digital 2 | 0.027 | 0.060 | 0.051 | no |
| Geiss - Flower | 0.238 | 0.197 | 0.079 | no |
| Geiss - Flower Blossom | 0.126 | 0.001 | 0.001 | no |
| Geiss - Game of Life | 0.129 | 0.242 | 0.000 | no |
| Geiss - Hurricane | 0.143 | 0.099 | 0.001 | no |
| Geiss - Iris | 0.086 | 0.002 | 0.000 | no |
| Geiss - Kaleidoscope 1 | 0.030 | 0.136 | 0.007 | no |
| Geiss - Mosaic | 0.187 | 0.169 | 0.040 | no |
| Geiss - Pistons | 0.205 | 0.202 | 0.010 | no |
| Geiss - Rose 3 | 0.644 | 0.191 | 0.078 | no |
| Geiss - Rose 4 | 0.084 | 0.185 | 0.044 | no |
| Geiss - Skin Dots Multi-layer 3 | 0.567 | 0.362 | 0.000 | no |
| Geiss - Smoke Trail | 0.202 | 0.161 | 0.061 | no |
| Geiss - Spiral Artifact (Filament Mix) - rad8 mix | 0.114 | 0.035 | 0.061 | yes |
| Geiss - Swirlie 4 | 0.393 | 0.162 | 0.033 | no |
| Geiss - Thumb Drum | 0.423 | 0.164 | 0.000 | no |
| Geiss - Trampoline | 0.197 | 0.070 | 0.071 | no |
| Goody - Lightning - Sonic Warp remix (PS2-0) | 0.117 | 0.286 | 0.066 | yes |
| Halfbreak - Noise-volume Negative | 0.641 | 0.271 | 0.065 | yes |
| Jc - Snowflake | 0.130 | 0.258 | 0.029 | yes |
| LuxXx - Dain Bammerge v1 (balls) | 0.678 | 0.422 | 0.071 | yes |
| Martin - Diabolo | 0.121 | 0.087 | 0.078 | no |
| Milk Artist At our Best - FED - SlowFast Ft AdamFX n Martin - HD CosmoFX | 0.744 | 0.039 | 0.011 | yes |
| ORB - Stahl - Glass Ocean | 0.473 | 0.009 | 0.031 | yes |
| Rovastar + Fvese - Mosaic Waves | 0.232 | 0.054 | 0.055 | no |
| Rovastar - Fractopia (Upspoken Mix) | 0.794 | 0.383 | 0.008 | no |
| Serge + 158 002 | 0.706 | 0.290 | 0.027 | yes |
| Serge + martin - crystal palace006 | 0.522 | 0.278 | 0.045 | yes |
| Serge + martin - crystal palace007d | 0.105 | 0.131 | 0.057 | yes |
| Serge + martin - soma | 0.895 | 0.202 | 0.077 | yes |
| Serge mashup003 | 0.927 | 0.158 | 0.055 | yes |
| Stahlregen & Aderrasi + bdrv + EoS + flexi + Geiss + krash + shifter + TEcHNO - Liquid Star (RMX)_1 | 0.029 | 0.024 | 0.000 | yes |
| Stahlregen & Boz - Machine Code (Reaction Diffusion)_1 - Isosceles Tweak 06 | 0.855 | 0.209 | 0.000 | yes |
| Stahlregen & Boz - Machine Code (Reaction Diffusion)_1 - Isosceles Tweak 08 | 0.621 | 0.268 | 0.000 | yes |
| Stahlregen & Boz - Machine Code (Reaction Diffusion)_1 - Isosceles Tweak 10 | 0.430 | 0.156 | 0.000 | yes |
| Stahlregen - Old School, Baby! (Black and White Mash) | 0.249 | 0.240 | 0.000 | yes |
| TobiasWolfBoi - The Pit | 0.200 | 0.162 | 0.022 | no |
| TonyMilkdrop - Angels Of Glory [Flexi - an excuse taken back + techstyle] --- Isosceles edit | 0.283 | 0.380 | 0.052 | yes |
| TonyMilkdrop - Angels Of Glory [Flexi - arrogance amplifier + techstyle] --- Isosceles edit | 0.591 | 0.433 | 0.005 | yes |
| TonyMilkdrop - Angels Of Glory [Flexi - safer sex + techstyle] --- Isosceles edit | 0.220 | 0.386 | 0.076 | yes |
| TonyMilkdrop - Nuclear [Flexi - help out + techstyle] --- Isosceles edit2 | 0.834 | 0.297 | 0.062 | yes |
| Tripgnosis - Antiphotonic Dual | 0.195 | 0.376 | 0.023 | yes |
| _Aderrasi - Wanderer in Curved Space - mash0000 - faclempt kibitzing meshuggana schmaltz (Geiss greyscale mix) | 0.146 | 0.145 | 0.000 | no |
| _Geiss - Spiral Artifact (Filament Mix) - beat dots mix | 0.620 | 0.021 | 0.064 | no |
| _Mig_025 | 0.040 | 0.042 | 0.026 | no |
| _Mig_146 | 0.423 | 0.092 | 0.000 | no |
| _Mig_304 - geiss remix 3 | 0.048 | 0.035 | 0.063 | no |
| _Mig_304 - geiss remix 3b | 0.050 | 0.045 | 0.051 | no |
| _____________Geiss - Myriad Spirals - cruise mix | 0.439 | 0.079 | 0.066 | no |
| amandio c - hat track - mrt sth | 0.286 | 0.334 | 0.000 | yes |
| amandio c - hat track - mrt sth3 | 0.684 | 0.353 | 0.000 | yes |
| amandio c - iterative - squares 3 | 0.020 | 0.102 | 0.026 | yes |
| amandio c - pulse - sth | 0.649 | 0.375 | 0.000 | yes |
| amandio c - typhoon season (inside) - sth | 0.602 | 0.394 | 0.000 | yes |
| blood red raw holy - sth absolutely blazed lying naked hidden in the savannah | 0.379 | 0.414 | 0.000 | yes |
| cope + flexi - colorful marble (ghost mix) | 0.090 | 0.262 | 0.057 | yes |
| cope - mandelbrot (32 iterations) - mrt mangler | 0.053 | 0.180 | 0.012 | yes |
| fiShbRaiN + Flexi - witchcraft unleashed [00 the template] + shifter - turing tape - stochastic difference engine | 0.177 | 0.081 | 0.054 | yes |
| fiShbRaiN + Flexi - witchcraft unleashed [00 the template] + shifter - turing tape - zit gist | 0.131 | 0.061 | 0.021 | yes |
| fiShbRaiN - betelguese (hyperdrive remix) - as blue as our new second sun | 0.375 | 0.149 | 0.000 | yes |
| fiShbRaiN - the adventures of prismo jenkins | 0.881 | 0.096 | 0.060 | yes |
| fishbrain and flexi - box of tricks [hexcraft 09 - polar] - mrt sth | 0.373 | 0.329 | 0.000 | yes |
| flexi - blame moebius | 0.424 | 0.493 | 0.000 | no |
| flexi - lorenz attractor | 0.028 | 0.101 | 0.051 | no |
| geiss + mstress - Cleaning a path - Painterly Crossfire (Crisp Emboss Mix) | 0.425 | 0.104 | 0.063 | no |
| hexcollie - Black Hole | 0.524 | 0.330 | 0.065 | yes |
| high_frequency/170 | 0.071 | 0.183 | 0.018 | yes |
| high_frequency/BEST OF ADAMFX 2 Flexi + Geiss - Bipolar vs reaction diffusion + Aderassi Phat Zylot4 --- Isosceles edit | 0.294 | 0.310 | 0.075 | yes |
| high_frequency/Flexi - self similarity - i love flexi a lot (apple IIe crt whores) nz | 0.033 | 0.172 | 0.010 | yes |
| high_frequency/blood red raw holy - btking the taboo | 0.891 | 0.262 | 0.000 | yes |
| high_frequency/shifter - smoked glass - i can't afford to mash this | 0.448 | 0.470 | 0.000 | yes |
| high_frequency/suksma - pus on my cat - hate shade | 0.452 | 0.465 | 0.048 | yes |
| martin - gate to moria | 0.369 | 0.216 | 0.007 | no |
| martin - move this body | 0.153 | 0.204 | 0.066 | no |
| shifter - digi | 0.040 | 0.088 | 0.062 | no |
| shifter - neon pulse | 0.047 | 0.110 | 0.036 | yes |
| shifter - phasm wraith - yi qing short term long term dance | 0.411 | 0.030 | 0.000 | yes |
| shifter - snakeskin (edged) | 0.039 | 0.084 | 0.044 | yes |
| shifter - spasmic comet neon - bleeding mandellard | 0.036 | 0.081 | 0.063 | yes |
| shifter - spills blender + krash beatdetect - mash0000 - here comb your hair with this planet | 0.236 | 0.166 | 0.075 | no |
| stahlregen & fishbrain + flexi + geiss + shifter - stonecraft (mashup 19c) | 0.632 | 0.072 | 0.062 | yes |
| suksma - Hexcollie - Julian Carnival - shimmy dumb grid | 0.202 | 0.381 | 0.067 | yes |
| suksma - biotoxins on strings - thyghmaster oilstrip lifedecay | 0.233 | 0.235 | 0.000 | yes |
| suksma - biotoxins on strings - walk away + flx per vtx equ | 0.411 | 0.241 | 0.000 | yes |
| suksma - chemosynthetic nosferatu - grey budget | 0.068 | 0.162 | 0.002 | yes |
| suksma - ed geining hateops - flx granimal | 0.865 | 0.110 | 0.052 | yes |
| suksma - flexi coheres vacuum energy - brujchump | 0.535 | 0.446 | 0.016 | yes |
| suksma - pus on my cat - fun bags --- Isosceles edit | 0.675 | 0.431 | 0.036 | yes |
| suksma - yin - 395 - Artificial Inspiration (brainstorming - outward) + martin + flexi + stahlregen - el | 0.119 | 0.303 | 0.000 | yes |
| suksma - yin - 395 - Artificial Inspiration (brainstorming - outward) + martin + flexi + stahlregen - elephant mating - maggotron | 0.119 | 0.303 | 0.000 | yes |
| various - whirygig03 - flx rigidigeridoolittle' - 2 chokmah wisdom grey zodiac neptune twos | 0.393 | 0.337 | 0.000 | yes |
| xtramartin (83) | 0.788 | 0.129 | 0.048 | yes |
| yin - 390 - Artificial Inspiration | 0.076 | 0.158 | 0.036 | yes |
| yin - 391 - Artificial Inspiration (outward spectrum) | 0.084 | 0.169 | 0.042 | yes |

## Renders flat (12 presets)

A single flat tone with almost no detail.

| Preset | Mean | Contrast | Saturation | Preview |
|---|---|---|---|---|
| BrainStain- boiling-mix2(redi jedi full carb mix) | 0.343 | 0.005 | 0.158 | yes |
| BrainStain-Blackwidow | 0.028 | 0.005 | 0.139 | yes |
| EVET - Scanazoic --- Isosceles edit | 0.139 | 0.019 | 0.646 | yes |
| Geiss - Explosion 3 nz+ | 0.130 | 0.006 | 1.000 | yes |
| Geiss - Mega Swirl 2 | 0.126 | 0.011 | 0.165 | no |
| ORB - Crysal Storm  | 0.338 | 0.017 | 1.000 | yes |
| Redi Jedi - pladman gets high at wal-mart | 0.302 | 0.010 | 1.000 | yes |
| Rovastar & Unchained - Ambrosia Mystic (Dark Heart Mix) | 0.080 | 0.017 | 0.563 | no |
| Rovastar + Unchained - Ambrosia Mystic (Dark Heart Mix) | 0.080 | 0.017 | 0.563 | no |
| Zylot - Magma Crawl | 0.557 | 0.008 | 0.797 | no |
| flexi + geiss - bipolar vs reaction diffusion [antirotating reflection]_Phat_SeizureBox | 0.782 | 0.001 | 0.764 | yes |
| flexi - grind my glitch up [342] | 0.825 | 0.012 | 1.000 | yes |

## Comparison with MilkDrop captures

The captures, which are not in this repository, are 512x512 captures from Milkwave 4.1 (a DirectX 9 port of MilkDrop 2) of 170 presets, taken twice. `tools/compare_frames.py` scores how far two images are from each other, 0 for identical. Presets use random numbers, so MilkDrop is not identical to itself: its two runs score a median of 0.24 against each other. Rendered with `MILK_BANDS=1`, to use Milkwave's audio analysis, our frames score a median of 0.24 against the first run. 130 of 170 are within 0.05 of MilkDrop's own repeatability, and 95 are closer to MilkDrop than it is to itself. Rendering for 5.0 s matches best: on the presets MilkDrop repeats well, the mean score is 0.140 at 5.0 s and 0.175 at 5.5 s.

These presets score more than 0.2 above their own repeat-run score, so MilkDrop produces much the same frame each time and ours differs. Several depend on audio levels, which the sweep and Milkwave's analysis make hard to match, or on `rand_preset`.

| Preset | MilkDrop vs itself | Ours vs MilkDrop |
|---|---|---|
| Jc - Snowflake | 0.09 | 0.59 |
| Flexi - against all ods | 0.00 | 0.49 |
| phat + EoS - Bass_responce_Red_Movements_Disorienting nebula3 aaa | 0.18 | 0.59 |
| $$$ Royal - Mashup (96) | 0.23 | 0.61 |
| Fumbling_Foo + En D & Martin - Mandelverse 31 | 0.27 | 0.65 |
| high_frequency__rationalizing insanity using only mainstream religion nz+ barf bowl | 0.33 | 0.71 |
| fiShbRaiN - betelguese (hyperdrive remix) - as blue as our new second sun | 0.16 | 0.49 |
| high_frequency__Flexi - self similarity - i love flexi a lot (apple IIe crt whores) nz | 0.16 | 0.49 |
| flexi + geiss - bipolar vs reaction diffusion [antirotating reflection]_Phat_SeizureBox | 0.27 | 0.59 |
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
