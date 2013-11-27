This program demonstrates the "dataplane" feature provided by Tilera
Linux.  Dataplane CPUs can be designated at boot time via Linux boot
parameters.  An application can then bind its tasks to dataplane CPUs
and "Zero Overhead Linux" will minimize system overhead by disabling
kernel timer ticks so long as there is one task per dataplane cpu and
the dataplane tasks do not invoke the kernel.

To build:
  TILERA_ROOT=<path_to_MDE> make

To run on hardware:
  TILERA_ROOT=<path_to_MDE> make run_dev

You may validate the expected output using the "test_dev" target instead
of "run_dev".

The test runs under simulation (via the "run_sim" target), but takes a
long time to complete.  You can validate the result of the simulation
run with the "test_sim_slow" target.

To run natively (assuming "--hvx dataplane=1-999"):
  make run
