CC	:= gcc
SRCDIR	:= src
OBJDIR	:= obj
LIBDIR	:= lib

SRC	:= $(wildcard src/*.c)
_OBJS 	:= $(patsubst src/%.c, %.o, $(SRC))
OBJS  	:= $(addprefix $(OBJDIR)/, $(_OBJS))
OBJS 	+= $(OBJDIR)/curl-websocket.o

CONCORD_DLIB	= $(LIBDIR)/libconcord.so

LIBCURL_CFLAGS	:= $(shell pkg-config --cflags libcurl)
LIBCURL_LDFLAGS	:= $(shell pkg-config --libs libcurl)

LIBUV_CFLAGS	:= $(shell pkg-config --cflags libuv)
LIBUV_LDFLAGS 	:= $(shell pkg-config --libs libuv)

LIBS_CFLAGS	:= $(LIBCURL_CFLAGS) $(LIBUV_CFLAGS) -Iinclude
LIBS_LDFLAGS 	:= $(LIBCURL_LDFLAGS) $(LIBUV_LDFLAGS) -ljscon

# @todo create specific CFLAGS for debugging
CFLAGS := -Wall -Werror -Wextra -pedantic -fPIC -O2 -g

.PHONY : all mkdir install clean purge

all : mkdir $(OBJS) $(CONCORD_DLIB)

mkdir :
	mkdir -p $(OBJDIR) $(LIBDIR)

$(OBJDIR)/%.o : $(SRCDIR)/%.c
	$(CC) $(CFLAGS) $(LIBS_CFLAGS) \
	      -c $< -o $@ $(LIBS_LDFLAGS)

$(OBJDIR)/curl-websocket.o :
	$(MAKE) -C $(SRCDIR)/curl-websocket

$(CONCORD_DLIB) :
	$(CC) $(LIBS_CFLAGS) \
	      $(OBJS) -shared -o $(CONCORD_DLIB) $(LIBS_LDFLAGS)

install : all
	cp $(INCL_DIR)/* /usr/local/include
	cp $(CONCORD_DLIB) /usr/local/lib && \
	ldconfig

clean :
	rm -rf $(OBJDIR)

purge : clean
	rm -rf $(LIBDIR)
	$(MAKE) -C test clean
