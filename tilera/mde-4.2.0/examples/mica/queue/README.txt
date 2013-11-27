The programs in this directory demonstrate how to use a queuing
interface to the IPsec acceleration functions on the MiCA shim.  The
example can be modified to work with any MiCA operations.  The queuing
data structures and functions are included in this example directory,
along with example programs that show how to use them.  The file
app.c shows how the queue can be used with the IPI interrupt
mechanism (if the preprocessor macro INTERRUPT_MODEL is defined to be
non-zero) or with the polling mechanism (if it is not).

To build:
  TILERA_ROOT=<path_to_MDE> make

To run the polling example under the simulator:
  TILERA_ROOT=<path_to_MDE> make run_sim_poll

To run the interrupt example under the simulator:
  TILERA_ROOT=<path_to_MDE> make run_sim_int
