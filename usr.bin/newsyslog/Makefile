#	$OpenBSD: Makefile,v 1.5 2001/08/21 17:25:05 deraadt Exp $

PROG=	newsyslog

CFLAGS+= -DCONF=\"/etc/newsyslog.conf\"
CFLAGS+= -DPIDFILE=\"/var/run/syslog.pid\"
CFLAGS+= -DCOMPRESS=\"/usr/bin/gzip\"
CFLAGS+= -DCOMPRESS_POSTFIX=\".gz\"
CFLAGS+= -DSTATS_DIR=\"/var/run\"
CFLAGS+= -DSENDMAIL=\"/usr/sbin/sendmail\"
CFLAGS+= -DQUAD_OFF_T

BINOWN=	root

MAN=	newsyslog.8
MLINKS=	newsyslog.8 newsyslog.conf.5

.include <bsd.prog.mk>
