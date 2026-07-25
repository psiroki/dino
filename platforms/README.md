# Cross Building Guidelines

This directory contains scripts and configurations for cross-compiling the project for various target handheld gaming devices using Docker toolchain environments.

## Overview

To build all target platforms (or a specific target platform), execute the shell script:

```bash
./platforms/cross_build.sh [TARGET_SUFFIX]
```

If `TARGET_SUFFIX` is omitted, `cross_build.sh` iterates through all entries configured in `platforms.txt`.

---

## Setting Up Toolchains & `platforms.txt`

The build system relies on Docker toolchain Makefiles maintained in separate GitHub repositories. To set up your local build environment:

### 1. Clone the Toolchain Repositories

Clone the necessary toolchain repositories onto your machine:

- **RG35XX Garlic / Bittboy**: [https://github.com/psiroki/rg35xx-toolchain.git](https://github.com/psiroki/rg35xx-toolchain.git)
- **Miyoo A30**: [https://github.com/psiroki/gpuarmhf.git](https://github.com/psiroki/gpuarmhf.git)
- **Miyoo Mini / Mini Plus**: [https://github.com/psiroki/miyoomini-buildroot.git](https://github.com/psiroki/miyoomini-buildroot.git)
- **AArch64 (e.g. Powkiddy x55)**: [https://github.com/psiroki/x55toolchain.git](https://github.com/psiroki/x55toolchain.git)

### 2. Create `platforms/platforms.txt`

Create a file named `platforms.txt` in the `platforms/` directory (`platforms/platforms.txt`).

> [!NOTE]
> `platforms.txt` is git-ignored because it contains absolute local filesystem paths to your cloned toolchain repositories.

#### File Format

Each non-empty line in `platforms.txt` defines a build target in space-delimited format:

```text
<TAG> <SUFFIX> <TOOLCHAIN_DOCKER_MAKEFILE_DIR>
```

- **`TAG`**: CMake flag name passed to `internal_build_helper.sh` (e.g. `MIYOO`, `RG35XX`, `BITTBOY`). During compilation, `internal_build_helper.sh` invokes `cmake` with `-D<TAG>=ON`.
- **`SUFFIX`**: Target identifier / platform directory suffix (e.g., `mm`, `a64`, `bb`, `ma30`). Output files for the target are built into `build/platforms/<SUFFIX>`.
- **`TOOLCHAIN_DOCKER_MAKEFILE_DIR`**: Path to the local directory containing the Makefile for the target platform's cross-compilation Docker container.

#### Example `platforms.txt`

```text
BITTBOY bb /path/to/cloned/rg35xx-toolchain
RG35XX a64 /path/to/cloned/x55toolchain
MIYOO mm /path/to/cloned/miyoomini-buildroot
RG35XX22 garlic /path/to/cloned/rg35xx-toolchain
MIYOOA30 ma30 /path/to/cloned/gpuarmhf
```

---

## Build Workflow & Execution Details

When `cross_build.sh` runs:

1. **Target Selection & Setup**: Reads each line of `platforms.txt` and ensures the output build directory `build/platforms/<SUFFIX>` exists.
2. **Compilation**:
   Changes directory into the specified toolchain directory (`TOOLCHAIN_DOCKER_MAKEFILE_DIR`) and invokes the `shell` target of its `Makefile` (line 42 of `cross_build.sh`):
   ```bash
   make "WORKSPACE_DIR=$PROJECT_ROOT" \
        "DOCKER_CMD=/bin/bash /workspace/platforms/internal_build_helper.sh build/platforms/$SUFFIX $TAG" \
        "INTERACTIVE=0" \
        shell
   ```
   - The target platform's `Makefile` MUST support a `shell` target that launches the build environment or Docker container using `DOCKER_CMD`.
   - `internal_build_helper.sh` configures CMake with `-D<TAG>=ON` and builds the executable inside `build/platforms/<SUFFIX>`.
3. **Packaging**:
   - `cross_build.sh` checks for the existence of a packer directory in `platforms/packers/<TAG>-<SUFFIX>`.
   - If a packer directory with a `Makefile` exists (e.g., `platforms/packers/MIYOO-mm`), it automatically executes `make` inside that directory with `PROJECT_ROOT` and `BUILD_DIR` parameters.
   - The packer Makefile packages the executable along with assets and shortcuts into a distributable `.zip` archive (placed in `build/platforms/`).
