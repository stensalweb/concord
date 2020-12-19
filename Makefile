SRCDIR	:= src
OBJDIR	:= obj
INCLDIR	:= include
LIBDIR	:= lib

SRC	:= $(wildcard src/*.c)
_OBJS 	:= $(patsubst src/%.c, %.o, $(SRC))
OBJS  	:= $(addprefix $(OBJDIR)/, $(_OBJS))
OBJS 	+= $(OBJDIR)/curl-websocket.o

CONCORD_DLIB	= $(LIBDIR)/libconcord.so
CONCORD_SLIB	= $(LIBDIR)/libconcord.a

#LIBCURL_CFLAGS	:= $(shell pkg-config --cflags libcurl)
#LIBCURL_LDFLAGS := $(shell pkg-config --libs libcurl)

LIBUV_CFLAGS	:= -I$${HOME}/workspace/packages/build/include 
LIBUV_LDFLAGS 	:= -L$${HOME}/workspace/packages/build/lib -luv

LIBJSCON_CFLAGS		:= -I./JSCON/include
LIBJSCON_LDFLAGS	:= -L./JSCON/lib -ljscon 

LIBCONCORD_CFLAGS	:= -I$(INCLDIR)
LIBCONCORD_LDFLAGS	:=

LIBS_CFLAGS	:= $(LIBCURL_CFLAGS) $(LIBUV_CFLAGS) \
		      $(LIBJSCON_CFLAGS) $(LIBCONCORD_CFLAGS)

LIBS_LDFLAGS	:= $(LIBCURL_LDFLAGS) $(LIBUV_LDFLAGS) \
		      $(LIBJSCON_LDFLAGS)

# @todo create specific CFLAGS for debugging
CFLAGS := -Wall -O0 -g -std=c11 -D_XOPEN_SOURCE=600

.PHONY : all mkdir install clean purge

all : mkdir $(OBJS) $(CONCORD_DLIB) $(CONCORD_SLIB)

mkdir :
	mkdir -p $(OBJDIR) $(LIBDIR)

$(OBJDIR)/%.o : $(SRCDIR)/%.c
	$(CC) $(CFLAGS) $(LIBS_CFLAGS) \
	      -c $< -o $@ $(LIBS_LDFLAGS)

$(CONCORD_DLIB) :
	$(CC) $(LIBS_CFLAGS) \
	      $(OBJS) -shared -o $(CONCORD_DLIB) $(LIBS_LDFLAGS)

$(CONCORD_SLIB) :
	$(AR) -cvq $(CONCORD_SLIB) $(OBJS)

install : all
	cp $(INCLDIR)/* /usr/local/include
	cp $(CONCORD_DLIB) /usr/local/lib && \
	ldconfig

clean :
	rm -rf $(OBJDIR)

purge : clean
	rm -rf $(LIBDIR)
	$(MAKE) -C test clean
