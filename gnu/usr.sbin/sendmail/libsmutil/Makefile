#	$OpenBSD: Makefile,v 1.6 2010/02/03 20:49:00 miod Exp $

LIB=	smutil
SRCS=	debug.c err.c lockfile.c safefile.c snprintf.c cf.c
ENVDEF=	-DNOT_SENDMAIL

.if ${MACHINE_CPU} == mips64
CFLAGS+= -fno-stack-protector
.endif

# This is not a library that gets installed so only build the .a version
NOPROFILE=1
NOPIC=1

.include <bsd.lib.mk>
