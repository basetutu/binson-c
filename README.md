ANSI C implementation of Binson format serialization library. Binson is like JSON, but faster, binary and
even simpler. See [binson.org](http://binson.org/)

Status
---------

Pre-alpha (debugging and testing functionality)

Features
---------

* Written in ANSI C (C89 standard)
* Has not 3rd party dependencies (stdlib only)
* Streaming mode parsing
* Input/output abstraction layer


Usage
---------

See ./examples subdirectory for usage examples.


Build
---------

You need to have CMake installed in your system.
Check its presence by:

`
$ cmake --version
`

If you can't see the version info, you need to install it.
For Debian/Ubuntu installation from repo:

`
$ sudo apt-get install cmake
`

For CentOS installation from repo:

`
$ sudo yum install cmake
`


To build binson-c library (plus examples) run from project directory:

```
$ mkdir build
$ cd build
$ cmake ..
$ make all
```

ToDo (before beta)
---------
- [] Test on bigendian (ARM, etc) platforms
- [] Unit testing configuration
- [] Binson-java compatibility testing
- [] Extensive error handling
- [] Adopt interfaces to provide model's storage abstraction
- [] ABI review


Feature requests
---------

* JSON import (using external parser library)

