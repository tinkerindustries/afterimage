# Remaining presets to fix

This list is a snapshot of one scan of the preset collection. It goes stale as fixes land, so regenerate it rather than editing it by hand:

- `build/milk_shader_check presets -v` lists shaders that fail to translate or compile.
- `build/milk_preset_check presets -v` lists equation code that fails to compile.
- `find presets -name '*.milk' -print0 | sort -z | xargs -0 build/milk_render - 5 256` prints each preset's mean brightness, contrast and saturation after 5 seconds of synthesised audio, starting from noise.

The render classes use these thresholds on that output: near black is mean brightness under 0.02, white is over 0.95, grey is saturation under 0.08, and flat is contrast under 0.02. They flag frames worth a look. Some grey presets are monochrome in MilkDrop too, so check the preview (the `.jpg` beside the `.milk`) before treating one as broken. `build/milk_render sheet.png 5 384 <preset.milk>...` puts previews and renders side by side.

Paths are relative to `presets/favorite_presets_2021_01_03/`. A preset can appear in more than one section.

## Shaders that fail to translate (60 presets)

The preset still runs, without the failing stage. HLSL errors come from glslang, MSL errors from the Metal compiler.

| Preset | Stage | Step | First error |
|---|---|---|---|
| accruing simon genitoll boothf | warp | HLSL | 'assignment expression' : Expected |
| Benjam and Zylot - Tie-Dye Supernova - prophecies - salad phalanx taps | warp | HLSL | 'assignment expression' : Expected |
| Cope - Cartune (extrusion machine) [fixed] | warp | HLSL | '' : Could not perform requested binary operation |
| Cope - Cartune (extrusion machine) [fixed] (Geiss breathing mix) | warp | HLSL | '' : Could not perform requested binary operation |
| cope - ferrofluid | warp | HLSL | '' : Could not perform requested binary operation |
| cope - ferrofluid [Flexis minor remix] | warp | HLSL | '' : Could not perform requested binary operation |
| dumpling antler melon sage - her dog be everywon | warp | HLSL | '' : could not create assignment |
| EoS+Phat - spectrum bubble new colors - orb sth go 4-ret brothers organs | comp | HLSL | 'constructor' :  cannot convert parameter 6 from ' const float' to ' temp 4-component vector of float' |
| fed - slowfast 2-0 | comp | MSL | expected unqualified-id |
| fed - slowfast 2-0 [acid tweak] | comp | MSL | expected unqualified-id |
| fiShbRaiN - betelguese (hyperdrive remix) - as blue as our new second sun | warp | HLSL | 'assignment expression' : Expected |
| fishbrain and flexi - box of tricks 15 polar borders [logarithmic inverse] - flx mgr suksma is meta pata border --- Isosceles edit | warp | HLSL | '' : could not create assignment |
| Flexi - crush ice 62 | warp | HLSL | '' : could not create assignment |
| Flexi - operation acid storm [featuring geiss' rayleigh scattering finish] | warp | HLSL | '' : could not create assignment |
| Flexi - truly soft piece of software - topology - cohere | warp | HLSL | '' : could not create assignment |
| Fumbling_Foo & Flexi, Martin, Orb, Unchained - Acid Mandala v3 | comp | HLSL | 'assign' :  l-value required "$Global" (can't modify a uniform) |
| Fumbling_Foo & Flexi, Martin, Orb, Unchained - Acid Mandala v3a | comp | HLSL | 'assign' :  l-value required "$Global" (can't modify a uniform) |
| Fumbling_Foo & Flexi, Martin, Orb, Unchained - Acid Mandala v3b | comp | HLSL | 'assign' :  l-value required "$Global" (can't modify a uniform) |
| Fumbling_Foo & Flexi, Martin, Orb, Unchained - Acid Mandala v3e | comp | HLSL | 'assign' :  l-value required "$Global" (can't modify a uniform) |
| Fumbling_Foo & Flexi, Martin, Orb, Unchained - Acid Mandala v4 | comp | HLSL | 'assign' :  l-value required "$Global" (can't modify a uniform) |
| Fumbling_Foo & Flexi, Martin, Orb, Unchained - Acid Mandala v4b | comp | HLSL | 'assign' :  l-value required "$Global" (can't modify a uniform) |
| Fumbling_Foo & Flexi, Martin, Orb, Unchained - Acid Mandala v4d | comp | HLSL | 'assign' :  l-value required "$Global" (can't modify a uniform) |
| Fumbling_Foo & Flexi, Martin, Orb, Unchained - Acid Mandala v5 | comp | HLSL | 'assign' :  l-value required "$Global" (can't modify a uniform) |
| Fumbling_Foo & Flexi, Martin, Orb, Unchained - Acid Mandala v5a | comp | HLSL | 'assign' :  l-value required "$Global" (can't modify a uniform) |
| Geiss - Flexi - Redi Jedi - Shifter - Phat - Rovastar - 120287110113(457) - orb sth go 4-ret cataclysmic sound | comp | HLSL | 'constructor' :  cannot convert parameter 6 from ' const float' to ' temp 4-component vector of float' |
| Goody + martin - crystal palace - Aqua Lumens | warp | HLSL | 'normalize' : ambiguous best function under implicit type conversion |
| Goody + martin - crystal palace - Aqua Lumens5 | warp | HLSL | 'normalize' : ambiguous best function under implicit type conversion |
| goody + martin - crystal palace - schizotoxin - the wild iris bloom - 16 iterations | warp | HLSL | 'normalize' : ambiguous best function under implicit type conversion |
| GreatWho + Flexi - Lasershow [bipolar striking party] | warp | HLSL | '' : could not create assignment |
| high_frequency/Bdrv Aderrasi - Accelerator (Orbital Migraine) BDRV et  AL  rmx8 - amoral love meets phreq missionary | warp | HLSL | 'assignment expression' : Expected |
| high_frequency/Bdrv Aderrasi - Accelerator (Orbital Migraine) BDRV et  AL  rmx8 - loavthe is a con-tact spwhore-T(honTh | warp | HLSL | 'assignment expression' : Expected |
| high_frequency/blood red raw holy - i am an insulted prude and a concerned parent and a disappearing goat | comp | HLSL | 'constructor' :  cannot convert parameter 6 from ' const float' to ' temp 4-component vector of float' |
| high_frequency/extreme/Bdrv Aderrasi - Accelerator (Orbital Migraine) BDRV et  AL  rmx8 - amoral love meets phreq missionary | warp | HLSL | 'assignment expression' : Expected |
| high_frequency/extreme/Bdrv Aderrasi - Accelerator (Orbital Migraine) BDRV et  AL  rmx8 - amoral love meets phreq missionary nz | warp | HLSL | 'assignment expression' : Expected |
| high_frequency/fed - slowfast 2-11 [Killa] | comp | MSL | expected unqualified-id |
| high_frequency/Geiss - Flexi - Redi Jedi - Shifter - Phat - Rovastar - 120287110113(457) - orb sth go 4-ret cataclysmi | comp | HLSL | 'constructor' :  cannot convert parameter 6 from ' const float' to ' temp 4-component vector of float' |
| high_frequency/suksma - chernobyl pie for dessert - more lateral shade of masueality | comp | HLSL | 'sampler/image' : cannot construct this type |
| high_frequency/suksma - conflating neediness with eunucharistocracy | comp | HLSL | 'sampler/image' : cannot construct this type |
| high_frequency/suksma - yaqui mirage master | comp | HLSL | 'sampler/image' : cannot construct this type |
| i am broken, so throw me away | warp | HLSL | '' : could not create assignment |
| martin + flexi - lock and release bipolar 03 | comp | HLSL | 'lavcol' : no matching overloaded function found |
| martin - lock and release | comp | HLSL | 'lavcol' : no matching overloaded function found |
| martin - prismo jenkins last expedition | comp | HLSL | 'assign' :  l-value required "$Global" (can't modify a uniform) |
| martin - stratospheric turbulences | warp | HLSL | 'assign' :  l-value required "$Global" (can't modify a uniform) |
| martin - stratospheric turbulences | comp | HLSL | 'tria' : no matching overloaded function found |
| martin - Zen walk | comp | HLSL | 'assign' :  l-value required "$Global" (can't modify a uniform) |
| ORB - Quicksand Lab | comp | HLSL | 'constructor' :  cannot convert parameter 6 from ' const float' to ' temp 4-component vector of float' |
| ORB - Sandblade | warp | HLSL | 'constructor' :  cannot convert parameter 5 from ' const float' to ' temp 4-component vector of float' |
| ORB - Stahl - Fire Worm | comp | HLSL | 'constructor' :  cannot convert parameter 6 from ' const float' to ' temp 4-component vector of float' |
| ORB - Stahl - Glass Ocean | comp | HLSL | 'constructor' :  cannot convert parameter 6 from ' const float' to ' temp 4-component vector of float' |
| pair up gnothing knew nz3 | comp | HLSL | 'constructor' :  cannot convert parameter 6 from ' const float' to ' temp 4-component vector of float' |
| Phat_EoS - our own personal demon - bdr_ Ireallydon'tknowwhatIwant - flx mgr turbine lurk nz+ | warp | HLSL | '' : could not create assignment |
| Phmall - Whipped (y) - flx mgr siamese declination scotsdale --- Isosceles edit | warp | HLSL | '' : could not create assignment |
| suksma - coal drapes - mrt fsh gordo dog crap in my bed cuz i sleep wid my shoes on | warp | HLSL | '' : could not create assignment |
| suksma - melting the boring | warp | HLSL | '' : could not create assignment |
| suksma - n4 | warp | HLSL | 'assignment expression' : Expected |
| suksma - satanic teleprompter - john dee enochian alien summons | warp | HLSL | 'assignment expression' : Expected |
| TonyMilkdrop - Angels Of Glory [Flexi - baby blue + $this shall not retain] --- Isosceles edit | warp | HLSL | '' : could not create assignment |
| TonyMilkdrop - Tricolors [Flexi - feedback accomplishment + $this shall not retain] --- Isosceles edit | warp | HLSL | '' : could not create assignment |
| Tripgnosis - Unstable Nuclei - flx mgr sombulesce courtchieve | warp | HLSL | '' : could not create assignment |
| Various artists - Goo - flx mgr gunslinging gosling bear --- Isosceles edit | warp | HLSL | '' : could not create assignment |

## Equation code that fails to compile (13 presets)

The compiler drops the statements it cannot parse and runs the rest. The causes seen so far are a missing `;` at the end of a line, and an identifier split across two `per_frame` lines (`is_` then `beat`).

- AdamFX Enterz Tha Mash With Martin + stahlregen - AdamFX - In Tha Mash Effectz A
- high_frequency/AdamFX Enterz Tha Mash With Martin + stahlregen - AdamFX - In Tha Mash Effectz nz splatter bare
- high_frequency/suksma - Hexcollie - Julian Carnival - shimmy dumb grid dogmaklyasm nz+5
- high_frequency/suksma - Hexcollie - Julian Carnival - shimmy dumb grid dogmaklyasm nz+6
- high_frequency/there is gum stuck to my hate
- LuxXx - A Sober Life (wasted)
- LuxXx - electric cell park b
- martin + stahlregen - martin in da mash 12a
- martin + stahlregen - martin in da mash 12b
- martin - disco mix 1
- martin - the forge of Isengard
- martin [shadow harlequins shape code] - fata morgana
- Milk Artist At our Best - FED - SlowFast Ft AdamFX n Martin - HD CosmoFX Also Ft Mstress n Armandio  - Thought About YouFX

## Renders near black (95 presets)

Mostly black after 5 seconds.

| Preset | Mean | Contrast | Saturation | Preview | Errors |
|---|---|---|---|---|---|
| $$$ Royal - Mashup (402) | 0.003 | 0.020 | 0.180 | yes |  |
| $$$ Royal - Mashup (489) | 0.004 | 0.026 | 0.172 | yes |  |
| $$$ Royal - Mashup (492) | 0.001 | 0.016 | 0.011 | yes |  |
| 45 | 0.005 | 0.041 | 0.050 | yes |  |
| _Mig_025 | 0.006 | 0.051 | 0.092 | no |  |
| _Mig_120 | 0.003 | 0.014 | 0.010 | no |  |
| _Mig_197b | 0.013 | 0.058 | 0.185 | no |  |
| Aderrasi - Kevlar Abyss | 0.014 | 0.060 | 0.839 | no |  |
| amandio c - iterative - squares 3 | 0.017 | 0.091 | 0.028 | yes |  |
| amandio c - iterative - squares 4 | 0.014 | 0.102 | 0.010 | yes |  |
| bowel trounce dumring | 0.002 | 0.020 | 0.016 | yes |  |
| BrainStain-Blackwidow | 0.011 | 0.003 | 0.408 | yes |  |
| BrainStain-re entry | 0.012 | 0.047 | 0.327 | no |  |
| cope - mandelbrot (32 iterations) - mrt mangler | 0.010 | 0.078 | 0.005 | yes |  |
| cope - mandelbrot (32 iterations) - mrt mangler nz | 0.008 | 0.060 | 0.017 | yes |  |
| cope - soar (v2.0) | 0.000 | 0.000 | 0.000 | no |  |
| craving 'spinach man' action figures suppositoric nutrition - probing maternity burn mark fletcher nz+ more palettes turned ass ugly | 0.000 | 0.010 | 0.000 | yes |  |
| dylan - LED ravers addition (spectratex) | 0.014 | 0.029 | 0.399 | yes |  |
| Eo.S. + Geiss - glowsticks v2 03 music shifter edit b (Psychedelic Mix) | 0.007 | 0.057 | 0.181 | no |  |
| Eo.S. - glowsticks v2 03 music shifter edit b | 0.005 | 0.037 | 0.028 | no |  |
| Eo.S. - glowsticks v2 03 music shifter edit b (Jelly V2) | 0.013 | 0.018 | 0.934 | no |  |
| eschatonal procrastination kery | 0.012 | 0.060 | 0.038 | yes |  |
| fiShbRaiN - witchcraft (ritual dance remix) | 0.007 | 0.045 | 0.076 | yes |  |
| Flexi + bdrv - ultramix #46 test [all your base] | 0.000 | 0.000 | 0.000 | yes |  |
| Flexi - spatialist | 0.000 | 0.001 | 0.000 | yes |  |
| Fumbling_Foo + En D & Martin - Mandelverse | 0.002 | 0.021 | 0.013 | yes |  |
| Fumbling_Foo + En D & Martin - Mandelverse 31 | 0.000 | 0.000 | 0.000 | yes |  |
| Geiss - Mosaic | 0.013 | 0.088 | 0.080 | no |  |
| Geiss - Tokamak Plus Painterly | 0.005 | 0.019 | 0.023 | no |  |
| goddamn nice - umbradg | 0.002 | 0.028 | 0.010 | yes |  |
| Goody + martin - crystal palace - Aqua Lumens | 0.000 | 0.000 | 0.000 | yes | shader errors |
| Goody + martin - crystal palace - Aqua Lumens5 | 0.000 | 0.000 | 0.000 | yes | shader errors |
| goody + martin - crystal palace - schizotoxin - the wild iris bloom - 16 iterations | 0.000 | 0.000 | 0.012 | no | shader errors |
| Goody - Sonic Infection | 0.000 | 0.001 | 0.005 | no |  |
| high_frequency/170 | 0.000 | 0.006 | 0.004 | yes |  |
| high_frequency/178 | 0.000 | 0.000 | 0.000 | yes |  |
| high_frequency/amandio c - green machinery 1a hell if i could nz+2 why not, right3 royal weddings are for satanists | 0.019 | 0.077 | 0.114 | yes |  |
| high_frequency/amandio c - tendrils reflected 1 'nacqtuelle | 0.001 | 0.008 | 0.051 | yes |  |
| high_frequency/Flexi - self similarity - i love flexi a lot (apple IIe crt whores) nz | 0.000 | 0.018 | 0.000 | yes |  |
| high_frequency/Halfbreak - Timeless Mechanic | 0.011 | 0.022 | 0.908 | yes |  |
| high_frequency/Pithlit - Under Pressure - salt bag collection 1 vs sbc2 | 0.013 | 0.009 | 0.823 | yes |  |
| high_frequency/shifter - spincycle(remix) - love at home - electroplasmic pissbowel | 0.000 | 0.009 | 0.008 | yes |  |
| high_frequency/shifter - spincycle(remix) - love at home - electroplasmic pissbowel roam3- | 0.001 | 0.016 | 0.010 | yes |  |
| high_frequency/suksma - Chemlock - Spot (Zylot & Aderrasi equations) martin stahlregen - dremp - meds | 0.004 | 0.021 | 0.089 | yes |  |
| high_frequency/suksma - chemosynthetic nosferatu - automating transformation with b n s3 - claim form fsh shf flx nz+ | 0.015 | 0.074 | 0.070 | yes |  |
| high_frequency/suksma - chemosynthetic nosferatu - automating transformation with b n s3 - claim form fsh shf flx nz+2 | 0.015 | 0.074 | 0.070 | yes |  |
| high_frequency/suksma - everyone's opinion is infinitely differennt - fed shd | 0.002 | 0.024 | 0.107 | yes |  |
| high_frequency/undulant --- Isosceles edit | 0.003 | 0.022 | 0.018 | yes |  |
| if it's good enough for me to save, it's dead enough for you to re-m flx nz+ | 0.000 | 0.000 | 0.000 | yes |  |
| Krash + Rovastar - Rainbow Orb | 0.006 | 0.020 | 0.953 | no |  |
| martin - chain breaker | 0.019 | 0.019 | 0.895 | no |  |
| martin - city lights | 0.000 | 0.000 | 0.000 | yes |  |
| martin - city lights v2(1) | 0.000 | 0.000 | 0.000 | yes |  |
| martin - city of shadows | 0.000 | 0.000 | 0.000 | no |  |
| martin - frosty caves 2 | 0.000 | 0.000 | 0.000 | yes |  |
| martin - frosty caves_a | 0.000 | 0.000 | 0.000 | yes |  |
| Martin - Pixies Party (Hakan mash-up) 2-3 | 0.004 | 0.026 | 0.153 | yes |  |
| martin - prismo jenkins last expedition | 0.001 | 0.009 | 0.016 | no | shader errors |
| martin - stratospheric turbulences | 0.002 | 0.017 | 0.071 | yes | shader errors |
| Martin - wire dance | 0.013 | 0.032 | 0.355 | no |  |
| midgitstraights of majillaen - decaying smoulderworm glowwood | 0.014 | 0.080 | 0.131 | yes |  |
| nearly unavoidable isolatory strategy flx roams | 0.000 | 0.000 | 0.000 | yes |  |
| ORB - Ocean Glow | 0.012 | 0.060 | 0.105 | yes |  |
| PHAT HD FX 2 FiShbRaiN  -  Flexi  ft AdamFX Aderrasi , Martin n Rova - Witchcraft Take OverFX  | 0.003 | 0.030 | 0.011 | yes |  |
| RLP + Geiss - Bass Zoom Redux (3 Layers Minefield Bass Reaction) | 0.005 | 0.022 | 0.220 | yes |  |
| Rovastar & Unchained - Ambrosia Mystic (Dark Heart Mix) | 0.018 | 0.024 | 0.918 | no |  |
| Rovastar + Unchained - Ambrosia Mystic (Dark Heart Mix) | 0.018 | 0.024 | 0.918 | no |  |
| shell robot --- Isosceles edit | 0.000 | 0.000 | 0.000 | yes |  |
| shell robot nz+ psiqolojiq | 0.010 | 0.031 | 0.229 | yes |  |
| shifter + flexi - american pie 5 nz+ m3 w3 | 0.009 | 0.009 | 0.927 | yes |  |
| shifter - fractal grinder (opalescent) | 0.000 | 0.001 | 0.035 | yes |  |
| shifter - neon pulse (reactive) | 0.019 | 0.085 | 0.279 | yes |  |
| shifter - snakeskin (edged) | 0.016 | 0.076 | 0.358 | yes |  |
| shifter - spasmic comet neon - bleeding mandellard | 0.011 | 0.033 | 0.114 | yes |  |
| shifter - spincycle c | 0.003 | 0.009 | 0.464 | no |  |
| suksma - autoerotic solipsistic omniversal meme - convoluted debt saturation | 0.006 | 0.043 | 0.041 | yes |  |
| suksma - biotoxins on strings - busy highway ignores | 0.000 | 0.000 | 0.000 | yes |  |
| suksma - dotes hostile undertake - antiborg | 0.000 | 0.000 | 0.000 | yes |  |
| suksma - ed geining hateops - flx DddDddDddDdd die | 0.002 | 0.006 | 0.093 | yes |  |
| suksma - ed geining hateops - flx hate shade | 0.012 | 0.032 | 0.572 | yes |  |
| suksma - ed geining hateops - flx sg5 ink stink you dink | 0.006 | 0.009 | 0.285 | yes |  |
| suksma - fiShbRaiN - witchcraft (phugly shame on remix) - darning kneedless - quigliss | 0.002 | 0.015 | 0.109 | yes |  |
| suksma - flexi coheres vacuum energy - now don't freak out, i'm teleporting to your location | 0.015 | 0.060 | 0.051 | yes |  |
| suksma - indecision about how to die - decision made | 0.013 | 0.037 | 0.204 | yes |  |
| suksma - melting the boring | 0.006 | 0.029 | 0.017 | yes | shader errors |
| suksma - satanic teleprompter - feeling filthy | 0.004 | 0.005 | 0.131 | yes |  |
| suksma - shifter - spincycle + labor of others | 0.011 | 0.008 | 0.519 | yes |  |
| The NG + MindJourney - Unconstructed DreamSpace iLuvTrtls | 0.006 | 0.030 | 0.211 | yes |  |
| the ultimate preset - competitive biohazard --- Isosceles edit | 0.009 | 0.009 | 0.776 | yes |  |
| TonyMilkdrop - Angels Of Glory [Flexi - to get tasered and to like it + techstyle] --- Isosceles edit | 0.011 | 0.024 | 0.057 | yes |  |
| TonyMilkdrop - Rhythm Of My Life [Flexi - $this shall not retain] | 0.002 | 0.005 | 0.191 | yes |  |
| TonyMilkdrop - Tricolors [Flexi - away with it + $this shall not retain] --- Isosceled edit | 0.011 | 0.086 | 0.014 | yes |  |
| TonyMilkdrop - Tricolors [Flexi - feedback accomplishment + $this shall not retain] --- Isosceles edit | 0.000 | 0.000 | 0.000 | yes | shader errors |
| Tripgnosis - Firesticks | 0.017 | 0.050 | 0.912 | yes |  |
| Vovan - Bass With Flover - smooth kaleidoscope | 0.000 | 0.000 | 0.001 | no |  |

## Renders white (42 presets)

Washed out to white after 5 seconds.

| Preset | Mean | Contrast | Saturation | Preview | Errors |
|---|---|---|---|---|---|
| $$$ Royal - Mashup (379) | 0.986 | 0.043 | 0.022 | yes |  |
| $$$ Royal - Mashup (383) | 1.000 | 0.000 | 0.000 | yes |  |
| 153 | 0.999 | 0.014 | 0.002 | yes |  |
| 193 | 1.000 | 0.000 | 0.000 | yes |  |
| bdrv + al shifter - phasm mx | 1.000 | 0.000 | 0.000 | yes |  |
| Cope - Passage (mandala mix) | 0.998 | 0.024 | 0.002 | no |  |
| custom1 | 1.000 | 0.000 | 0.000 | yes |  |
| fiShbRaiN - betelguese (hyperdrive remix) - hebba | 1.000 | 0.000 | 0.000 | yes |  |
| fiShbRaiN - betelguese3 aa | 1.000 | 0.000 | 0.000 | yes |  |
| fleekus flokus - violent and insane homicidal air molecules2 - shf fab we are the info flow, not | 0.959 | 0.126 | 0.121 | yes |  |
| HALFBREAK - Kinetic Mantra | 0.957 | 0.118 | 0.089 | yes |  |
| Hexcollie, Aderassi, ORB, baked n 3dragons - Moving target | 0.985 | 0.087 | 0.013 | yes |  |
| hey horse teeth girl | 1.000 | 0.000 | 0.000 | yes |  |
| high_frequency/BEST OF ADAMFX 2 Flexi + Geiss - Bipolar vs reaction diffusion + Deamon Lord + EOS 23 | 0.988 | 0.033 | 0.010 | yes |  |
| high_frequency/fleekus flokus - violent and insane homicidal air molecules2 - shf fab we are the info flow, not the structure of space time | 0.959 | 0.126 | 0.121 | yes |  |
| high_frequency/shifter - phasm wraith (fps+ mix) | 1.000 | 0.000 | 0.000 | yes |  |
| LuxXx - The Feeder | 0.986 | 0.065 | 0.023 | yes |  |
| LuxXx - The Feeder II | 0.986 | 0.065 | 0.023 | yes |  |
| LuxXx - Token Beats | 0.991 | 0.042 | 0.018 | yes |  |
| martin - gentle happiness | 0.954 | 0.084 | 0.055 | yes |  |
| martin - starfield sectors | 0.997 | 0.026 | 0.003 | yes |  |
| martin - starfield sectors (Ati fix) | 0.997 | 0.026 | 0.003 | no |  |
| Martin N AdamFX Infusion = Phat+Yin+EoS_Mandala Chaser Ft AdamFX n Martin - The Beast Mandala Chaser FX H | 0.971 | 0.097 | 0.044 | yes |  |
| Martin n FriendzFX - Bombyx mori - The Undecided Big Bang - Ft AdamFX - Flexi - Rova - Orb - Hexocollie  | 0.952 | 0.088 | 0.280 | yes |  |
| Martin n FriendzFX - Bombyx mori - The Undecided Big Bang - Ft AdamFX - Flexi - Rova - Orb - Hexocollie B | 0.952 | 0.088 | 0.280 | yes |  |
| ORB - Supernova Meltdown | 1.000 | 0.000 | 0.000 | no |  |
| Rovastar + Geiss - Hyperkaleidoscope Glow 2 motion blur | 1.000 | 0.000 | 0.000 | no |  |
| Rovastar + Loadus + Geiss - FractalDrop (Triple Mix) | 0.978 | 0.116 | 0.006 | no |  |
| Rovastar + Loadus + Geiss - Tone-mapped FractalDrop 7c | 0.982 | 0.087 | 0.003 | no |  |
| Rovastar + Loadus - FractalDrop (Active Sparks Mix) Steady | 0.999 | 0.004 | 0.011 | no |  |
| shifter + flexi - liquid circuitry from the fractalism hive - orb tg lampart revision overdue | 1.000 | 0.000 | 0.000 | yes |  |
| shifter - escape (sigur ros) | 0.970 | 0.018 | 0.059 | yes |  |
| shifter - orthoganal light niobium | 0.954 | 0.087 | 0.080 | yes |  |
| suksma - flexi coheres vacuum energy - brujchump | 0.970 | 0.164 | 0.001 | yes |  |
| suksma - newspaper | 0.955 | 0.122 | 0.089 | yes |  |
| The NG + Stahlregen & Boz + EoS + Geiss + Phat + Rovastar + Zylot + Flexi + martin + Fishbrain - Machine Code (In The Wind Tunnel) | 0.994 | 0.051 | 0.008 | yes |  |
| Waltra - Emitter | 0.995 | 0.036 | 0.014 | yes |  |
| Waltra - Lyrium | 0.974 | 0.075 | 0.077 | yes |  |
| Waltra - Phantom UltraMATTER | 0.981 | 0.040 | 0.060 | yes |  |
| xtramartin (844) | 0.997 | 0.026 | 0.003 | yes |  |
| yin - 102 - Through the ether (The separation) - Bitcore Tweak | 0.979 | 0.030 | 0.037 | yes |  |
| Zylot - Crossing Over (Geiss Tweak) | 0.995 | 0.011 | 0.008 | no |  |

## Renders grey (92 presets)

Little or no colour. Some are monochrome by design.

| Preset | Mean | Contrast | Saturation | Preview | Errors |
|---|---|---|---|---|---|
| $$$ Royal - Mashup (191) | 0.037 | 0.136 | 0.000 | yes |  |
| $$$ Royal - Mashup (286) | 0.321 | 0.288 | 0.072 | yes |  |
| 143 | 0.032 | 0.145 | 0.073 | yes |  |
| _Aderrasi - Wanderer in Curved Space - mash0000 - faclempt kibitzing meshuggana schmaltz (Geiss greyscale mix) | 0.212 | 0.101 | 0.000 | no |  |
| _Geiss - Confetti (Kaleidoscope Mix) | 0.090 | 0.262 | 0.000 | no |  |
| _Mig_014_version2 | 0.027 | 0.136 | 0.045 | no |  |
| _Mig_146 | 0.423 | 0.082 | 0.000 | no |  |
| A HD ADAMFX Creation  !!FT  Martin + flexi +  Rovastar MDropFX Collaboration E Adam HD INFECTIONFX A | 0.940 | 0.196 | 0.004 | yes |  |
| A MilkKing Recreation FT Martin  - Violet Flash ft AdamFX n Geiss - Spiral Tunnels in FX  | 0.606 | 0.402 | 0.062 | yes |  |
| Aderrasi - Ghast (Quarter Past Ghast Mix) | 0.051 | 0.124 | 0.072 | no |  |
| amandio c - hat track - mrt sth | 0.313 | 0.304 | 0.000 | yes |  |
| amandio c - hat track - mrt sth3 | 0.571 | 0.371 | 0.000 | yes |  |
| amandio c - log tower 1 pig vomit 2uqle4 dorqstre l-egrunth | 0.044 | 0.152 | 0.056 | yes |  |
| amandio c - pulse - sth | 0.575 | 0.363 | 0.000 | yes |  |
| amandio c - typhoon season (inside) - sth | 0.679 | 0.373 | 0.000 | yes |  |
| Bdrv et AL Aderrasi - Accelerator (Orbital Migraine) bdvr xxx2_1 - mrt sth | 0.177 | 0.272 | 0.000 | yes |  |
| blood red raw holy - sth absolutely blazed lying naked hidden in the savannah | 0.447 | 0.396 | 0.000 | yes |  |
| carved in skin nz+ | 0.884 | 0.252 | 0.029 | yes |  |
| Cope-WIP - Strange Attractor 2 | 0.758 | 0.264 | 0.000 | no |  |
| EVET + Flexi -Hue Burster | 0.913 | 0.264 | 0.039 | yes |  |
| fiShbRaiN + Flexi - witchcraft unleashed [00 the template] + shifter - turing tape - criminally negligent homigrind | 0.175 | 0.077 | 0.027 | yes |  |
| fiShbRaiN + Flexi - witchcraft unleashed [00 the template] + shifter - turing tape - stochastic difference engine | 0.189 | 0.071 | 0.000 | yes |  |
| fiShbRaiN + Flexi - witchcraft unleashed [00 the template] + shifter - turing tape - zit gist | 0.158 | 0.072 | 0.001 | yes |  |
| fiShbRaiN - betelguese (hyperdrive remix) - as blue as our new second sun | 0.311 | 0.362 | 0.000 | yes | shader errors |
| fishbrain and flexi - box of tricks [hexcraft 09 - polar] - mrt sth | 0.286 | 0.330 | 0.000 | yes |  |
| Flexi + Rovastar + suksma - Fractopia [sth] | 0.245 | 0.223 | 0.000 | yes |  |
| flexi - blame moebius | 0.424 | 0.493 | 0.000 | no |  |
| Flexi - disruptor [vision of eggs] | 0.419 | 0.448 | 0.012 | yes |  |
| flexi - lorenz attractor | 0.021 | 0.101 | 0.047 | no |  |
| Fumbling_Foo, Martin, Orb, Flexi - Star Forge v7 (HAKAN mash-up) 2-55 [mixwithv6v1bass] remade7d [[pixel theme]] 0-2 {damaged publication} | 0.047 | 0.183 | 0.029 | yes |  |
| Geiss - Cosmic Dust 2 | 0.180 | 0.334 | 0.069 | no |  |
| Geiss - Digitunnel | 0.022 | 0.050 | 0.066 | no |  |
| Geiss - Game of Life | 0.151 | 0.267 | 0.000 | no |  |
| Geiss - Hurricane | 0.056 | 0.192 | 0.019 | no |  |
| Geiss - Kaleidoscope 1 | 0.054 | 0.160 | 0.077 | no |  |
| Geiss - Pistons | 0.054 | 0.116 | 0.061 | no |  |
| Geiss - Rose 3 | 0.651 | 0.194 | 0.078 | no |  |
| Geiss - Rose 4 | 0.100 | 0.214 | 0.051 | no |  |
| Geiss - Skin Dots 11b | 0.192 | 0.357 | 0.013 | no |  |
| Geiss - Skin Dots Multi-layer 3 | 0.480 | 0.339 | 0.000 | no |  |
| Geiss - Thumb Drum | 0.392 | 0.145 | 0.000 | no |  |
| gunthry is out back bloodying up the pine trees nz+ | 0.063 | 0.212 | 0.073 | yes |  |
| Halfbreak - Noise-volume Negative | 0.548 | 0.340 | 0.061 | yes |  |
| Hexcollie - Julian Shader Wars4 | 0.179 | 0.326 | 0.000 | yes |  |
| high_frequency/BEST OF ADAMFX 2 Flexi + Geiss - Bipolar vs reaction diffusion + Aderassi Phat Zylot4 --- Isosceles edit | 0.302 | 0.311 | 0.075 | yes |  |
| high_frequency/blood red raw holy - btking the taboo | 0.809 | 0.351 | 0.000 | yes |  |
| high_frequency/extreme/sewn to slavish memes | 0.646 | 0.256 | 0.051 | yes |  |
| high_frequency/Halfbreak + Flexi and goody + geiss + benski - Sparkly LSB Feedback' | 0.667 | 0.402 | 0.000 | yes |  |
| high_frequency/shifter - smoked glass - i can't afford to mash this | 0.509 | 0.388 | 0.000 | yes |  |
| high_frequency/suksma - pus on my cat - hate shade | 0.819 | 0.355 | 0.000 | yes |  |
| high_frequency/TonyMilkdrop - Rhythm Of My Life [Flexi - techstyle] | 0.477 | 0.447 | 0.011 | yes |  |
| Jc - Snowflake | 0.137 | 0.233 | 0.029 | yes |  |
| martin + stahlregen - martin in da mash 12b | 0.850 | 0.082 | 0.039 | yes | code errors |
| martin - adrift on a dead planet - lite | 0.689 | 0.300 | 0.069 | no |  |
| martin - gate to moria | 0.345 | 0.235 | 0.000 | no |  |
| martin - mandelbox explorer - high speed demo version (LamersAss Remix)(1) | 0.945 | 0.001 | 0.048 | yes |  |
| martin - move this body | 0.119 | 0.210 | 0.077 | no |  |
| martin - X Portal | 0.080 | 0.160 | 0.000 | no |  |
| NeW Adam Master Mashup FX 2 Geiss - Reaction Diffusion 34 + Swelling Spiral  | 0.330 | 0.428 | 0.002 | yes |  |
| NeW Adam Master Mashup FX 2 Geiss and Zylot - Reaction Diffusion 3 (Overload Mix 2) AMAZING MASUP Various + Hurricane Nightmare | 0.560 | 0.336 | 0.067 | yes |  |
| ORB - Cloud Burst | 0.923 | 0.169 | 0.048 | yes |  |
| ORB - Comic Book Curves 2 thine pain chasm believable character slaughter | 0.323 | 0.433 | 0.063 | yes |  |
| ORB - Sandblade | 0.047 | 0.168 | 0.047 | no | shader errors |
| Serge + 158 002 | 0.868 | 0.246 | 0.013 | yes |  |
| Serge + martin - crystal palace006 | 0.816 | 0.319 | 0.019 | yes |  |
| Serge + martin - crystal palace007d | 0.078 | 0.210 | 0.046 | yes |  |
| Serge + martin - soma | 0.905 | 0.218 | 0.023 | yes |  |
| Serge mashup003 | 0.896 | 0.218 | 0.060 | yes |  |
| shifter - digi | 0.030 | 0.080 | 0.066 | no |  |
| shifter - phasm wraith - yi qing short term long term dance | 0.280 | 0.190 | 0.000 | yes |  |
| some say the end is near | 0.934 | 0.146 | 0.071 | yes |  |
| Stahlregen & Aderrasi + bdrv + EoS + flexi + Geiss + krash + shifter + TEcHNO - Liquid Star (RMX)_1 | 0.029 | 0.022 | 0.000 | yes |  |
| Stahlregen & Boz - Machine Code (Reaction Diffusion)_1 - Isosceles Tweak 06 | 0.480 | 0.168 | 0.000 | yes |  |
| Stahlregen & Boz - Machine Code (Reaction Diffusion)_1 - Isosceles Tweak 08 | 0.685 | 0.271 | 0.000 | yes |  |
| Stahlregen & Boz - Machine Code (Reaction Diffusion)_1 - Isosceles Tweak 10 | 0.436 | 0.158 | 0.000 | yes |  |
| stahlregen & fishbrain + flexi + geiss + shifter - stonecraft (mashup 19c) | 0.633 | 0.075 | 0.072 | yes |  |
| Stahlregen - Old School, Baby! (Black and White Mash) | 0.455 | 0.378 | 0.000 | yes |  |
| suksma - biotoxins on strings - thyghmaster oilstrip lifedecay | 0.312 | 0.278 | 0.000 | yes |  |
| suksma - biotoxins on strings - walk away + flx per vtx equ | 0.516 | 0.217 | 0.000 | yes |  |
| suksma - chemosynthetic nosferatu - grey budget | 0.105 | 0.105 | 0.013 | yes |  |
| suksma - flaring squamoid mode placement nz | 0.034 | 0.156 | 0.056 | yes |  |
| suksma - grainery clock mortis | 0.945 | 0.109 | 0.031 | yes |  |
| suksma - pus on my cat - fun bags --- Isosceles edit | 0.778 | 0.382 | 0.035 | yes |  |
| suksma - yin - 395 - Artificial Inspiration (brainstorming - outward) + martin + flexi + stahlregen - el | 0.206 | 0.372 | 0.007 | yes |  |
| suksma - yin - 395 - Artificial Inspiration (brainstorming - outward) + martin + flexi + stahlregen - elephant mating - maggotron | 0.206 | 0.372 | 0.007 | yes |  |
| TonyMilkdrop - Angels Of Glory [Flexi - an excuse taken back + techstyle] --- Isosceles edit | 0.205 | 0.326 | 0.067 | yes |  |
| TonyMilkdrop - Angels Of Glory [Flexi - arrogance amplifier + techstyle] --- Isosceles edit | 0.630 | 0.388 | 0.002 | yes |  |
| TonyMilkdrop - Angels Of Glory [Flexi - safer sex + techstyle] --- Isosceles edit | 0.326 | 0.429 | 0.059 | yes |  |
| va ultramix - 154 [chaos] | 0.892 | 0.059 | 0.000 | yes |  |
| various - whirygig03 - flx rigidigeridoolittle' - 2 chokmah wisdom grey zodiac neptune twos | 0.276 | 0.287 | 0.000 | yes |  |
| xtramartin (83) | 0.843 | 0.091 | 0.032 | yes |  |
| Zylot - True Visionary (Go Further Back) - Serge - Mashpotato 1 | 0.030 | 0.156 | 0.072 | yes |  |

## Renders flat (17 presets)

A single flat tone with almost no detail.

| Preset | Mean | Contrast | Saturation | Preview | Errors |
|---|---|---|---|---|---|
| BrainStain- boiling-mix2(redi jedi full carb mix) | 0.172 | 0.014 | 0.435 | yes |  |
| flexi + geiss - bipolar vs reaction diffusion [antirotating reflection]_Phat_SeizureBox | 0.782 | 0.001 | 0.764 | yes |  |
| Flexi - crush ice 62 | 0.573 | 0.000 | 0.812 | yes | shader errors |
| Flexi - operation acid storm [featuring geiss' rayleigh scattering finish] | 0.687 | 0.017 | 0.399 | yes | shader errors |
| Flexi - truly soft piece of software - topology - cohere | 0.089 | 0.010 | 0.540 | yes | shader errors |
| geiss + martin - mandelbox explorer - high speed demo version (fire mix) | 0.872 | 0.002 | 1.000 | no |  |
| Geiss - Eye of Jupiter | 0.416 | 0.010 | 1.000 | no |  |
| Geiss - Iris | 0.054 | 0.019 | 0.308 | no |  |
| high_frequency/Bdrv Aderrasi - Accelerator (Orbital Migraine) BDRV et  AL  rmx8 - amoral love meets phreq missionary | 0.279 | 0.014 | 0.932 | yes | shader errors |
| high_frequency/extreme/Bdrv Aderrasi - Accelerator (Orbital Migraine) BDRV et  AL  rmx8 - amoral love meets phreq missionary | 0.279 | 0.014 | 0.932 | yes | shader errors |
| high_frequency/extreme/Bdrv Aderrasi - Accelerator (Orbital Migraine) BDRV et  AL  rmx8 - amoral love meets phreq missionary nz | 0.274 | 0.016 | 0.981 | yes | shader errors |
| martin - mandelbox explorer - high speed demo version | 0.650 | 0.001 | 0.169 | no |  |
| martin - mandelbox explorer - wreck diver nz+ historical | 0.792 | 0.000 | 0.694 | yes |  |
| martin - mandelbox explorer - wreck diver nz+ phranq | 0.167 | 0.000 | 0.893 | yes |  |
| pheremones is spelled pheromones asshole | 0.293 | 0.014 | 0.584 | yes |  |
| suksma - ed geining hateops - flx palette massacrow | 0.038 | 0.012 | 0.874 | yes |  |
| xtramartin (1) | 0.650 | 0.000 | 0.168 | yes |  |

## Mismatches seen on contact sheets

These render with normal brightness but look clearly different from their MilkDrop previews.

- greazed up humility - cable knit sveat: fine grey noise instead of a coloured burst.
- suksma - lick salad dry --- Isosceles edit: purple swirl instead of blue clouds.
- stahlregen - old school, baby! (spiral ornament): dark with thin lines instead of large green blocks.
- RLP + Geiss - Bass Zoom Redux (3 Layers Minefield Bass Reaction): near black instead of bright lavender.
- TonyMilkdrop - Tricolors [Flexi - away with it + $this shall not retain] --- Isosceled edit: black with one small object instead of a full-screen image.
- Flexi + Rovastar + suksma - Fractopia [sth]: grey instead of yellow and purple.
- shifter - spun sugar - smooth cheese: dim red wisps instead of a bright white burst.
- suksma - breaking the beam only to hump the tower --- Isosceles edit: red and orange instead of pale yellow and white.

`flexi - julia island [output stage]` renders flat magenta. It has no preview to compare against.
