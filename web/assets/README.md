# Optional 1980s NES room asset

The web scene runs without external assets using generated Three.js geometry.

For the intended room, download **Nintendo Entertainment System - 85' Scene** by Miguel Adão from Sketchfab and place the exported GLB at:

`web/assets/nes-room.glb`

Source: https://sketchfab.com/3d-models/nintendo-entertainment-system-85-scene-cfd6dcb544fa4401b3dac20061d7fefb

License: Creative Commons Attribution (CC BY). Keep the artist attribution with any public deployment.

The loader searches mesh names containing `screen`, `display`, `glass`, `crt`, `television`, or `tv` and replaces the best candidate material with the live NES framebuffer. If the source model uses different names, inspect the scene in the browser console and adjust `findScreen()` in `web/scene.js`.
