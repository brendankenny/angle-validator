# angle-validator

A working build is shipped in `out/`. Installation and (re)building is only necessary if modifying source.

## Init and Build (Mac/Linux)
1. `git clone https://github.com/brendankenny/angle-validator.git`
2. Download, install, and activate the [Emscripten SDK](https://kripken.github.io/emscripten-site/docs/getting_started/downloads.html)
    ```sh
    git clone https://github.com/juj/emsdk.git
    ./emsdk/emsdk install latest
    ./emsdk/emsdk activate latest
    ```
3. `git clone https://chromium.googlesource.com/angle/angle` (TODO: lock version)
4. `yarn build`

## Test
`yarn test`

## Update source list
Occasionally, will need to regenerate list of source files and compile flags as ANGLE is updated.
1. Check for changes to shader translator sample
    ```sh
    cd angle
    git fetch
    git diff origin/master -R -- samples/shader_translator/shader_translator.cpp
    git diff origin/master -R # optional, will be very large
    git pull # if desired
    cd ..
    ```
2. If update needed, [initialize ANGLE](https://github.com/google/angle/blob/master/doc/DevSetup.md#getting-the-source)
    ```sh
    cd angle
    python scripts/bootstrap.py
    gclient sync
    cd ..
    ```
3. `yarn gen`
4. Compare to checked-in build config (`git diff build/build-config.json`)
5. `yarn build` if code or build changes necessitate

## Update test expectation binary
If ANGLE is updated, should update test expectation generator from ANGLE source.
1. `yarn build --gt`
