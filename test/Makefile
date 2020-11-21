TOP 	:= ..
CC	:= gcc

LIBDIR	:= $(TOP)/lib

LIBCURL_CFLAGS	:= $(shell pkg-config --cflags libcurl)
LIBCURL_LDFLAGS	:= $(shell pkg-config --libs libcurl)

LIBUV_CFLAGS	:= $(shell pkg-config --cflags libuv)
LIBUV_LDFLAGS	:= $(shell pkg-config --libs libuv)

LIBJSCON_CFLAGS		:=
LIBJSCON_LDFLAGS	:= -ljscon

LIBCONCORD_CFLAGS	:= -I$(TOP)/include
LIBCONCORD_LDFLAGS	:= "-Wl,-rpath,$(LIBDIR)" -L$(LIBDIR) -lconcord

LIBS_CFLAGS	:= $(LIBCURL_CFLAGS) $(LIBUV_CFLAGS) \
		      $(LIBJSCON_CFLAGS) $(LIBCONCORD_CFLAGS)

LIBS_LDFLAGS	:= $(LIBCURL_LDFLAGS) $(LIBUV_LDFLAGS) \
		      $(LIBJSCON_LDFLAGS) $(LIBCONCORD_LDFLAGS)

CFLAGS	= -Wall -Werror -Wextra -pedantic -g

.PHONY : clean purge

all: test-api test-websockets

test-api : test-api.c $(LIBDIR) Makefile
	$(CC) $(CFLAGS) $(LIBS_CFLAGS) \
	      test-api.c -o $@ $(LIBS_LDFLAGS)

test-websockets : test-websockets.c $(LIBDIR) Makefile
	$(CC) $(CFLAGS) $(LIBS_CFLAGS) \
	      test-websockets.c -o $@ $(LIBS_LDFLAGS)

$(LIBDIR) :
	$(MAKE) -C $(TOP)

clean :
	rm -rf test-api test-websockets *.out
