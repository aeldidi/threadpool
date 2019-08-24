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

# API Docs #

Documented here will be all of the exposed functions included with the file.

### class `didi::threadpool`: ###

### Member Functions ###

#### constructor ####
```c++
threadpool(int threadc);
```
creates a threadpool with `threadc` threads.

Returns once all threads have been spun up. Throws `std::bad_argument` when
threadc is less than 1.

#### destructor ####
```c++
~threadpool();
```

Clears the threadpool's queue and allows the threadpool to finish any current
jobs before freeing its resources.

Returns immediately, allowing the threads to shut down on their own in the
background.

#### reset ####
```c++
void reset();
```

Clears the threadpool's queue so new jobs can be taken in, then waits for the
current jobs to finish.

Returns after all threads become idle.

#### wait ####
```c++
void wait();
```

Waits for the threadpool to finish all queued jobs.

Returns after all threads become idle.

#### add_job ####
```c++
void add_job(std::function<void()> fn);
```

Adds the function fn to the head of the threadpool's jobqueue.

Returns immediately.