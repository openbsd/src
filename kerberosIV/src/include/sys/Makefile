# Generated automatically from Makefile.in by configure.
# $KTH: Makefile.in,v 1.22 1998/03/15 05:57:53 assar Exp $

srcdir		= .

SHELL		= /bin/sh

LN_S		= ln -s
INSTALL		= /usr/bin/install -c
INSTALL_DATA	= ${INSTALL} -m 644
MKINSTALLDIRS = ../../mkinstalldirs

prefix		= /usr/athena
includedir	= ${prefix}/include
BROKEN_SOCKET_H	= no



HEADERS = socket.h

all: stamp-headers

Wall:
	$(MAKE) CFLAGS="-g -Wall -Wno-comment -Wmissing-prototypes -Wmissing-declarations -D__USE_FIXED_PROTOTYPES__"

install: all

uninstall:

clean:
	rm -f $(HEADERS) stamp-headers

mostlyclean:	clean
distclean:	clean
	rm -f Makefile config.status *~

realclean:	clean

socket.h:
	if test "$(BROKEN_SOCKET_H)" = yes; then \
	  $(LN_S) $(srcdir)/socket.x socket.h; \
	fi || true

stamp-headers:
	$(MAKE) $(HEADERS)
	touch stamp-headers

.PHONY: all Wall install uninstall clean mostlyclean distclean realclean
