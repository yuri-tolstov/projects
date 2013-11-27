# Default target.
all:

ifndef TILERA_ROOT
$(error The 'TILERA_ROOT' environment variable is not set.)
endif


test_dev:
	echo "WARNING: This example cannot be tested on hardware."


# Test two simulators pinging each other once.
#
# HACK: We run a final "true" process to help ensure we process all
# the output from the processes before shutting down.
#

# Use "ACCURATE=1" to avoid "--functional".
ACCURATE ?=

COMMON := --image gx8036 $(if $(ACCURATE),,--functional) --console

LINK0 := xgbe0
ESA0 := mpipe0:mac=0:peer=mpipe0/1:bt-on-drop
IP0 := 192.168.100.200
HW0 := 00:1A:CA:BE:00:00

LINK1 := gbe1
ESA1 := mpipe0:mac=1:peer=mpipe0/0:bt-on-drop
IP1 := 192.168.100.201
HW1 := 00:1A:CA:BE:01:01

run_sim:
	socket_key="ping/$$$$"; \
	$(TILERA_ROOT)/bin/parallelize \
	  -++- \
	  $(TILERA_ROOT)/bin/tile-monitor \
	    --peer-listen "@$$socket_key" \
	    $(COMMON) \
	    --sim-args -+- --extra-shim-args "$(ESA0)" -+- \
	    --run -+- ifconfig $(LINK0) $(IP0) hw ether $(HW0) -+- \
	    --run -+- arp -i $(LINK0) -s $(IP1) $(HW1) -+- \
	    --peer-barrier \
	    --run -+- ifconfig $(LINK0) up -+- \
	    --peer-barrier \
	    --run -+- ping -c 1 $(IP1) -+- \
	    --run -+- true -+- \
	    --quit \
	  -++- \
	  $(TILERA_ROOT)/bin/tile-monitor \
	    --peer-connect "@$$socket_key" \
	    $(COMMON) \
	    --sim-args -+- --extra-shim-args "$(ESA1)" -+- \
	    --run -+- ifconfig $(LINK1) $(IP1) hw ether $(HW1) -+- \
	    --run -+- arp -i $(LINK1) -s $(IP0) $(HW0) -+- \
	    --peer-barrier \
	    --run -+- ifconfig $(LINK1) up -+- \
	    --peer-barrier \
	    --run -+- ping -c 1 $(IP0) -+- \
	    --run -+- true -+- \
	    --quit; \

test_sim: run_sim
	echo "Successful test."

.PHONY: test_dev run_sim test_sim
