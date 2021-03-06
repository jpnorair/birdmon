CC := gcc
LD := ld

SUBAPP      := cmds
BIRDMON_PKG   ?=
BIRDMON_DEF   ?= 
BIRDMON_INC   ?=
BIRDMON_LIB   ?= 
BIRDMON_OSCFLAGS ?= 

ifneq ($(EXT_DEBUG),0)
	ifeq ($(EXT_DEBUG),1)
		CFLAGS  ?= -std=gnu99 -Og -g -Wall $(BIRDMON_OSCFLAGS) -pthread -D__DEBUG__
	else
		CFLAGS  ?= -std=gnu99 -O2 -Wall $(BIRDMON_OSCFLAGS) -pthread -D__DEBUG__
	endif
else 
	CFLAGS      ?= -std=gnu99 -O3 $(BIRDMON_OSCFLAGS) -pthread
endif

BUILDDIR    := ../$(BIRDMON_BLD)

SUBAPPDIR   := .
SRCEXT      := c
DEPEXT      := d
OBJEXT      := o
LIB         := $(BIRDMON_LIB)
LIBINC      := $(subst -L./,-L./../,$(BIRDMON_LIBINC))
INC			:= $(subst -I./,-I./../,$(BIRDMON_INC)) -I./../test
INCDEP      := $(INC)

SOURCES     := $(shell find . -type f -name "*.$(SRCEXT)")
OBJECTS     := $(patsubst ./%,$(BUILDDIR)/%,$(SOURCES:.$(SRCEXT)=.$(OBJEXT)))


all: resources $(SUBAPP)
obj: $(OBJECTS)
remake: cleaner all


#Copy Resources from Resources Directory to Target Directory
resources: directories

#Make the Directories
directories:
	@mkdir -p $(SUBAPPDIR)
	@mkdir -p $(BUILDDIR)

#Clean only Objects
clean:
	@$(RM) -rf $(BUILDDIR)

#Full Clean, Objects and Binaries
cleaner: clean
	@$(RM) -rf $(SUBAPPDIR)

#Pull in dependency info for *existing* .o files
-include $(OBJECTS:.$(OBJEXT)=.$(DEPEXT))

#Direct build of the test app with objects
$(SUBAPP): $(OBJECTS)
	$(CC) $(INC) $(LIBINC) -o $(SUBAPPDIR)/$(SUBAPP) $^ $(LIB)

#Compile Stages
$(BUILDDIR)/%.$(OBJEXT): ./%.$(SRCEXT)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(BIRDMON_DEF) $(INC) -c -o $@ $<
	@$(CC) $(CFLAGS) $(BIRDMON_DEF) $(INCDEP) -MM ./$*.$(SRCEXT) > $(BUILDDIR)/$*.$(DEPEXT)
	@cp -f $(BUILDDIR)/$*.$(DEPEXT) $(BUILDDIR)/$*.$(DEPEXT).tmp
	@sed -e 's|.*:|$(BUILDDIR)/$*.$(OBJEXT):|' < $(BUILDDIR)/$*.$(DEPEXT).tmp > $(BUILDDIR)/$*.$(DEPEXT)
	@sed -e 's/.*://' -e 's/\\$$//' < $(BUILDDIR)/$*.$(DEPEXT).tmp | fmt -1 | sed -e 's/^ *//' -e 's/$$/:/' >> $(BUILDDIR)/$*.$(DEPEXT)
	@rm -f $(BUILDDIR)/$*.$(DEPEXT).tmp

#Non-File Targets
.PHONY: all remake clean cleaner resources

