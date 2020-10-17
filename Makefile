CC 	= gcc
SRCDIR 	= src
OBJDIR 	= obj
INCLDIR = include
LIBDIR 	= lib

SRC   = $(filter-out src/%_private.c, $(wildcard src/*.c))
_OBJS = $(patsubst src/%.c, %.o, $(SRC))
OBJS  = $(addprefix $(OBJDIR)/, $(_OBJS))

CONCORD_DLIB = $(LIBDIR)/libconcord.so

CFLAGS = -Wall -Werror -pedantic -g \
	 -I$(INCLDIR) -IJSCON/$(INCLDIR)

LDLIBS = -ljscon -lcurl -luv

.PHONY : all clean purge

all: mkdir $(JSCON_SLIB) $(OBJS) $(CONCORD_DLIB)

mkdir:
	-mkdir -p $(OBJDIR) $(LIBDIR)

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) -c -fPIC $< -o $@ $(CFLAGS)


$(CONCORD_DLIB):
	$(CC) $(OBJS) -shared -o $(CONCORD_DLIB) $(LDLIBS)

install: all
	cp $(CONCORD_DLIB) /usr/local/lib && \
	ldconfig

clean :
	-rm -rf $(OBJDIR)

purge : clean
	-rm -rf $(LIBDIR)
	$(MAKE) -C JSCON purge
	$(MAKE) -C test clean
