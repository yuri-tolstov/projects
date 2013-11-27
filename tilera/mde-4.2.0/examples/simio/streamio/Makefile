# This is a -*- Makefile -*-

# Default target.
all:

FROM ?= .

# Get compilation rules and rules for common files.
include $(FROM)/../common/Makefile.in


# Grab dependencies, if they exist.
-include remote.d tile.d

remote: $(FROM)/remote.cc $(COMMON_OBJ_FILES)
	g++ -o $@ $(CPPFLAGS) $(LDFLAGS) $^

tile: $(FROM)/tile.c
	$(CC) -o $@ $(CFLAGS) $(LDFLAGS) $(XFLAGS) -static $^ -lgxio -ltmc

BUILD_TARGETS := remote tile

all: $(BUILD_TARGETS)


SOCKET_TOKEN := foo
SHIM_ARGS := trio0:mac=0:listen=$(SOCKET_TOKEN)
#SHIM_ARGS := $(SHIM_ARGS):trace_mask=0xfff


.PHONY: run_sim
run_sim: $(BUILD_TARGETS)
	$(PARALLELIZE) --timeout 300 \
	  -++- $(TILE_MONITOR) --image 4x4 \
	    --functional \
	    --sim-args -+- --extra-shim-args $(SHIM_ARGS) -+- \
	    --huge-pages 4 \
	    --upload tile tile \
	    -- tile \
	  -++- ./remote $(SOCKET_TOKEN)

# test_sim is identical to run_sim, except it saves the output.
.PHONY: test_sim
test_sim: $(BUILD_TARGETS)
	$(PARALLELIZE) --timeout 300 --output $@.out \
	  -++- $(TILE_MONITOR) --image 4x4 \
	    --functional \
	    --sim-args -+- --extra-shim-args $(SHIM_ARGS) -+- \
	    --huge-pages 4 \
	    --upload tile tile \
	    -- tile \
	  -++- ./remote $(SOCKET_TOKEN)

clean:
	rm -f *.d *.o $(BUILD_TARGETS)

.PHONY: all clean 
