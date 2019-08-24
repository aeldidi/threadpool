# C++11 Threadpool #
A single file C++11 implementation of a threadpool in the public domain.

A primary goal of this libaray is to create a clean implementation which
can be easily modified to fit the needs of any project. If anyone has
any suggestions of how performance can be improved without sacrificing
readibility, please contribute.

## Usage ##
Being one file, you will have to define `THREADPOOL_IMPLEMENTATION` when 
compiling one of your source files. That will be the one where the threadpool 
implementation will reside.

The rest of the source files using it can simply `#include` the file 
themselves.

The threadpool class is availible in the `didi` namespace and can be accessed
using `didi::threadpool`.