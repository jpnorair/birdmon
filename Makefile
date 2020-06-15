# Copyright 2020, JP Norair
#
# Licensed under the OpenTag License, Version 1.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.indigresso.com/wiki/doku.php?id=opentag:license_1_0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

CC := gcc
LD := ld

THISMACHINE ?= $(shell uname -srm | sed -e 's/ /-/g')
THISSYSTEM	?= $(shell uname -s)

APP         ?= birdmon
PKGDIR      := ../_hbpkg/$(THISMACHINE)
SYSDIR      := ../_hbsys/$(THISMACHINE)
EXT_DEF     ?= 
EXT_INC     ?= 
EXT_LIBFLAGS ?= 
EXT_LIBS    ?= 
VERSION     ?= 1.0.a

# Try to get git HEAD commit value
ifneq ($(INSTALLER_HEAD),)
    GITHEAD := $(INSTALLER_HEAD)
else
    GITHEAD := $(shell git rev-parse --short HEAD)
endif

ifeq ($(MAKECMDGOALS),debug)
	APPDIR      := bin/$(THISMACHINE)
	BUILDDIR    := build/$(THISMACHINE)_debug
	DEBUG_MODE  := 1
else
	APPDIR      := bin/$(THISMACHINE)
	BUILDDIR    := build/$(THISMACHINE)
	DEBUG_MODE  := 0
endif


# Make sure the LD_LIBRARY_PATH includes the _hbsys directory
ifneq ($(findstring $(SYSDIR)/lib,$(LD_LIBRARY_PATH)),)
	error "$(SYSDIR)/lib not in LD_LIBRARY_PATH.  Please update your settings to include this."
endif

ifeq ($(THISSYSTEM),Darwin)
	OSCFLAGS := -Wno-nullability-completeness -Wno-expansion-to-defined
	OSLIBINC := -L/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/lib
	OSINC := -I/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/include
	LIBBSD :=
    
else ifeq ($(THISSYSTEM),Linux)
	OSCFLAGS := 
	OSLIBINC := 
	OSINC := 
	ifneq ($(findstring OpenWRT,$(THISMACHINE)),)
		LIBBSD := -lfts
	else
		LIBBSD := -lbsd
	endif
else
	LIBBSD :=
endif


DEFAULT_DEF := -DBIRDMON_PARAM_GITHEAD=\"$(GITHEAD)\"
LIBMODULES  := argtable cJSON clithread cmdtab otvar bintex OTEAX libotfs $(EXT_LIBS)
SUBMODULES  := cmds main

SRCEXT      := c
DEPEXT      := d
OBJEXT      := o

CFLAGS_DEBUG?= -std=gnu99 -Og -g -Wall $(OSCFLAGS) -pthread
CFLAGS      ?= -std=gnu99 -O3 $(OSCFLAGS) -pthread
INC         := -I. -I./include -I./$(SYSDIR)/include $(OSINC) $(EXT_INC)
INCDEP      := -I.
LIBINC      := -L./$(SYSDIR)/lib $(OSLIBINC) $(EXT_LIB) 
LIB         := -largtable -lbintex -lcJSON -lclithread -lcmdtab -lotvar -ltalloc -lccronexpr -lcurl -lm -lc $(LIBBSD)

BIRDMON_OSCFLAGS:= $(OSCFLAGS)
BIRDMON_PKG   := $(PKGDIR)
BIRDMON_DEF   := $(DEFAULT_DEF) $(EXT_DEF)
BIRDMON_INC   := $(INC) $(EXT_INC)
BIRDMON_LIBINC:= $(LIBINC)
BIRDMON_LIB   := $(EXT_LIBFLAGS) $(LIB)
BIRDMON_BLD   := $(BUILDDIR)
BIRDMON_APP   := $(APPDIR)


# Export the following variables to the shell: will affect submodules
export BIRDMON_OSCFLAGS
export BIRDMON_PKG
export BIRDMON_DEF
export BIRDMON_LIBINC
export BIRDMON_INC
export BIRDMON_LIB
export BIRDMON_BLD
export BIRDMON_APP

deps: $(LIBMODULES)
all: release
release: directories $(APP)
debug: directories $(APP).debug
obj: $(SUBMODULES)
pkg: deps all install
remake: cleaner all


install: 
	@rm -rf $(PKGDIR)/$(APP).$(VERSION)
	@mkdir -p $(PKGDIR)/$(APP).$(VERSION)
	@cp $(APPDIR)/$(APP) $(PKGDIR)/$(APP).$(VERSION)/
	@rm -f $(PKGDIR)/$(APP)
	@ln -s $(APP).$(VERSION) ./$(PKGDIR)/$(APP)
	cd ../_hbsys && $(MAKE) sys_install INS_MACHINE=$(THISMACHINE) INS_PKGNAME=birdmon

directories:
	@mkdir -p $(APPDIR)
	@mkdir -p $(BUILDDIR)

# Clean only this machine
clean:
	@$(RM) -rf $(BUILDDIR)
	@$(RM) -rf $(APPDIR)

# Clean all builds
cleaner: 
	@$(RM) -rf ./build
	@$(RM) -rf ./bin

#Linker
$(APP): $(SUBMODULES) 
	$(eval OBJECTS := $(shell find $(BUILDDIR) -type f -name "*.$(OBJEXT)"))
	$(CC) $(CFLAGS) $(BIRDMON_DEF) $(BIRDMON_INC) $(BIRDMON_LIBINC) -o $(APPDIR)/$(APP) $(OBJECTS) $(BIRDMON_LIB)

$(APP).debug: $(SUBMODULES)
	$(eval OBJECTS_D := $(shell find $(BUILDDIR) -type f -name "*.$(OBJEXT)"))
	$(CC) $(CFLAGS_DEBUG) $(BIRDMON_DEF) -D__DEBUG__ $(BIRDMON_INC) $(BIRDMON_LIBINC) -o $(APPDIR)/$(APP).debug $(OBJECTS_D) $(BIRDMON_LIB)

#Library dependencies (not in birdmon sources)
$(LIBMODULES): %: 
#	cd ./../$@ && $(MAKE) lib && $(MAKE) install
	cd ./../$@ && $(MAKE) pkg

#birdmon submodules
$(SUBMODULES): %: directories
	cd ./$@ && $(MAKE) -f $@.mk obj EXT_DEBUG=$(DEBUG_MODE)

#Non-File Targets
.PHONY: deps all release debug obj pkg remake install directories clean cleaner

