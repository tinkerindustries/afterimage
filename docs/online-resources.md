# Online resources for MilkDrop and Geiss

MilkDrop effects survive in large numbers because they are plain-text preset files that people shared and archived. The original Geiss effects are compiled into the program, so the way to see them is to run Geiss itself or watch recordings.

## Watching in a browser

- [Butterchurn](https://github.com/jberg/butterchurn) is a WebGL 2 port of MilkDrop. Its demo site, butterchurnviz.com, runs classic presets against a microphone or an audio file. The code is the [`butterchurn`](https://www.npmjs.com/package/butterchurn) npm package, and the `butterchurn-presets` package holds the preset library.
- [Churn](https://churn.leyanlo.com/credits) is an unofficial browser front end built on Butterchurn. Its credits page names the original preset authors.
- Webamp (webamp.org) recreates Winamp in the browser and uses Butterchurn for its MilkDrop window.

## Preset collections

- [projectM](https://github.com/projectM-visualizer/projectm) is the open-source MilkDrop reimplementation for Linux, macOS and Windows. It reads `.milk` files.
- Cream of the Crop is a curated pack of MilkDrop presets with their textures. The [foobar2000 projectM plugin](https://www.foobar2000.org/components/view/foo_vis_projectM/releases) bundles it, and its release notes link to a NestDrop blog post describing the collection. Some reports say a share of those presets render incorrectly in projectM.
- The F-Droid apps [ProjectM Android](https://f-droid.org/bo/packages/nl.neerdael.projectm/) and [ProjectM TV](https://f-droid.org/bo/packages/nl.neerdael.projectmtv/) offer the same collection as an optional download.

## The original Geiss

- Ryan Geiss's site has pages for [Geiss 2](https://www.geisswerks.com/geiss2/geiss2.html) and the original Geiss. Both are Windows Winamp plugins and screensavers.
- A web search did not find the Geiss source on GitHub. Whether it is public is unconfirmed.
- YouTube has many screen recordings of both.

## Reading presets

`.milk` files are plain text. Opening one shows its base values and its per-frame and per-vertex equations, which map onto the warp, decay and seed loop described in [how-feedback-visualisers-work.md](how-feedback-visualisers-work.md).
