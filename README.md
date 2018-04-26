# A First-Fit Heap written in C++

[![Build Status](https://travis-ci.org/tpressure/first-fit-heap.svg?branch=master)](https://travis-ci.org/tpressure/first-fit-heap) (GCC and Clang)


A free-standing first fit heap implementation in C++. The library is header only and can easily be integrated into other projects.

## Requirements

* gcc >= 4.8

## Supported Platforms

* Linux
* Freestanding

## Configuration

### Linux

For a Linux project, just include the **heap.hpp** header where appropriate and add **-DHEAP\_LINUX** to your build configuration.

### Freestanding

If you want to use the heap as a freestanding library, e.g. in a operating system kernel or a bare metal application, the following
types and functions need to be implemented:

* size\_t and uint\*\_t
* placement new
* heap\_max(x, y) returning the x if x > y, and y otherwise
* heap\_assert(cond, str) terminating the program if cond == false, continue otherwise
