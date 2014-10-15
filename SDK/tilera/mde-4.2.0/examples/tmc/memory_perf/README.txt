This program demonstrates the allocation of memory with different
memory controller and cache homing properties. It also measures the
performance of the different memory types in a variety of situations.

See the Application Libraries Reference Guide for more information
about the APIs used in this example.

To build:
  TILERA_ROOT=<path_to_MDE> make

To run on hardware:
  TILERA_ROOT=<path_to_MDE> make run_dev

To run natively (assuming "--hvx STRIPE_MEMORY=never"):
  make run
