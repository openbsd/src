# $OpenBSD: Makefile.inc,v 1.1.1.1 2001/08/15 14:37:10 fgsch Exp $
# Copyright (c) 1993 Chris Provenzano, proven@athena.mit.edu

LIBC_R?=	/usr/lib/libc_r.a
LIBPTHREAD?=	/usr/lib/libpthread.a

.if 1 # ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "sparc"
LDADD+=		-lpthread
DPADD+=		${LIBPTHREAD}
.else
LDADD+=		-pthread
DPADD+=		${LIBC_R}
.endif

CFLAGS+=	-Wall # -Werror
DEBUG=		-ggdb
CFLAGS+=	-DSRCDIR='"${.CURDIR}"'
CFLAGS+=	-I${.CURDIR}/../include

NOMAN=

MAXTIME=	5

regress: ${PROG}
	@echo; echo "*** $? ***"; \
	if (ulimit -t ${MAXTIME}; ${.OBJDIR}/$?); then \
		echo "-- $? passed"; \
	else \
		echo "-- $? FAILED (exit code $$?)" >&2; \
	fi; 
