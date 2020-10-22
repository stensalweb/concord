CC 	= gcc
SRCDIR 	= src
OBJDIR 	= obj
INCLDIR = include
LIBDIR 	= lib

SRC   = $(wildcard src/*.c)
_OBJS = $(patsubst src/%.c, %.o, $(SRC))
OBJS  = $(addprefix $(OBJDIR)/, $(_OBJS))

CONCORD_DLIB = $(LIBDIR)/libconcord.so
# path to JSCON lib folder here

CFLAGS = -Wall -Werror -pedantic -g \
	 -I$(INCLDIR)

LDLIBS  = -L$(LIBDIR) -ljscon -lcurl -luv
LDFLAGS = "-Wl,-rpath,$(LIBDIR)" 

.PHONY : all clean purge

all: mkdir $(OBJS) $(CONCORD_DLIB)

mkdir:
	mkdir -p $(OBJDIR) $(LIBDIR)

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) -c -fPIC $< -o $@ $(CFLAGS)


$(CONCORD_DLIB):
	$(CC) $(OBJS) -shared -o $(CONCORD_DLIB) $(LDLIBS)

install: all
	cp $(INCL_DIR)/* /usr/local/include
	cp $(CONCORD_DLIB) /usr/local/lib && \
	ldconfig

clean :
	rm -rf $(OBJDIR)

purge : clean
	rm -rf $(LIBDIR)
	$(MAKE) -C test clean
