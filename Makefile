CFLAGS = -Wall -Werror -pedantic
LDLIBS = -lcurl JSONc.a
CC = gcc

SRCDIR = src
OBJDIR = obj
EXEC = discordc

OBJS = 	$(OBJDIR)/test.o \
	$(OBJDIR)/REST.o \
	$(OBJDIR)/channel.o \
	$(OBJDIR)/user.o \
	$(OBJDIR)/guild.o \
	$(OBJDIR)/api_wrapper.o

MAIN = test.c
MAIN_O = $(OBJDIR)/test.o

.PHONY : clean all debug

all: $(EXEC)

$(EXEC): JSONc.a build
	$(CC) -o $@ $(OBJS) $(LDLIBS)

JSONc.a:
	ar rcs $@ JSONc/obj/*

build: mkdir $(MAIN_O) $(OBJS)

mkdir:
	-mkdir -p $(OBJDIR)

$(MAIN_O): $(MAIN)
	$(CC) -c $< -o $@ $(CFLAGS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) -c $< -o $@ $(CFLAGS)

debug : $(MAIN) $(SRCDIR)/*.c
	$(CC) -g $(MAIN) $(SRCDIR)/*.c -o debug.out $(CFLAGS)

clean :
	-rm -rf $(OBJDIR)

purge : clean
	-rm -rf $(EXEC) *.txt debug.out JSONc.a
