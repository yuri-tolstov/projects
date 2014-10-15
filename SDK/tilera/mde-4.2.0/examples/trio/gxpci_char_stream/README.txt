Introduction
------------

This example shows how to open a character stream connection and transfer
data between a tile application and a x86 host application using the Gx PCIe
legacy character stream API. 

The source is compiled on both the host and TILEGx platforms, with conditional
compilation used to change the few lines that need to be different.

Running The Example
-------------------

To run the example, install a TILEncore-Gx card on a x86 machine with the
proper driver, set the TILERA_ROOT environment variable and then do:

1. To build:
  TILERA_ROOT=<path_to_MDE> make

2. To run the test:
  TILERA_ROOT=<path_to_MDE> make test_dev
