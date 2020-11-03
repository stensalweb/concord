CC 	:= gcc
SRCDIR 	:= src
OBJDIR 	:= obj
INCLDIR := include
LIBDIR 	:= lib

SRC   := $(wildcard src/*.c)
_OBJS := $(patsubst src/%.c, %.o, $(SRC))
OBJS  := $(addprefix $(OBJDIR)/, $(_OBJS))
OBJS += $(OBJDIR)/curl-websocket.o

CONCORD_DLIB = $(LIBDIR)/libconcord.so

# @todo create specific CFLAGS for debugging
CFLAGS := -Wall -Werror -Wextra -pedantic -fPIC \
	  -O2 -g -I$(INCLDIR)

LDLIBS  := -L$(LIBDIR) -ljscon -lcurl -luv
LDFLAGS := "-Wl,-rpath,$(LIBDIR)" 

.PHONY : all mkdir install clean purge

all : mkdir $(OBJS) $(CONCORD_DLIB)

mkdir :
	mkdir -p $(OBJDIR) $(LIBDIR)

$(OBJDIR)/%.o : $(SRCDIR)/%.c
	$(CC) -c $< -o $@ $(CFLAGS)

$(OBJDIR)/curl-websocket.o :
	$(MAKE) -C $(SRCDIR)/curl-websocket

$(CONCORD_DLIB) :
	$(CC) $(OBJS) -shared -o $(CONCORD_DLIB) $(LDLIBS)

install : all
	cp $(INCL_DIR)/* /usr/local/include
	cp $(CONCORD_DLIB) /usr/local/lib && \
	ldconfig

clean :
	rm -rf $(OBJDIR)

purge : clean
	rm -rf $(LIBDIR)
	$(MAKE) -C test clean
