CC = gcc
CFLAGS=-Wall -std=c99

DESTDIR=

all:
	$(CC) $(CFLAGS) io-vtf.c  -o libpixbufloader-vtf.so \
		`pkg-config --cflags gtk+-2.0` \
		-shared -fpic -DGDK_PIXBUF_ENABLE_BACKEND -O3

clean:
	rm libpixbufloader-vtf.so

install:
	chmod 644 libpixbufloader-vtf.so
	mkdir -p $(DESTDIR)/usr/lib/gtk-2.0/2.10.0/loaders/
	cp libpixbufloader-vtf.so $(DESTDIR)/usr/lib/gtk-2.0/2.10.0/loaders/

