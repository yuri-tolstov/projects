The following program demonstrates the use of DMA using the MiCA shim.
It is a good introduction to the use of the low-level gxio_mica_*
interface, including how to register memory with the shim.

See the Application Libraries Reference Guide for more information
about the APIs used in this example.

To build:
  TILERA_ROOT=<path_to_MDE> make

To run under the simulator:
  TILERA_ROOT=<path_to_MDE> make run_sim
