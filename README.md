# Dino Jump

## Building

### Desktop

```bash
mkdir build
cd build
cmake ..
make
```

There is no packager for desktop. You can launch the game by executing `build/target/dino_jump` in the repo root.
The desktop build requires the following files:

```
dino_jump
assets/assets.bin
assets/PC.layout
```

You can control the game with the arrow keys, Q/E sets the difficulty level.

### Cross compiling for other platforms

The build system uses Docker images for cross compilations set up by custom makefiles. These can be found in GitHub repositories.

Repo locations:

- RG35XX Garlic (and Bittboy): https://github.com/psiroki/miyoomini-buildroot.git
- ARM64: https://github.com/psiroki/x55toolchain.git
- MiyooMini: https://github.com/psiroki/miyoomini-buildroot.git

Clone these repos in some folder, and you can create a `platforms/platforms.txt` based on this by filling out the paths as
described with the angled bracket placeholders (you need the Emscripten SDK for the web build):

```
BITTBOY bb <path to cloned RG35XX Garlic and Bittboy git repo>
RG35XX a64 <path to cloned ARM64 git repo>
RG35XX a32 <path to cloned MiyooMini git repo>
RG35XX22 garlic <path to cloned RG35XX Garlic and Bittboy git repo>
MIYOO mm <path to cloned MiyooMini git repo>
MIYOOA30 ma30 <path to cloned MiyooMini git repo>
WEB web <path to Emscripten.cmake>
```

Feel free to omit any of the target platforms above, obviously you don't even have to clone repos or download the Emscripten SDK
if you don't need any platform.

You can run `cross_build.sh` which will run through all the platforms specified in `platforms.txt`, build, and even package those
platforms that have a packer defined.
