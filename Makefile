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
UV_LIB      = $(LIBDIR)/libuv_a.a

CFLAGS = -Wall -Werror -pedantic -g \
	 -I$(INCLDIR) -IJSCON/$(INCLDIR) -Ilibuv/$(INCLDIR)

LDLIBS = -lcurl $(JSCON_LIB) $(UV_LIB)

.PHONY : all clean purge

all: mkdir $(JSCON_LIB) $(UV_LIB) $(OBJS) $(CONCORD_LIB)

mkdir:
	-mkdir -p $(OBJDIR) $(LIBDIR)

$(JSCON_LIB):
	$(MAKE) -C JSCON purge all
	cp JSCON/libjscon.a $(LIBDIR)

$(UV_LIB):
	cd libuv && \
	mkdir -p build && \
	(cd build && cmake .. -DBUILD_TESTING=ON) && \
	cmake --build build && cp build/libuv_a.a ../$(LIBDIR)
	# Run tests:
	#(cd build && ctest -C Debug --output-on-failure)

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) -c $< -o $@ $(CFLAGS)

$(CONCORD_LIB):
	ar rcs $@ $(OBJS)

clean :
	-rm -rf $(OBJDIR)

purge : clean
	-rm -rf $(LIBDIR)
	$(MAKE) -C JSCON purge
	$(MAKE) -C test clean
