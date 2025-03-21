#
#  UNPUBLISHED PROPRIETARY SOURCE CODE
#  Copyright (c) 2016 MXCHIP Inc.
#
#  The contents of this file may not be disclosed to third parties, copied or
#  duplicated in any form, in whole or in part, without the prior written
#  permission of MXCHIP Corporation.
#

export MAKEFILES_PATH := $(MICO_OS_PATH)/makefiles

# Some possible inputs into this file
#RTOS        : FreeRTOS, ThreadX
#NETWORK     : LwIP, NetX_Duo, NetX
#HOST_ARCH   : ARM_CM3, ARM_CM4
#NAME        : BESL, Gedday, uSSL, ThreadX, NetX_Duo
#SOURCE_ROOT : ../../../    (needs trailing slash)
#DEBUG       : 1

all: stripped_lib

$(info $(filter wipe_source_for_test,$(MAKECMDGOALS)))
ifeq ($(filter wipe_source_for_test,$(MAKECMDGOALS)),)

LIBRARY_NAME :=$(NAME)
ifndef LIBRARY_NAME
$(error LIBRARY_NAME not defined)
endif

ifdef RTOS
LIBRARY_NAME :=$(LIBRARY_NAME).$(RTOS)
endif
ifdef NETWORK
LIBRARY_NAME:=$(LIBRARY_NAME).$(NETWORK)
endif

ifndef HOST_ARCH
$(error HOST_ARCH not defined - needed to include correct toolchain)
endif

LIBRARY_NAME:=$(LIBRARY_NAME).$(HOST_ARCH)

LIBRARY_OUTPUT_DIR ?= .

CC :=

include $(MICO_OS_PATH)/makefiles/micoder_host_cmd.mk
include $(MICO_OS_PATH)/makefiles/micoder_toolchain_GCC.mk

ifndef CC
$(error No matching toolchain found for architecture $(HOST_ARCH))
endif

LIBRARY_NAME:=$(LIBRARY_NAME).$(TOOLCHAIN_NAME)

# Add a option for pre-built library to turn of library poison.
# This is useful for adding a new library which uses poisoned functions extensively.
# Over time, these function calls in the library can be gradually phased out.
ifdef BYPASS_LIBRARY_POISON_CHECK
LIBRARY_POISON_H_INCLUSION :=
else
LIBRARY_POISON_H_INCLUSION := -include library_poison.h
endif

MAKEFILES_DIR := $(TOOLS_ROOT)$(SOURCE_ROOT)/tools/makefiles

# NOTE: The system builds each object twice - once with built-in functions disabled and bad functions poisoned to catch uses of these functions,
#       Then again with built-in functions re-enabled to allow optimisations

$(SOURCE_ROOT)build/$(NAME)/%.o: %.c $(MAKEFILES_PATH)/mico_library_makefile.mk
	$(QUIET)$(call MKDIR, $(dir $@))
	$(QUIET)$(ECHO) Compiling $<
	$(QUIET)$(CC) -I $(SOURCE_ROOT)/tools/makefiles -fno-builtin $(LIBRARY_POISON_H_INCLUSION) $(CFLAGS) -o $@ $<
	$(QUIET)$(CC) $(CFLAGS) -o $@ $<

$(SOURCE_ROOT)build/$(NAME)/%.o: %.cpp $(MAKEFILES_PATH)/mico_library_makefile.mk
	$(QUIET)$(call MKDIR, $(dir $@))
	$(QUIET)$(ECHO) Compiling $<
	$(QUIET)$(CC) $(CFLAGS) -o $@ $<
	$(QUIET)$(CXX) -I $(SOURCE_ROOT)/tools/makefiles $(LIBRARY_POISON_H_INCLUSION) -fno-builtin $(CFLAGS) -o $@ $<
	$(QUIET)$(CXX) $(CFLAGS) -o $@ $<

$(SOURCE_ROOT)build/$(NAME)/%.o: %.S $(MAKEFILES_PATH)/mico_library_makefile.mk
	$(QUIET)$(call MKDIR, $(dir $@))
	$(QUIET)$(ECHO) Assembling $<
	$(QUIET)$(CC) $(CFLAGS) -o $@ $<


# MICO pre-built library defines
CFLAGS += -DMiCO_PREBUILT_LIBS -DMiCO_SDK_VERSION_MAJOR=3

ifdef DEBUG
CFLAGS += -DDEBUG
ifndef ALWAYS_OPTIMISE
CFLAGS += -O0
else
CFLAGS += -Os
endif
LIBRARY_NAME :=$(LIBRARY_NAME).debug
else
CFLAGS += -Os -DNDEBUG
LIBRARY_NAME :=$(LIBRARY_NAME).release
endif

ASMFLAGS += $(CPU_ASMFLAGS)


#platform_assert
CFLAGS += -I$(SOURCE_ROOT)platform \
          -I$(SOURCE_ROOT)platform/$(HOST_ARCH)
          
          
CFLAGS += -DMICO_PREBUILT_LIBS 

