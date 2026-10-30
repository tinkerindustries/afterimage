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

## Renders near black (73 presets)

Mostly black after 5 seconds.

| Preset | Mean | Contrast | Saturation | Preview |
|---|---|---|---|---|
| $$$ Royal - Mashup (402) | 0.005 | 0.025 | 0.225 | yes |
| $$$ Royal - Mashup (489) | 0.004 | 0.017 | 0.238 | yes |
| $$$ Royal - Mashup (492) | 0.001 | 0.020 | 0.004 | yes |
| 101 | 0.008 | 0.036 | 0.071 | yes |
| 235 | 0.014 | 0.051 | 0.264 | yes |
| 236 | 0.014 | 0.051 | 0.264 | yes |
| Adam Eatit Mashup FX 2 martin - disco mix + Lodus + Geiss + Ludicrous speed + Baked Ft another AdamFX M9 | 0.016 | 0.049 | 0.759 | yes |
| BrainStain-Blackwidow | 0.016 | 0.005 | 0.296 | yes |
| BrainStain-re entry | 0.017 | 0.061 | 0.312 | no |
| EoS+Phat - spectrum bubble new colors - orb sth go 4-ret brothers organs | 0.000 | 0.000 | 0.000 | yes |
| Flexi + bdrv - ultramix #46 test [all your base] | 0.000 | 0.000 | 0.000 | yes |
| Flexi - spatialist | 0.000 | 0.000 | 0.000 | yes |
| Fumbling_Foo + En D & Martin - Mandelverse 31 | 0.000 | 0.000 | 0.000 | yes |
| Fumbling_Foo - Star Forge v7 (HAKAN mash-up) [[pixel theme]] 0-8 v1 | 0.010 | 0.042 | 0.018 | yes |
| Geiss - 3 layers (Minefield Mix) | 0.019 | 0.076 | 0.219 | no |
| Geiss - Dancing Spirits (Bright) | 0.010 | 0.038 | 0.067 | yes |
| Geiss - Flexi - Redi Jedi - Shifter - Phat - Rovastar - 120287110113(457) - orb sth go 4-ret cataclysmic sound | 0.000 | 0.000 | 0.000 | yes |
| Geiss - Hyperion (LSB mix) | 0.019 | 0.058 | 0.388 | no |
| Geiss - Iris | 0.011 | 0.017 | 0.052 | no |
| Goody - Sonic Infection | 0.000 | 0.000 | 0.000 | no |
| LuX - Digital LifeWheel I Remix3d | 0.018 | 0.064 | 0.176 | yes |
| LuX - Digital LifeWheel Shifted A | 0.018 | 0.065 | 0.193 | yes |
| LuxXx - FearThisStorm | 0.014 | 0.075 | 0.087 | yes |
| Martin - Pixies Party (Hakan mash-up) 2-3 | 0.016 | 0.070 | 0.153 | yes |
| Martin - wire dance | 0.004 | 0.021 | 0.083 | no |
| PHAT HD FX 2 FiShbRaiN  -  Flexi  ft AdamFX Aderrasi , Martin n Rova - Witchcraft Take OverFX  | 0.008 | 0.049 | 0.029 | yes |
| RLP + Geiss - Bass Zoom Redux (3 Layers Minefield Bass Reaction) | 0.003 | 0.017 | 0.145 | yes |
| TonyMilkdrop - Angels Of Glory [Flexi - to get tasered and to like it + techstyle] --- Isosceles edit | 0.011 | 0.024 | 0.056 | yes |
| TonyMilkdrop - Tricolors [Flexi - away with it + $this shall not retain] --- Isosceled edit | 0.015 | 0.107 | 0.015 | yes |
| Tripgnosis - Firesticks | 0.018 | 0.050 | 0.908 | yes |
| _Mig_025 | 0.008 | 0.059 | 0.095 | no |
| _Mig_120 | 0.003 | 0.017 | 0.000 | no |
| _Mig_197b | 0.014 | 0.039 | 0.181 | no |
| amandio c - iterative - squares 4 | 0.015 | 0.109 | 0.009 | yes |
| bowel trounce dumring | 0.002 | 0.023 | 0.013 | yes |
| cope - soar (v2.0) | 0.001 | 0.004 | 0.087 | no |
| fiShbRaiN - witchcraft (ritual dance remix) | 0.005 | 0.039 | 0.076 | yes |
| goddamn nice - umbradg | 0.002 | 0.022 | 0.035 | yes |
| high_frequency/178 | 0.000 | 0.000 | 0.000 | yes |
| high_frequency/Flexi - self similarity - i love flexi a lot (apple IIe crt whores) nz | 0.008 | 0.083 | 0.003 | yes |
| high_frequency/Geiss - Flexi - Redi Jedi - Shifter - Phat - Rovastar - 120287110113(457) - orb sth go 4-ret cataclysmi | 0.000 | 0.000 | 0.000 | yes |
| high_frequency/Pithlit - Under Pressure - salt bag collection 1 vs sbc2 | 0.019 | 0.017 | 0.780 | yes |
| high_frequency/amandio c - green machinery 1a hell if i could nz+2 why not, right3 royal weddings are for satanists | 0.003 | 0.030 | 0.019 | yes |
| high_frequency/amandio c - tendrils reflected 1 'nacqtuelle | 0.000 | 0.003 | 0.030 | yes |
| high_frequency/shifter - spincycle(remix) - love at home - electroplasmic pissbowel | 0.001 | 0.004 | 0.031 | yes |
| high_frequency/shifter - spincycle(remix) - love at home - electroplasmic pissbowel roam3- | 0.000 | 0.000 | 0.000 | yes |
| high_frequency/suksma - Chemlock - Spot (Zylot & Aderrasi equations) martin stahlregen - dremp - meds | 0.003 | 0.020 | 0.063 | yes |
| high_frequency/suksma - pus on my cat - hate shade | 0.000 | 0.000 | 0.000 | yes |
| high_frequency/undulant --- Isosceles edit | 0.002 | 0.021 | 0.014 | yes |
| high_frequency/undulant 03 Oengyilkos Vasarnap | 0.005 | 0.018 | 0.234 | yes |
| if it's good enough for me to save, it's dead enough for you to re-m flx nz+ | 0.000 | 0.000 | 0.000 | yes |
| lemuel - the search for god is absurdit is if everyone dies alone | 0.011 | 0.055 | 0.199 | yes |
| martin - chain breaker | 0.019 | 0.023 | 0.889 | no |
| nearly unavoidable isolatory strategy flx roams | 0.000 | 0.000 | 0.000 | yes |
| shell robot --- Isosceles edit | 0.000 | 0.000 | 0.000 | yes |
| shell robot nz+ psiqolojiq | 0.015 | 0.031 | 0.472 | yes |
| shifter + flexi - american pie 5 nz+ m3 w3 | 0.015 | 0.012 | 0.992 | yes |
| shifter - fractal grinder (opalescent) | 0.000 | 0.001 | 0.048 | yes |
| shifter - liquid circuitry - suksma defilement | 0.010 | 0.016 | 0.570 | yes |
| shifter - snakeskin (edged) | 0.011 | 0.083 | 0.229 | yes |
| shifter - spincycle c | 0.015 | 0.020 | 0.701 | no |
| suksma - DemonLD_-_Future_domination - mash0000 - sclera torture | 0.018 | 0.071 | 0.090 | yes |
| suksma - autoerotic solipsistic omniversal meme - convoluted debt saturation | 0.012 | 0.050 | 0.164 | yes |
| suksma - biotoxins on strings - busy highway ignores | 0.003 | 0.053 | 0.000 | yes |
| suksma - dotes hostile undertake - antiborg | 0.000 | 0.000 | 0.000 | yes |
| suksma - ed geining hateops - flx DddDddDddDdd die | 0.007 | 0.028 | 0.157 | yes |
| suksma - ed geining hateops - flx beagle | 0.006 | 0.042 | 0.115 | yes |
| suksma - ed geining hateops - flx hate shade | 0.002 | 0.018 | 0.129 | yes |
| suksma - ed geining hateops - flx palette massacrow | 0.003 | 0.001 | 0.931 | yes |
| suksma - ed geining hateops - flx sg5 ink stink you dink | 0.011 | 0.044 | 0.168 | yes |
| suksma - ichor souse amiunable canthode | 0.019 | 0.063 | 0.577 | yes |
| suksma - shifter - spincycle + labor of others | 0.014 | 0.006 | 0.608 | yes |
| va ultramix - 154 [chaos] | 0.010 | 0.013 | 0.000 | yes |

