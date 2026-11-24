CC ?= cc
PKG_CONFIG ?= pkg-config
CFLAGS ?= -O2
WARNINGS = -std=c17 -Wall -Wextra -Wpedantic -Wshadow -Wconversion
PACKAGES = sdl3 zbar
SOURCES = src/main.c src/decoder.c src/results.c
HEADERS = src/decoder.h src/results.h

ifeq ($(shell uname -s),Darwin)
APP_METADATA = -Wl,-sectcreate,__TEXT,__info_plist,Info.plist
METADATA_DEP = Info.plist
endif

.PHONY: all check-deps demo clean
all: qr-scanner

check-deps:
	@$(PKG_CONFIG) --atleast-version=3.2.0 sdl3 && $(PKG_CONFIG) --exists zbar || { \
		echo "SDL3 >= 3.2.0 and ZBar development files are required. See README.md." >&2; \
		exit 1; \
	}

qr-scanner: $(SOURCES) $(HEADERS) $(METADATA_DEP) Makefile | check-deps
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) \
		$(shell $(PKG_CONFIG) --cflags $(PACKAGES) 2>/dev/null) $(SOURCES) \
		$(LDFLAGS) $(APP_METADATA) $(shell $(PKG_CONFIG) --libs $(PACKAGES) 2>/dev/null) $(LDLIBS) -o $@

demo: qr-scanner
	./qr-scanner --image demo/qr-example.bmp --no-history

clean:
	rm -f qr-scanner
