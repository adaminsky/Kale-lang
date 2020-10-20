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

Execute `./Kale` and then type in your program. Alternatively, pipe the text of
the program into stdin like the following:

```sh
./Kale < fib.kl
```

An object file is output called output.o and the LLVM IR is dumped to stderr.
To try running a program, pipe stderr into another file and then call
clang on that. In the following example, I am using `printd` which is defined
in the print\_dyn.cc file and created as the shared library "libprint". To
link with this library, do the following:

```sh
./Kale < fib.kl 2> fib.ir
clang -x ir fib.ir -L. -lprint
```

This assumes that you are executing the above commands in the `build/` folder.
Finally, to correctly run the program with the dynamic library, the
`LD_LIBRARY_PATH` must be set to the directory where `libprint.so` is located
which is probably the current build directory.

Another way that a Kale program can be used is by linking with a C/C++
program by using the `output.o` file as an input to a clang build.
