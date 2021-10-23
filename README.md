# Multi Channel Self Organized Time Division Multiple Access (MCSOTDMA) Protocol

Implements the MCSOTDMA protocol.

## Building
The protocol is made available as a shared library.
It is unit-tested through an executable that loads this library.

Both targets must be linked against the `glue` library.
The `CMakeLists.txt` expects it to be available through a folder, which should be provided using symbolic links.

### Library Prerequisites
The `libtuhh_intairnet_mc-sotdma.so` library must be linked against the [Glue Library](https://collaborating.tuhh.de/e-4/research-projects/intairnet-collection/intairnet-linklayer-glue/-/tree/master).  

- Please create a symbolic link `glue-lib-headers` that points to the top directory of that repository, i.e. `ln -s <path/to/glue/lib> glue-lib-headers`.

### Executable Prerequisites
The `mcsotdma-unittests` exectuable must be linked against the [Glue Library](https://collaborating.tuhh.de/e-4/research-projects/intairnet-collection/intairnet-linklayer-glue/-/tree/master).  

- Please create a symbolic link `glue-lib-headers` that points to the top directory of that repository, i.e. `ln -s <path/to/glue/lib> glue-lib-headers`.

### Using `cmake`
Once the prerequisities are provided, you can build the two targets using `cmake`.  

#### Release build
- Create a directory for the release-type build: `mkdir cmake-build-release`.  
- Change to that directory: `cd cmake-build-release`
- Call `cmake` to generate a `Makefile`: `cmake -DCMAKE_BUILD_TYPE=Release ..`
- Build the library: `make tuhh_intairnet_mc-sotdma`
- Build the executable: `make mcsotdma-unittests`

#### Debug build
- Create a directory for the release-type build: `mkdir cmake-build-debug`.  
- Change to that directory: `cd cmake-build-debug`
- Call `cmake` to generate a `Makefile`: `cmake -DCMAKE_BUILD_TYPE=Debug ..`
- Build the library: `make tuhh_intairnet_mc-sotdma`
- Build the executable: `make mcsotdma-unittests`
