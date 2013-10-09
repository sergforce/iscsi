CC=gcc
CFLAGS=-Wall
LDFLAGS=-mwindows -lcomctl32 -lshlwapi -lgdi32
WINDRES=windres

#define DEBUG 
#1
#endef

OS=$(shell uname)
ARCH=$(shell uname -m)

OBJ=$(CURDIR)

ifdef DEBUG
OBJDIR=$(OBJ)/$(OS)/obj-$(ARCH)-debug
CFLAGS+=-g -D_DEBUG -D_DEBUG2
LDFLAGS+=-g
else
OBJDIR=$(OBJ)/$(OS)/obj-$(ARCH)
CFLAGS+=-O3 -fomit-frame-pointer -funroll-all-loops
endif


#directories
DEBUG_DIR=../daemon/debug
OSSPEC_DIR=../daemon/$(OS)

INCLUDE=-I$(PWD) -I$(PWD)/$(DEBUG_DIR) -I$(PWD)/$(OSSPEC_DIR) -I$(PWD)/../daemon/
CFLAGS+= $(INCLUDE)

TOPDIR=
DDIR=../daemon
BASEDIR=/
export BASEDIR
#std files

.SUFFIXES:

INIT_FILES=init.c \
	   exwin.h \
	   resource.h \
	   $(DDIR)/dcirlist.c \
	   $(DDIR)/iscsi_mem.c \
	   $(DDIR)/conf_reader.c \
	   $(DDIR)/Win32/conf_win.c \
           $(SPEC_FILES)

INIT_SRC=$(INIT_FILES:%.h=)
INIT_OBJS=$(INIT_SRC:%.c=$(OBJDIR)/%.o)

.PHONY: clean dirs distclean

init: dirs init_res.o $(OBJDIR)/init

init_res.o:  init.rc init.rc2 init.exe.manifest
	@echo " [RES] $@"
	@$(WINDRES) -o $(OBJDIR)/$@ -i init.rc
	


$(INIT_OBJS): Makefile

$(OBJDIR)/init: $(INIT_OBJS) $(OBJDIR)/init_res.o
	@echo " [LD]  $@"
	@$(CC) -o $@ $^ $(LDFLAGS)

clean:
	@rm -rf $(OBJDIR)/*.o
	@rm -rf $(OBJDIR)/../daemon/*.o
	@rm -rf $(OBJDIR)/../daemon/Win32/*.o
	@echo "Removed intermediate files"

distclean:
	@rm -Rf $(OBJDIR)

$(OBJDIR)/%.o : %.c
	@echo " [CC]  $*.c"
	@$(CC) $(CFLAGS) -MMD -c -o $(OBJDIR)/$*.o $*.c	
	@if [ ! -f $(OBJDIR)/$*.d -a -f $*.d ]; then \
	    printf "$(OBJDIR)/" > $(OBJDIR)/$*.d; \
	    cat $*.d >> $(OBJDIR)/$*.d && rm -f $*.d; \
	fi
	@cp -f $(OBJDIR)/$*.d $(OBJDIR)/$*.dep
	@sed -e 's/#.*//' -e 's/^.*: *//' -e 's/ *\\$$//' -e 's/^ *//' -e '/^$$/ d' -e 's/$$/:/' \
		    < $(OBJDIR)/$*.d >> $(OBJDIR)/$*.dep

 
dirs:
	@mkdir -p $(OBJDIR)
	@mkdir -p $(OBJDIR)/../daemon
	@mkdir -p $(OBJDIR)/../daemon/Win32
