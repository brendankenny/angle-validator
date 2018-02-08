# angle-validator

## Build (Mac/Linux)
1. `git clone https://github.com/brendankenny/angle-validator.git`
2. Download, install, and activate the [Emscripten SDK](https://kripken.github.io/emscripten-site/docs/getting_started/downloads.html)
    ```sh
    git clone https://github.com/juj/emsdk.git
    ./emsdk/emsdk install latest
    ./emsdk/emsdk activate latest
    ```
3. `git clone https://chromium.googlesource.com/angle/angle` (TODO: lock version)
4. `yarn build`

## Update source list
Occasionally, will need to regenerate list of source files and compile flags as ANGLE is updated.
1. [initialize ANGLE](https://github.com/google/angle/blob/master/doc/DevSetup.md#getting-the-source)
    ```sh
    cd angle
    python scripts/bootstrap.py
    gclient sync
    cd ..
    ```
2. `yarn gen`
3. Compare to checked-in build config (`git diff build/build-config.json`)
4. `yarn build` if needed


