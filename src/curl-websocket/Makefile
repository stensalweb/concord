TOP 	= ../..
OBJDIR	= $(TOP)/obj

LIBCURL_CFLAGS := $(shell pkg-config --cflags libcurl)
LIBCURL_LDFLAGS := $(shell pkg-config --libs libcurl)

LIBOPENSSL_CFLAGS := $(shell pkg-config --cflags openssl)
LIBOPENSSL_LDFLAGS := $(shell pkg-config --libs openssl)

ifeq ($(DEBUG),1)
CFLAGS ?= -Wall -Wextra -O0 -ggdb3
else
CFLAGS ?= -Wall -Wextra -O1 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=1 #-fsanitize=undefined
endif

LIBS_CFLAGS := $(LIBCURL_CFLAGS) $(LIBOPENSSL_CFLAGS)
LIBS_LDFLAGS := $(LIBCURL_LDFLAGS) $(LIBOPENSSL_LDFLAGS)

.PHONY: all clean mv

all: curl-websocket.o mv

curl-websocket.o: curl-websocket.c
	$(CC) -c -fPIC $< -o $@ $(CFLAGS)

mv:
	mv curl-websocket.o $(OBJDIR)/

clean:
	rm -f $(OBJDIR)/curl-websocket.o
