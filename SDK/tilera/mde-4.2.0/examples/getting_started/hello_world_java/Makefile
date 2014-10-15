# =============================================================================
# Makefile
# =============================================================================

# -----------------------------------------------------------------------------
# Definitions
# -----------------------------------------------------------------------------

# Useful commands
RM=rm -f
MKDIR=mkdir -p
SET_AND_MKDIR=set -e; mkdir -p $(@D)

# source/build directories
SRC_DIR=src
BIN_DIR=bin

# main class
MAIN=com.tilera.examples.HelloWorld

# Java source
JAVA_SRCS= \
  com/tilera/examples/HelloWorld.java

# Java classes to build
CLASSES=$(JAVA_SRCS:%.java=$(BIN_DIR)/%.class)

# Attempt to locate java if JAVA_HOME is not set
ifndef JAVA_HOME
  JAVAC_FOUND=`ls -1 /usr/bin/javac`
  ifneq ("$(JAVAC_FOUND)","")
    JAVA_HOME=/usr
  else
    JAVA_HOME=
  endif
endif

# Java tools
JAVAC=$(JAVA_HOME)/bin/javac
JAVA=$(JAVA_HOME)/bin/java
JDB=$(JAVA_HOME)/bin/jdb

# Java compiler options
JOPTS=-g

# displays options for JDWP agent, then exits
JVM_DEBUG_HELP=-agentlib:jdwp=help

# configures JVM to listen at startup for a debugger connection at specified hostname:port
JVM_DEBUG_LISTEN_PORT=-agentlib:jdwp=server=y,suspend=y,transport=dt_socket,address


# -----------------------------------------------------------------------------
# Rules
# -----------------------------------------------------------------------------

# --- Top-level rules ---

default: all

all: $(BIN_DIR)/stamp

run: $(BIN_DIR)/stamp
	$(JAVA) -cp $(BIN_DIR) $(MAIN)

debug_help:
	$(JAVA) $(JVM_DEBUG_HELP)

debug: $(BIN_DIR)/stamp
	$(JAVA) $(JVM_DEBUG_LISTEN_PORT)=8000 \
	  -cp $(BIN_DIR) $(MAIN)

jdb:
	$(JDB) -attach localhost:8000

# Note: this is only an example of how to configure OProfile

# Also, this assumes you have created a Linux user/group
# named "oprofile", for example by:
#   cat /etc/passwd | grep oprofile -- to see if it exists
#   useradd -U oprofile
#   passwd oprofile

profile_init:
	set -e; \
	opcontrol --init; \
	opcontrol --no-vmlinux; \
	opcontrol --event=ONE:12000000; \
	opcontrol --separate=all; \
	opcontrol --callgraph=20; \
	opcontrol --status

profile_start:
	set -e; \
	opcontrol --reset; \
	opcontrol --start	

profile:
	$(JAVA) -XX:-Inline -agentlib:jvmti_oprofile \
	  -cp $(BIN_DIR) $(MAIN)

profile_stop:
	set -e; \
	opcontrol --stop; \
	opcontrol --dump; \
	opcontrol --shutdown

# Note: we add a sleep here only so 'make clean'
# invocation is visible in Eclipse console
clean:
	rm -rf $(BIN_DIR)
	sleep 1

# Removes IDE-created project files
clean_project_files:
	rm -rf $(BIN_DIR)
	rm -rf .classpath .externalToolBuilders .project .tile_eclipse_preferences.xml

show:
	@echo ""
	@echo "JAVA_HOME   = $(JAVA_HOME)"
	@echo "JAVAC       = $(JAVAC)"
	@echo "JAVA        = $(JAVA)"
	@echo ""
	@echo "JAVA_SRCS   = $(JAVA_SRCS)"
	@echo "CLASSES     = $(CLASSES)"
	@echo ""

# --- Build rules ---

$(BIN_DIR)/stamp: $(CLASSES) $(JNI_HDRS) $(JNI_OBJS) $(NATIVE_LIB)
	touch $@

$(BIN_DIR)/%.class: $(SRC_DIR)/%.java
	$(SET_AND_MKDIR); \
	$(JAVAC) $(JOPTS) -d $(BIN_DIR) -cp $(BIN_DIR) -sourcepath $(SRC_DIR) $<
