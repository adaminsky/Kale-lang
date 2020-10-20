# Kale Lang

![Badge](https://github.com/adaminsky/Kale-lang/workflows/CI/badge.svg)

- Based on the LLVM Kaleidoscope tutorial, but tried to use some good design
  principles I've learned from taking a compilers class.
- Mostly just a fun project done to learn more about LLVM.

## Building

```sh
mkdir build && cd build
cmake ..
make
```

This project depends on LLVM libraries, so just having clang is not enough to
build.

## Running

Execute `./Kale` and then type in your program. An example interactive session
is shown below:

```sh
ready> extern cos(x);
...
ready> cos(1.2345);
```

Ending the session with CTRL-d writes the object output.o. This file can then
be linked into a C/C++ program.
