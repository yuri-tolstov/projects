Introduction
------------

This directory has a simple example demonstrating a client-server
application, the client of which is run under Tile Linux, and the server
of which is run under the Tilera Bare Metal Environment (BME).  The two
tiles communicate via messages on the User Dynamic Network (UDN), and
via shared memory.  The example application encrypts an input data file
using a trivial encryption scheme.

Running The Example
-------------------

To run the example, set the TILERA_ROOT environment variable to the base
directory of your Tilera MDE installation.  You may then do:

   make run_dev

to compile and run the example on a Tilera TILExpress card, or

   make run_sim

to compile and run it under the simulator.

Expected Output
---------------

The server tile prints messages similar to the following when it starts up, and when it gets requests:

(0,0) server: my UDN coordinates are (0,0)
(0,0) server: request from (0,1): map PA 0x3000_0000 PTE 0x2c_0003_0010_603b
(0,0) server: request from (0,1): process 407 bytes at offset 0 to offset 0x400

(etc.)

These messages will be printed to your screen via tile-monitor, which
is set in this example to use the --console option.  Note that you can't
run tile-console simultaneously with the tile-monitor --console option
due to device locking restrictions.

When either test is done, the Makefile compares the test output with the
expected output using the diff utility.  If there are differences,
they are displayed.
