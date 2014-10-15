This program shows how to use mpipe rules for the incoming pcap packets
under the simulator. The program sets up multiple mpipe rules including 
port filtering, vlan filtering and mac filtering, combined with different flow
control attributes like round-robin, static flow and dynamic flow. The packet
numbers being processed by each tile with a certain mpipe rule are showed.

See the Application Libraries Reference Guide for more information
about the APIs used in this example.

To build:
  TILERA_ROOT=<path_to_MDE> make

To run under the simulator:
  TILERA_ROOT=<path_to_MDE> make run_sim
