TOP 	:= ../..
OBJDIR	:= $(TOP)/obj

ifeq ($(DEBUG),1)
CFLAGS ?= -Wall -Wextra -O0 -ggdb3 -fPIC
else
CFLAGS ?= -Wall -Wextra -O1 -U_FORTIFY_SOURCE \
	  -D_FORTIFY_SOURCE=1 -fPIC
endif

.PHONY: all clean mv

all: curl-websocket.o mv

curl-websocket.o: curl-websocket.c

mv:
	mv curl-websocket.o $(OBJDIR)/

clean:
	rm -f $(OBJDIR)/curl-websocket.o
