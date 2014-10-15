#-------------------------------------------------------------------------------
# Build environment check
#-------------------------------------------------------------------------------
ifndef DS11PROJECT
$(error "DS11PROJECT is not setup")
endif
PROJLIST := npu n804 tilegx
ifeq ($(findstring $(DS11PROJECT),$(PROJLIST)),)
$(error "DS11PROJECT is not one of these: $(PROJLIST)")
endif
ifndef DS11VIEW_ROOT
$(error "DS11VIEW_ROOT is not setup")
endif
$(info "$(DS11VIEW_ROOT) $(DS11ENV_ROOT)")
ifneq ($(DS11VIEW_ROOT)/env,$(DS11ENV_ROOT))
$(error "Build environment doesn't belong to the MAN Project")
endif
ifndef DS11BUILD_ROOT
$(error "DS11BUILD_ROOT is not setup")
endif
ifneq ($(DS11BUILD_ROOT),$(DS11VIEW_ROOT)/build)
$(error "DS11BUILD_ROOT is not properly setup")
endif
ifndef DS11INSTALL_ROOT
$(error "DS11INSTALL_ROOT is not setup")
endif
ifneq ($(DS11INSTALL_ROOT),$(DS11VIEW_ROOT)/install)
$(error "DS11BUILD_INSTALL is not properly setup")
endif
