CFLAGS = -Wall -Werror -pedantic
LDLIBS = -lcurl $(LIBDIR)/JSCON.a
CC = gcc

SRCDIR = src
OBJDIR = obj
INCLUDEDIR = -Iinclude -IJSCON/include
LIBDIR = lib
LIB = $(LIBDIR)/libconcord.a
EXEC = run

OBJS = 	$(OBJDIR)/test.o \
	$(OBJDIR)/memory.o \
	$(OBJDIR)/channel.o \
	$(OBJDIR)/user.o \
	$(OBJDIR)/guild.o \
	$(OBJDIR)/utils_private.o \
	$(OBJDIR)/http.o

MAIN = test.c
MAIN_O = $(OBJDIR)/test.o

.PHONY : clean all debug

all: $(EXEC)

$(EXEC): build $(LIBDIR)/JSCON.a
	$(CC) -o $@ $(OBJS) $(LDLIBS)

$(LIBDIR)/JSCON.a: JSCON/obj
	-ar rcs $@ JSCON/obj/*

JSCON/obj:
	$(MAKE) -C JSCON

build: mkdir $(MAIN_O) $(OBJS) $(LIB)

mkdir:
	-mkdir -p $(OBJDIR) $(LIBDIR)

$(MAIN_O): $(MAIN)
	$(CC) $(INCLUDEDIR) -c $< -o $@ $(CFLAGS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(INCLUDEDIR) -c $< -o $@ $(CFLAGS)

$(LIB):
	-ar rcs $@ $(OBJS)

debug : $(EXEC) $(MAIN) $(SRCDIR)/*.c
	$(CC) -g $(INCLUDEDIR) $(MAIN) $(SRCDIR)/*.c -o debug.out $(CFLAGS) $(LDLIBS)

clean :
	-rm -rf $(OBJDIR)

purge : clean
	-rm -rf $(EXEC) $(LIBDIR) *.txt debug.out
