VERSION  = 1.0.0
PREFIX  ?= /usr/local
BINDIR   = $(PREFIX)/bin
MANDIR   = $(PREFIX)/share/man/man1
DOCDIR   = $(PREFIX)/share/doc/flowm

CC      ?= cc
PKGCFG  ?= pkg-config

X11_CFLAGS := $(shell $(PKGCFG) --cflags x11 2>/dev/null)
X11_LIBS   := $(shell $(PKGCFG) --libs x11 2>/dev/null || echo -lX11)

CFLAGS  ?= -O2
CFLAGS  += -std=c99 -Wall -Wextra -Wpedantic -Wshadow \
           -Wmissing-prototypes -Wstrict-prototypes \
           -Wwrite-strings -Wconversion-extra-dummy
CFLAGS  := $(filter-out -Wconversion-extra-dummy,$(CFLAGS))
CFLAGS  += $(X11_CFLAGS) -DFLOWM_VERSION=\"$(VERSION)\"
LDLIBS  += $(X11_LIBS)

SRC = src/main.c src/wm.c src/client.c src/events.c src/keys.c \
      src/config.c src/ewmh.c src/workspace.c src/bar.c \
      src/rules.c src/util.c src/log.c src/geometry.c src/struts.c \
      src/scratchpad.c src/ipc.c
OBJ = $(SRC:.c=.o)
BIN = flowm

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJ) $(LDLIBS)

$(OBJ): src/wm.h src/client.h src/events.h src/keys.h src/config.h \
        src/ewmh.h src/workspace.h src/bar.h src/rules.h src/util.h \
        src/log.h src/geometry.h src/struts.h src/scratchpad.h src/ipc.h

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

debug: CFLAGS += -O0 -g3 -fsanitize=address,undefined
debug: LDFLAGS += -fsanitize=address,undefined
debug: clean $(BIN)

install: $(BIN)
	install -Dm755 $(BIN) $(DESTDIR)$(BINDIR)/$(BIN)
	install -Dm644 flowm.1 $(DESTDIR)$(MANDIR)/flowm.1
	install -Dm644 flowm.conf $(DESTDIR)$(DOCDIR)/flowm.conf

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(BIN)
	rm -f $(DESTDIR)$(MANDIR)/flowm.1
	rm -f $(DESTDIR)$(DOCDIR)/flowm.conf

TEST_BIN = tests/test_parsers tests/test_geometry

test: $(TEST_BIN)
	./tests/test_parsers
	./tests/test_geometry

tests/test_parsers: tests/test_parsers.c src/config.c src/rules.c \
                    src/util.c src/log.c src/geometry.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

tests/test_geometry: tests/test_geometry.c src/geometry.c src/util.c \
                    src/log.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

UTILS_BIN = utils/panelctl

utils: $(UTILS_BIN)

$(UTILS_BIN): utils/panelctl.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ utils/panelctl.c $(LDLIBS)

clean:
	rm -f $(BIN) $(OBJ) $(TEST_BIN) $(UTILS_BIN)

.PHONY: all debug test install uninstall clean utils
