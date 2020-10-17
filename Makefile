CC 	= gcc
SRCDIR 	= src
OBJDIR 	= obj
INCLDIR = include
LIBDIR 	= lib

SRC   = $(filter-out src/*_private.c, $(wildcard src/*.c))
_OBJS = $(patsubst src/%.c, %.o, $(SRC))
OBJS  = $(addprefix $(OBJDIR)/, $(_OBJS))

CONCORD_LIB = $(LIBDIR)/libconcord.a
JSCON_LIB   = $(LIBDIR)/libjscon.a

CFLAGS = -Wall -Werror -pedantic -g -I$(INCLDIR) -IJSCON/$(INCLDIR)
LDLIBS = -lcurl $(JSCON_LIB)

.PHONY : all clean purge

all: mkdir $(JSCON_LIB) $(OBJS) $(CONCORD_LIB)

mkdir:
	-mkdir -p $(OBJDIR) $(LIBDIR)

$(JSCON_LIB):
	$(MAKE) -C JSCON purge all
	cp JSCON/libjscon.a $(LIBDIR)

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) -c $< -o $@ $(CFLAGS)

$(CONCORD_LIB):
	-ar rcs $@ $(OBJS)

clean :
	-rm -rf $(OBJDIR)

purge : clean
	-rm -rf $(LIBDIR)
	$(MAKE) -C JSCON purge