OBJS := $(addprefix $(SOURCE_ROOT)build/$(NAME)/,$(filter %.o,$(SOURCES:.cpp=.o) $(SOURCES:.c=.o) $(SOURCES:.S=.o)))

$(SOURCE_ROOT)build/$(NAME)/$(LIBRARY_NAME).a: $(OBJS)
	$(QUIET)$(RM) $@
	$(if $(wordlist 1,50,     $(OBJS)),$(QUIET)$(AR) -rcs $@ $(wordlist 1,50,     $(OBJS)))
	$(if $(wordlist 51,100,   $(OBJS)),$(QUIET)$(AR) -rcs $@ $(wordlist 51,100,   $(OBJS)))
	$(if $(wordlist 101,150,  $(OBJS)),$(QUIET)$(AR) -rcs $@ $(wordlist 101,150,  $(OBJS)))
	$(if $(wordlist 151,200,  $(OBJS)),$(QUIET)$(AR) -rcs $@ $(wordlist 151,200,  $(OBJS)))
	$(if $(wordlist 201,250,  $(OBJS)),$(QUIET)$(AR) -rcs $@ $(wordlist 201,250,  $(OBJS)))
	$(if $(wordlist 251,300,  $(OBJS)),$(QUIET)$(AR) -rcs $@ $(wordlist 251,300,  $(OBJS)))
	$(if $(wordlist 301,350,  $(OBJS)),$(QUIET)$(AR) -rcs $@ $(wordlist 301,350,  $(OBJS)))
	$(if $(wordlist 351,400,  $(OBJS)),$(QUIET)$(AR) -rcs $@ $(wordlist 351,400,  $(OBJS)))
	$(if $(wordlist 401,450,  $(OBJS)),$(QUIET)$(AR) -rcs $@ $(wordlist 401,450,  $(OBJS)))
	$(if $(wordlist 451,500,  $(OBJS)),$(QUIET)$(AR) -rcs $@ $(wordlist 451,500,  $(OBJS)))
	$(if $(wordlist 501,550,  $(OBJS)),$(QUIET)$(AR) -rcs $@ $(wordlist 501,550,  $(OBJS)))
	$(if $(wordlist 551,600,  $(OBJS)),$(QUIET)$(AR) -rcs $@ $(wordlist 551,600,  $(OBJS)))
	$(if $(wordlist 601,650,  $(OBJS)),$(QUIET)$(AR) -rcs $@ $(wordlist 601,650,  $(OBJS)))
	$(if $(wordlist 651,700,  $(OBJS)),$(QUIET)$(AR) -rcs $@ $(wordlist 651,700,  $(OBJS)))
	$(if $(wordlist 701,750,  $(OBJS)),$(QUIET)$(AR) -rcs $@ $(wordlist 701,750,  $(OBJS)))
	$(if $(wordlist 751,1000, $(OBJS)),$(QUIET)$(AR) -rcs $@ $(wordlist 751,1000, $(OBJS)))


stripped_lib: $(SOURCE_ROOT)build/$(NAME)/$(LIBRARY_NAME).a $(MAKEFILES_PATH)/mico_library_makefile.mk
ifdef NO_STRIP_LIBS
	$(QUIET)$(CP) $(SOURCE_ROOT)/build/$(NAME)/$(LIBRARY_NAME).a $(LIBRARY_OUTPUT_DIR)/$(LIBRARY_NAME).a
else
	$(QUIET)$(STRIP) -g -o $(LIBRARY_OUTPUT_DIR)/$(LIBRARY_NAME).a $(SOURCE_ROOT)/build/$(NAME)/$(LIBRARY_NAME).a
endif
	$(QUIET)$(RM) -rf $(SOURCE_ROOT)/build/$(NAME)
	$(QUIET)$(ECHO) Make $(LIBRARY_NAME).a DONE

clean:
	$(QUIET)$(RM) -rf $(LIBRARY_OUTPUT_DIR)/$(NAME)*.a

else

ifeq ($(HOST_OS),Win32)
LIB_TOOLS_DIR := $(subst /,\,/pf8_3/MinGW/msys/1.0/mingw/bin/)
EXECUTABLE_SUFFIX  := .exe
endif

LIB_RM    := $(LIB_TOOLS_DIR)rm$(EXECUTABLE_SUFFIX)
LIB_RMDIR := $(LIB_TOOLS_DIR)rmdir$(EXECUTABLE_SUFFIX)
LIB_XARGS := $(LIB_TOOLS_DIR)xargs$(EXECUTABLE_SUFFIX)
LIB_FIND := $(LIB_TOOLS_DIR)find$(EXECUTABLE_SUFFIX)

wipe_source_for_test:
	cd $(KEEP_LIST_DIR) && $(LIB_FIND) . -type f -a -not \( $(foreach SPEC,$(KEEP_LIST), -path "./$(SPEC)" -o ) -false \) -print0 | $(LIB_XARGS) -0 $(LIB_RM)
	cd $(KEEP_LIST_DIR) && $(LIB_FIND) . -type d -empty -print0 | $(LIB_XARGS) -0 --no-run-if-empty $(LIB_RMDIR)

endif