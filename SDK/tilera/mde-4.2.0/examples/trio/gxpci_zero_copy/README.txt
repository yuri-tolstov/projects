Introduction
------------

This example shows a minimal application that uses zero-copy
command queues to send a packet from the host to the TILEncore-Gx
card. The source code can be compiled on either the host or Tile
platforms; a different version of the run_test() method is used
according to which platform is being compiled.

Running The Example
-------------------

To run the example, install a TILEncore-Gx card on a x86 machine with the
proper driver, set the TILERA_ROOT environment variable and then do:

1. To build:
  TILERA_ROOT=<path_to_MDE> make

2. To run the test:
  TILERA_ROOT=<path_to_MDE> make test_dev
