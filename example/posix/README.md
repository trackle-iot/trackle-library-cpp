# Examples for POSIX systems

## Content
This folder contains examples meant to be used in POSIX OSes (macOS and Linux systems in particular).

These examples were tested on:
 - Ubuntu 22.04 (GCC 11.3.0 targeting x86_64);
 - macOS Monterey 12.2 (clang 13.1.6 targeting x86_64).

The version of the library the examples were linked against, at the time of writing, is *trackle_library_cpp v2.3.0* .

## Examples

There are two examples in src folder:
 - ```src/main.c``` (builds ```bin/example_c```);
 - ```src/main.cpp``` (builds ```bin/example_cpp```).

The first one is written in pure C and uses the library through its C interface (defined by ```trackle_interface.h```).

The second one is written in C++ and uses the library directly (through ```trackle.h```).

The functionality implemented by the two examples is the same.

The examples use the same set of callback functions defined inside ```src/callbacks.c```.

In order to be able to connect to the cloud, credentials must be provided inside ```include/trackle_hardcoded_credentials.h```. Instructions to perform this operation are provided inside the file itself.

## Build and run

In order to build and run examples, one has to:

1. Open a terminal window;
2. ```cd``` to the folder containing the examples (the one where this README is located);
3. Build the examples by running ```make```command;
4. Run an example by launching one of the executables generated inside the ```bin``` subfolder.

