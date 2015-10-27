CC=gcc
CFLAGS=-Wall -Wextra -Werror -std=gnu99
BIN=libpixbufloader-vtf.so
DESTDIR=`pkg-config gdk-pixbuf-2.0 --variable=gdk_pixbuf_moduledir`

all: $(BIN)

$(BIN): io-vtf.c
	$(CC) $(CFLAGS) $< -o $@ \
		`pkg-config --cflags gtk+-2.0` \
		-shared -fpic -DGDK_PIXBUF_ENABLE_BACKEND -O3

clean:
	rm $(BIN)

install: $(BIN) x-vtf.xml
	mkdir -p $(DESTDIR)
	install -t $(DESTDIR) $(BIN)
	gdk-pixbuf-query-loaders --update-cache

uninstall:
	rm $(DESTDIR)/$(BIN)

.PHONY: all clean install uninstall
