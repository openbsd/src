#	$OpenBSD: Makefile,v 1.5 2004/11/02 21:12:15 pefo Exp $

LIB=	smutil
SRCS=	debug.c err.c lockfile.c safefile.c snprintf.c cf.c
ENVDEF=	-DNOT_SENDMAIL

.if ${MACHINE_ARCH} == mips64
CFLAGS+= -fno-stack-protector
.endif

# This is not a library that gets installed so only build the .a version
NOPROFILE=1
NOPIC=1

.include <bsd.lib.mk>
