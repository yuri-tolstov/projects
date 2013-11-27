These examples show how to forward packets using mPIPE.

Each example initializes mPIPE. It requests all incoming packets (but
drops jumbo packets).  It creates multiple threads to pull in packets
and forward them.

See the Application Libraries Reference Guide for more information
about the APIs used in the examples.

To build:
  TILERA_ROOT=<path_to_MDE> make

Example 1: app.c, forward packets using one mPIPE instance.
To run under the simulator:
  TILERA_ROOT=<path_to_MDE> make run_sim

by default, it pulls ingress packet from the mac0 of mPIPE instance #0
and fowards them thorugh same mac.

Example 2: app_gx8072.c, forward packets using links across two mPIPE
instances.

To run under the simulator:
   TILERA_ROOT=<path_to_MDE> make run_sim_gx8072

by default, it pulls ingress packets from the mac0 of mPIPE instance
#0 and forwards them through the mac0 of mPIPE instance #1. At the
first run, it will spend 1/2 hour to generate the simulator image:
gx8072-dataplane.image.

To regenerate gx8072-dataplane.image:
    TILERA_ROOT=<path_to_MDE> make gx8072_image

Note: This example also shows the remote buffer return across two
mPIPE instances when built with GXIO_MULTI_MPIPES.


