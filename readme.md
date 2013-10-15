LabStream Backend
=================

This repository contains some C code for intefacing with the LabStream components through Redis, using hiredis and libevent.

It has generic immplementations of Sources and Streams, that can be reused to make any device talk to the system easily.

The repository with the webserver code can be found at https://github.com/repovizz/frontend.

Structure
----------

* `lib` contains a recent copy of hiredis, in case the user wants to statically link it, as well as its headers and a minimal JSON parser.
* `src` contains the Stream C class, as well as a draft of the Entity C++ class.
* `test` contains some test code that creates a Source and generates noisy sinusoids.

Installation
----------

First of all, make sure that you have libevent installed (`libevent-dev` package).
If you don't have hiredis installed, go to `lib/hiredis` and run `make && sudo make install`.
Then, go back and run `make`.

Usage
-------

The demo binary is in `test/main`. Arguments are hard-coded for now, but should be Redis server hostname and port to connect to.

> WARNING: libhiredis is installed by default under `usr/local/lib`. If the library is not found, add it to the path with `export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib`.