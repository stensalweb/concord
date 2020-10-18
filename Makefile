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
JSCON_DIR   = ./JSCON

CFLAGS = -Wall -Werror -pedantic -g \
	 -I$(INCLDIR) -IJSCON/$(INCLDIR)

LDLIBS  = -L$(JSCON_DIR)/lib -ljscon -lcurl -luv
LDFLAGS = "-Wl,-rpath,$(JSCON_DIR)/lib" 

.PHONY : all clean purge

all: mkdir $(OBJS) $(CONCORD_DLIB)

mkdir:
	mkdir -p $(OBJDIR) $(LIBDIR)

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) -c -fPIC $< -o $@ $(CFLAGS)


$(CONCORD_DLIB): $(JSCON_DIR)/lib
	$(CC) $(OBJS) -shared -o $(CONCORD_DLIB) $(LDLIBS)

$(JSCON_DIR)/lib:
	$(MAKE) -C $(JSCON_DIR)
	cp $(JSCON_DIR)/lib/* lib

install: all
	cp $(CONCORD_DLIB) /usr/local/lib && \
	ldconfig

clean :
	rm -rf $(OBJDIR)

purge : clean
	rm -rf $(LIBDIR)
	$(MAKE) -C JSCON purge
	$(MAKE) -C test clean
