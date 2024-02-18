.POSIX:

include config.mk

SRC = src/env.c src/main.c src/mpc.c src/utils.c src/value.c
OBJ = $(SRC:.c=.o)

BUILD_COMMAND = $(CC) -o clisp $(OBJ) $(CFLAGS) $(LIBS)
BUILD_OBJ_COMMAND = $(CC) -c -o $@ $(CFLAGS) $(LIBS) $<

all: $(OBJ)
	$(BUILD_COMMAND)

src/%.o: src/%.c
ifdef DEBUG
	$(BUILD_OBJ_COMMAND) -g
else
	$(BUILD_OBJ_COMMAND)
endif

src/env.o: src/env.h src/utils.h src/value.h
src/main.o: src/env.h src/grammar.h src/mpc.h src/value.h
src/mpc.o: src/mpc.h
src/utils.o: src/utils.h
src/value.o: src/config.h src/grammar.h src/env.h src/mpc.h src/utils.h src/value.h

clean:
	rm -f clisp $(OBJ)

install: clisp
	mkdir -p $(PREFIX)/bin
	cp -f $< $(PREFIX)/bin/$<
	chmod 755 $(PREFIX)/bin/$<

uninstall:
	rm -f $(PREFIX)/bin/clisp

.PHONY: clisp clean install uninstall