## Renders white (44 presets)

Nearly all white after 5 seconds.

| Preset | Mean | Contrast | Saturation | Preview |
|---|---|---|---|---|
| $$$ Royal - Mashup (383) | 1.000 | 0.000 | 0.000 | yes |
| 153 | 0.999 | 0.018 | 0.001 | yes |
| Cope - Passage (mandala mix) | 0.999 | 0.023 | 0.002 | no |
| EVET - Magnetic Blizzard | 0.965 | 0.093 | 0.059 | yes |
| Hexcollie, Aderassi, ORB, baked n 3dragons - Moving target | 0.987 | 0.083 | 0.009 | yes |
| LuxXx - The Feeder | 1.000 | 0.002 | 0.000 | yes |
| LuxXx - The Feeder II | 1.000 | 0.002 | 0.000 | yes |
| LuxXx - Token Beats | 1.000 | 0.004 | 0.000 | yes |
| Martin N AdamFX Infusion = Phat+Yin+EoS_Mandala Chaser Ft AdamFX n Martin - The Beast Mandala Chaser FX H | 0.989 | 0.066 | 0.014 | yes |
| ORB - Inferno | 0.975 | 0.066 | 0.121 | no |
| ORB - Supernova Meltdown | 1.000 | 0.000 | 0.000 | no |
| Rovastar + Loadus + Geiss - FractalDrop (Triple Mix) | 0.963 | 0.155 | 0.011 | no |
| Rovastar + Loadus + Geiss - Tone-mapped FractalDrop 7c | 0.974 | 0.078 | 0.007 | no |
| Rovastar + Loadus - FractalDrop (Active Sparks Mix) Steady | 0.999 | 0.004 | 0.008 | no |
| Serge + 158 001 | 0.973 | 0.084 | 0.118 | yes |
| Serge + martin - soma | 0.996 | 0.030 | 0.004 | yes |
| Serge circles009 | 0.967 | 0.091 | 0.068 | yes |
| Syst3mFailur - satanic ring_Phat+EoS_DIEty_FREEquency_Remix | 0.990 | 0.018 | 0.037 | yes |
| The NG + Stahlregen & Boz + EoS + Geiss + Phat + Rovastar + Zylot + Flexi + martin + Fishbrain - Machine Code (In The Wind Tunnel) | 0.993 | 0.052 | 0.008 | yes |
| Waltra - Control Freak | 1.000 | 0.001 | 0.001 | yes |
| Waltra - Emitter | 1.000 | 0.000 | 0.000 | yes |
| Waltra - Phantom Spectrum 2 | 1.000 | 0.001 | 0.001 | yes |
| Waltra - Phantom UltraMATTER | 0.995 | 0.021 | 0.014 | yes |
| Zylot - Color Of Music | 1.000 | 0.000 | 0.000 | no |
| Zylot - Crossing Over (Geiss Tweak) | 0.997 | 0.033 | 0.005 | no |
| bdrv + al shifter - phasm mx | 1.000 | 0.000 | 0.000 | yes |
| custom1 | 0.982 | 0.018 | 0.150 | yes |
| fiShbRaiN - betelguese (hyperdrive remix) - hebba | 1.000 | 0.000 | 0.000 | yes |
| fiShbRaiN - betelguese3 aa | 1.000 | 0.002 | 0.000 | yes |
| fleekus flokus - violent and insane homicidal air molecules2 - shf fab we are the info flow, not | 1.000 | 0.012 | 0.001 | yes |
| hey horse teeth girl | 1.000 | 0.000 | 0.000 | yes |
| high_frequency/BEST OF ADAMFX 2 Flexi + Geiss - Bipolar vs reaction diffusion + Deamon Lord + EOS 23 | 0.992 | 0.040 | 0.010 | yes |
| high_frequency/fleekus flokus - violent and insane homicidal air molecules2 - shf fab we are the info flow, not the structure of space time | 1.000 | 0.012 | 0.001 | yes |
| high_frequency/rationalizing insanity using only mainstream religion nz+ barf bowl | 0.965 | 0.046 | 0.195 | yes |
| high_frequency/shifter - phasm wraith (fps+ mix) | 1.000 | 0.000 | 0.000 | yes |
| high_frequency/suksma - m-martin w-geiss wp-m c-m - opulaesqueuants pomcromise | 0.971 | 0.090 | 0.042 | yes |
| martin - starfield sectors | 0.997 | 0.027 | 0.003 | yes |
| martin - starfield sectors (Ati fix) | 0.997 | 0.027 | 0.003 | no |
| shifter + flexi - liquid circuitry from the fractalism hive - orb tg lampart revision overdue | 0.992 | 0.023 | 0.013 | yes |
| suksma - diploautomatic autoimmunity vs passivematic aggpressive - crab ride | 0.985 | 0.049 | 0.040 | yes |
| suksma - ed geining hateops - flx shibbalibbadi | 0.965 | 0.088 | 0.029 | yes |
| suksma - newspaper | 0.978 | 0.104 | 0.041 | yes |
| xtramartin (844) | 0.997 | 0.027 | 0.003 | yes |
| yin - 102 - Through the ether (The separation) - Bitcore Tweak | 0.972 | 0.033 | 0.050 | yes |

