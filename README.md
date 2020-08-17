# Kale Lang

![Badge](https://github.com/adaminsky/Kale-lang/workflows/CI/badge.svg)

- Based on the LLVM Kaleidoscope tutorial, but tried to use actual good design
  principles.
- Just a fun project done to learn about how LLVM works.

## Building

```sh
mkdir build && cd build
cmake ..
make
```

## Running

Execute `./Kale` and then type in your program. Currently, every token must be
separated by a space, so the following is valid:

```sh
ready> extern cos ( x ) ;
```
