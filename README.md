# Multi Channel Self Organized Time Division Multiple Access (MCSOTDMA) Protocol

Implements the MCSOTDMA protocol.

## Building
The protocol is made available as a shared library.
Unit-tests it through an executable.

Both targets must be linked against other LDACS libraries.
The `CMakeLists.txt` expects these to be available through folders, which should be provided using symbolic links.

### Library Prerequisites
The `libtuhh_intairnet_mc-sotdma.so` library must be linked against the [Glue Library](https://collaborating.tuhh.de/e-4/research-projects/intairnet-collection/intairnet-linklayer-glue/-/tree/master).  

- Please create a symbolic link `glue-lib-headers` that points to the top directory of that repository, i.e. `ln -s <path/to/glue/lib> glue-lib-headers`.

### Executable Prerequisites
The `mcsotdma-unittests` exectuable must be linked against both the [Glue Library](https://collaborating.tuhh.de/e-4/research-projects/intairnet-collection/intairnet-linklayer-glue/-/tree/master) as well as the [RLC Library](https://collaborating.tuhh.de/e-4/research-projects/intairnet-collection/avionic-rlc).  

- Please create a symbolic link `glue-lib-headers` that points to the top directory of that repository, i.e. `ln -s <path/to/glue/lib> glue-lib-headers`.
- Please create a symbolic link `avionic-rlc-headers` that points to the top directory of that repository, i.e. `ln -s <path/to/rlc/lib> avionic-rlc-headers`.  
- Please create a symbolic link `avionic-rlc` that points to the build directory of the repository, i.e. `ln -s <path/to/rlc/lib/cmake-build-debug> avionic-rlc`.  

