# Build notes

The application targets the HiBy R1's MIPS32r2 Linux environment. The makefile expects a MIPS little-endian cross compiler named `mipsel-linux-gnu-gcc` and its matching `strip` utility. Set `CROSS_COMPILE` to the toolchain prefix if yours uses another name.

1. Initialize the platform dependency from the repository root:

   ```sh
   git submodule update --init --recursive
   ```

2. Build the app and helper sources:

   ```sh
   make -C app
   ```

3. To build the optional packet capture helper:

   ```sh
   make -C app capture
   ```

The shared framebuffer, key input, and font implementation comes from the pinned `bidhata/Hiby-R1-Mod` submodule. The app also expects compatible network utilities and device interfaces at runtime; availability varies by firmware. This document records the current build intent, not a verified reproducible build.

The repository intentionally omits firmware, kernel modules, compiled binaries, and packet captures. Obtain firmware through official channels and build target binaries locally.