## Renders grey (91 presets)

Almost no colour.

| Preset | Mean | Contrast | Saturation | Preview |
|---|---|---|---|---|
| $$$ Royal - Mashup (191) | 0.090 | 0.149 | 0.000 | yes |
| A HD ADAMFX Creation  !!FT  Martin + flexi +  Rovastar MDropFX Collaboration E Adam HD INFECTIONFX A | 0.932 | 0.205 | 0.004 | yes |
| A MilkKing Recreation FT Martin  - Violet Flash ft AdamFX n Geiss - Spiral Tunnels in FX  | 0.755 | 0.315 | 0.030 | yes |
| Aderrasi - Ghast (Full Circle Mix) | 0.078 | 0.230 | 0.014 | no |
| Aderrasi - Ghast (Quarter Past Ghast Mix) | 0.097 | 0.171 | 0.069 | no |
| Bdrv et AL Aderrasi - Accelerator (Orbital Migraine) bdvr xxx2_1 - mrt sth | 0.158 | 0.267 | 0.000 | yes |
| Cope-WIP - Strange Attractor 2 | 0.758 | 0.264 | 0.000 | no |
| EVET + Flexi -Hue Burster | 0.927 | 0.235 | 0.035 | yes |
| EVET - Approaching Singularity | 0.816 | 0.180 | 0.072 | yes |
| EVET- Esotic & Rozzer - Slazer Eyes | 0.940 | 0.087 | 0.038 | yes |
| Fed + Geiss - Cauldron painterly 5 - over dots --- Isosceles edit1 | 0.023 | 0.130 | 0.067 | yes |
| Flexi + Rovastar + suksma - Fractopia [sth] | 0.150 | 0.233 | 0.000 | yes |
| Flexi - disruptor [vision of eggs] | 0.419 | 0.448 | 0.012 | yes |
| Flexi - operation acid storm [featuring geiss' rayleigh scattering finish] | 0.048 | 0.073 | 0.000 | yes |
| Geiss - Cosmic Dust 2 - quasistatic noise dual pane window | 0.195 | 0.161 | 0.053 | no |
| Geiss - Digitunnel | 0.068 | 0.150 | 0.076 | no |
| Geiss - Eggs | 0.104 | 0.186 | 0.058 | no |
| Geiss - Game of Life | 0.129 | 0.240 | 0.000 | no |
| Geiss - Hurricane | 0.074 | 0.233 | 0.003 | no |
| Geiss - Kaleidoscope 1 | 0.051 | 0.161 | 0.065 | no |
| Geiss - Pistons | 0.127 | 0.240 | 0.020 | no |
| Geiss - Rose 3 | 0.641 | 0.191 | 0.078 | no |
| Geiss - Rose 4 | 0.083 | 0.184 | 0.044 | no |
| Geiss - Skin Dots Multi-layer 3 | 0.532 | 0.357 | 0.000 | no |
| Geiss - Swirlie 4 | 0.402 | 0.162 | 0.032 | no |
| Geiss - Thumb Drum | 0.424 | 0.165 | 0.000 | no |
| Goody - Lights in the Sky | 0.938 | 0.196 | 0.069 | yes |
| Goody - Unstable Sonic Reactor - Final | 0.824 | 0.292 | 0.067 | no |
| Jc - Snowflake | 0.085 | 0.216 | 0.038 | yes |
| Martin - Diabolo | 0.121 | 0.087 | 0.077 | no |
| Martin N AdamFX Infusion = Phat+Yin+EoS_Mandala Chaser Ft AdamFX n Martin - The Beast Mandala Chaser FX J | 0.933 | 0.152 | 0.059 | yes |
| Milk Artist At our Best - FED - SlowFast Ft AdamFX n Martin - HD CosmoFX | 0.749 | 0.004 | 0.000 | yes |
| ORB - Stahl - Glass Ocean | 0.470 | 0.009 | 0.025 | yes |
| Rovastar - Fractopia (Upspoken Mix) | 0.765 | 0.387 | 0.013 | no |
| Serge + 158 002 | 0.904 | 0.213 | 0.009 | yes |
| Serge + martin - crystal palace006 | 0.419 | 0.356 | 0.032 | yes |
| Serge + martin - crystal palace007d | 0.076 | 0.158 | 0.070 | yes |
| Serge mashup003 | 0.904 | 0.212 | 0.068 | yes |
| Stahlregen & Aderrasi + bdrv + EoS + flexi + Geiss + krash + shifter + TEcHNO - Liquid Star (RMX)_1 | 0.028 | 0.023 | 0.000 | yes |
| Stahlregen & Boz - Machine Code (Reaction Diffusion)_1 - Isosceles Tweak 06 | 0.469 | 0.162 | 0.000 | yes |
| Stahlregen & Boz - Machine Code (Reaction Diffusion)_1 - Isosceles Tweak 08 | 0.528 | 0.216 | 0.000 | yes |
| Stahlregen & Boz - Machine Code (Reaction Diffusion)_1 - Isosceles Tweak 10 | 0.431 | 0.161 | 0.000 | yes |
| Stahlregen - Old School, Baby! (Black and White Mash) | 0.474 | 0.378 | 0.000 | yes |
| TobiasWolfBoi - The Pit | 0.210 | 0.181 | 0.031 | no |
| TonyMilkdrop - Angels Of Glory [Flexi - an excuse taken back + techstyle] --- Isosceles edit | 0.240 | 0.344 | 0.059 | yes |
| TonyMilkdrop - Angels Of Glory [Flexi - arrogance amplifier + techstyle] --- Isosceles edit | 0.611 | 0.431 | 0.003 | yes |
| Tripgnosis - Antiphotonic Dual | 0.195 | 0.376 | 0.024 | yes |
| Waltra - Cosmic Wind | 0.885 | 0.220 | 0.055 | yes |
| _Aderrasi - Wanderer in Curved Space - mash0000 - faclempt kibitzing meshuggana schmaltz (Geiss greyscale mix) | 0.131 | 0.169 | 0.000 | no |
| _Geiss - Confetti (Kaleidoscope Mix) | 0.064 | 0.223 | 0.000 | no |
| _Mig_146 | 0.421 | 0.061 | 0.000 | no |
| _____________Geiss - Myriad Spirals - cruise mix | 0.480 | 0.099 | 0.064 | no |
| amandio c - hat track - mrt sth | 0.302 | 0.348 | 0.000 | yes |
| amandio c - hat track - mrt sth3 | 0.679 | 0.354 | 0.000 | yes |
| amandio c - iterative - squares 3 | 0.020 | 0.102 | 0.026 | yes |
| amandio c - log tower 1 pig vomit 2uqle4 dorqstre l-egrunth | 0.031 | 0.122 | 0.068 | yes |
| amandio c - pulse - sth | 0.655 | 0.373 | 0.000 | yes |
| amandio c - typhoon season (inside) - sth | 0.596 | 0.396 | 0.000 | yes |
| blood red raw holy - sth absolutely blazed lying naked hidden in the savannah | 0.453 | 0.410 | 0.000 | yes |
| carved in skin nz+ | 0.753 | 0.273 | 0.064 | yes |
| cope - mandelbrot (32 iterations) - mrt mangler | 0.027 | 0.134 | 0.005 | yes |
| cope - mandelbrot (32 iterations) - mrt mangler nz | 0.023 | 0.104 | 0.040 | yes |
| eschatonal procrastination kery | 0.026 | 0.085 | 0.071 | yes |
| fiShbRaiN + Flexi - witchcraft unleashed [00 the template] + shifter - turing tape - stochastic difference engine | 0.183 | 0.075 | 0.026 | yes |
| fiShbRaiN + Flexi - witchcraft unleashed [00 the template] + shifter - turing tape - zit gist | 0.139 | 0.063 | 0.001 | yes |
| fiShbRaiN - betelguese (hyperdrive remix) - as blue as our new second sun | 0.374 | 0.150 | 0.000 | yes |
| fiShbRaiN - the adventures of prismo jenkins | 0.908 | 0.077 | 0.054 | yes |
| fishbrain and flexi - box of tricks [hexcraft 09 - polar] - mrt sth | 0.319 | 0.319 | 0.000 | yes |
| flexi - blame moebius | 0.424 | 0.493 | 0.000 | no |
| flexi - lorenz attractor | 0.028 | 0.101 | 0.051 | no |
| hexcollie - Black Hole | 0.523 | 0.330 | 0.060 | yes |
| high_frequency/170 | 0.167 | 0.287 | 0.056 | yes |
| high_frequency/BEST OF ADAMFX 2 Flexi + Geiss - Bipolar vs reaction diffusion + Aderassi Phat Zylot4 --- Isosceles edit | 0.295 | 0.305 | 0.071 | yes |
| high_frequency/Halfbreak + Flexi and goody + geiss + benski - Sparkly LSB Feedback' | 0.610 | 0.432 | 0.063 | yes |
| high_frequency/blood red raw holy - btking the taboo | 0.855 | 0.302 | 0.000 | yes |
| high_frequency/shifter - smoked glass - i can't afford to mash this | 0.356 | 0.454 | 0.000 | yes |
| martin - gate to moria | 0.367 | 0.213 | 0.007 | no |
| martin - move this body | 0.152 | 0.202 | 0.065 | no |
| shifter - phasm wraith - yi qing short term long term dance | 0.278 | 0.192 | 0.000 | yes |
| stahlregen & fishbrain + flexi + geiss + shifter - stonecraft (mashup 19c) | 0.633 | 0.075 | 0.072 | yes |
| suksma - biotoxins on strings - thyghmaster oilstrip lifedecay | 0.249 | 0.249 | 0.000 | yes |
| suksma - biotoxins on strings - walk away + flx per vtx equ | 0.306 | 0.263 | 0.000 | yes |
| suksma - chemosynthetic nosferatu - grey budget | 0.073 | 0.149 | 0.002 | yes |
| suksma - ed geining hateops - flx granimal | 0.858 | 0.109 | 0.053 | yes |
| suksma - flexi coheres vacuum energy - brujchump | 0.913 | 0.257 | 0.002 | yes |
| suksma - grainery clock mortis | 0.938 | 0.114 | 0.040 | yes |
| suksma - pus on my cat - fun bags --- Isosceles edit | 0.597 | 0.444 | 0.054 | yes |
| suksma - yin - 395 - Artificial Inspiration (brainstorming - outward) + martin + flexi + stahlregen - el | 0.112 | 0.290 | 0.004 | yes |
| suksma - yin - 395 - Artificial Inspiration (brainstorming - outward) + martin + flexi + stahlregen - elephant mating - maggotron | 0.112 | 0.290 | 0.004 | yes |
| various - whirygig03 - flx rigidigeridoolittle' - 2 chokmah wisdom grey zodiac neptune twos | 0.401 | 0.338 | 0.000 | yes |
| xtramartin (83) | 0.815 | 0.108 | 0.041 | yes |

## Renders flat (7 presets)

A single flat tone with almost no detail.

| Preset | Mean | Contrast | Saturation | Preview |
|---|---|---|---|---|
| BrainStain- boiling-mix2(redi jedi full carb mix) | 0.254 | 0.006 | 0.272 | yes |
| EVET - Scanazoic --- Isosceles edit | 0.085 | 0.019 | 0.615 | yes |
| EVET - SmoulderingScape --- Isosceles edit | 0.034 | 0.015 | 0.997 | yes |
| Redi Jedi - pladman gets high at wal-mart | 0.301 | 0.007 | 1.000 | yes |
| flexi + geiss - bipolar vs reaction diffusion [antirotating reflection]_Phat_SeizureBox | 0.782 | 0.001 | 0.764 | yes |
| flexi - grind my glitch up [342] | 0.856 | 0.012 | 1.000 | yes |
| pair up gnothing knew nz3 | 0.695 | 0.018 | 0.394 | yes |

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
