TOP  = ..

LIBDIR = $(TOP)/lib

EXEC = run
SRC  = test.c

CFLAGS = -Wall -Werror -pedantic -g -I$(TOP)/include -I$(TOP)/JSCON/include
LDLIBS = $(LIBDIR)/* -lcurl

.PHONY : clean purge

$(EXEC): $(LIBDIR)
	gcc -o $@ $(SRC) $(LDLIBS) $(CFLAGS)

$(LIBDIR):
	$(MAKE) -C $(TOP)

clean :
	-rm -rf run

purge :
	$(MAKE) -C $(TOP) purge
