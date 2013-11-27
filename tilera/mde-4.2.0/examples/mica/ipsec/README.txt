The following program demonstrates how to use the general IPsec
acceleration functions and specifically ESP/IPv4/tunneling inbound and
outbound packet processing on the MiCA shim, using the gxcr library.
It shows how to register memory with the shim, how to create
gxcr_ipsec_sa_t data structures for IPsec acceleration, to prepare
keys and other data, how to perform the operation and how to access
the results.

See the Application Libraries Reference Guide for more information
about the APIs used in this example.

To build:
  TILERA_ROOT=<path_to_MDE> make

To run under the simulator:
  TILERA_ROOT=<path_to_MDE> make run_sim
