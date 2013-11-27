This example shows how to use a software queue to reorder packets coming 
from mPIPE.  This allows a single flow to be processed round-robin on 
different workers without losing the original packet order.

It initializes mPIPE.  It requests all incoming packets (but drops
jumbo packets).  It creates multiple threads to pull in packets and
forward them.  The example forwards any number of links using a 
configurable-size pool of workers.   The packets are forwarded out the link
on which they were received.  But this could easily be changed to be a lookup
table to perform L2/3 forwarding.  

The reorder queue can be sized based on the application requirements and 
additional services can be added either in the worker or in the send-function.
Code added to the worker should generally provide an upper bound on latency 
to prevent blocking thus worker code is usually used to identify the egress 
port and potentially QoS etc.  Code added on the "send" side of the software 
queue can have longer/unbounded latency without blocking thus traffic 
management application code will generally be implemented on the SEND side.

See the Application Libraries Reference Guide for more information
about the APIs used in this example.

To build:
  TILERA_ROOT=<path_to_MDE> make

To run under the simulator:
  TILERA_ROOT=<path_to_MDE> make run_sim
