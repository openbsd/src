#	$OpenBSD: Makefile,v 1.3 1997/04/27 13:48:54 downsj Exp $

PROG=	newsyslog

CFLAGS+= -DCONF=\"/etc/newsyslog.conf\"
CFLAGS+= -DPIDFILE=\"/var/run/syslog.pid\"
CFLAGS+= -DCOMPRESS=\"/usr/bin/gzip\"
CFLAGS+= -DCOMPRESS_POSTFIX=\".gz\"

BINOWN=	root

MAN=	newsyslog.8

.include <bsd.prog.mk>
