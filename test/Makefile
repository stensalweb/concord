TOP  = ..

LIBDIR = $(TOP)/lib

EXEC = run
SRC  = test.c

CFLAGS = -Wall -Werror -pedantic -g \
	 -I$(TOP)/include -I$(TOP)/JSCON/include

LDLIBS  = -L$(LIBDIR) -ljscon -lcurl -luv -lconcord
LDFLAGS = "-Wl,-rpath,$(LIBDIR)"

.PHONY : clean purge

$(EXEC): $(LIBDIR)
	gcc -o $@ $(SRC) $(LDLIBS) $(CFLAGS) $(LDFLAGS)

$(LIBDIR):
	$(MAKE) -C $(TOP)

clean :
	-rm -rf run *.out
