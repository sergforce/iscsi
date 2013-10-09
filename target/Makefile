CC=gcc
CFLAGS=-Wall
LDFLAGS=

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
CFLAGS+=-O2
endif


#directories
CONTRIB_DIR=contrib
DEBUG_DIR=debug
OSSPEC_DIR=$(OS)

export OSSPEC_DIR;
export DEBUG_DIR;
export CONTRIB_DIR;

include $(OS)/Makefile.in

INCLUDE=-I$(PWD) -I$(PWD)/$(CONTRIB_DIR) -I$(PWD)/$(DEBUG_DIR) -I$(PWD)/$(OSSPEC_DIR)
CFLAGS+= $(INCLUDE)

TOPDIR=
BASEDIR=/
export BASEDIR
#std files

.SUFFIXES:

DAEMON_FILES=sock.c \
	     sock.h \
	     dcirlist.c \
	     dcirlist.h \
	     iscsi.h \
	     iscsi_mem.c \
	     iscsi_mem.h \
	     iscsi_param.c \
	     iscsi_param.h \
	     iscsi_session.h \
	     conf_reader.h \
	     conf_reader.c \
	     target_conf.h \
	     tf.h \
	     spec.h \
	     pdu.c \
	     target.c \
	     $(DEBUG_DIR)/iscsi_debug.c \
	     $(DEBUG_DIR)/iscsi_debug.h \
	     $(DEBUG_DIR)/iscsi_comments.h \
	     $(CONTRIB_DIR)/crc32.c \
	     $(CONTRIB_DIR)/crc32.h \
	     $(CONTRIB_DIR)/crc32cr.h \
	     $(SPEC_FILES)

DAEMON_SRC=$(DAEMON_FILES:%.h=)
DAEMON_OBJS=$(DAEMON_SRC:%.c=$(OBJDIR)/%.o)

.PHONY: clean dirs distclean

daemon:	dirs $(OBJDIR)/daemon

$(DAEMON_OBJS): Makefile $(OS)/Makefile.in

$(OBJDIR)/daemon: $(DAEMON_OBJS)
	@echo " [LD]  $@"
	@$(CC) -o $@ $^ $(LDFLAGS)

clean:
	@rm -rf $(OBJDIR)/*.o $(OBJDIR)/$(CONTRIB_DIR)/*.o \
	    $(OBJDIR)/$(DEBUG_DIR)/*.o $(OBJDIR)/$(OSSPEC_DIR)/*.o
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
	@mkdir -p $(OBJDIR)/$(CONTRIB_DIR)
	@mkdir -p $(OBJDIR)/$(DEBUG_DIR)
	@mkdir -p $(OBJDIR)/$(OSSPEC_DIR)


