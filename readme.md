LabStream Backend
=================

This repository contains some C code for intefacing with the LabStream components through Redis, using hiredis and libevent.

It has generic immplementations of Sources and Streams, that can be reused to make any device talk to the system easily.

The repository with the webserver code can be found at http://atg.lbl.gov/gitlab/repovizz/framework.

Structure
----------

* `lib` contains a recent copy of hiredis, in case the user wants to statically link it, as well as its headers and a minimal JSON parser.
* `src` contains the Stream C class, as well as a draft of the Entity C++ class.
* `test` contains some test code that creates a Source and generates noisy sinusoids.
* `fpga` contains the code for connecting to the FPGA and making it talk to Redis.

Installation
----------

First of all, make sure that you have libevent installed (`libevent-dev` package).
If you don't have hiredis installed, go to `lib/hiredis` and run `make && sudo make install`.
Then, go back and run `make`. There are 2 tasks, `test` and `fpga`.

Usage
-------

The binaries are in `test/main` and `fpga/main`.

Simply run the desired binary. The first argument specifies the port Redis is listening to (6379 by default). The second argument specifies the Redis host (localhost by default).

For the FPGA, set the `BOARD_IP` env variable to the IP of the board, as in: `BOARD_IP=192.168.12.34 ./fpga/main`.

> WARNING: libhiredis is installed by default under `usr/local/lib`. If the library is not found, add it to the path with `export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib`.