#	$OpenBSD: Makefile.inc,v 1.4 1998/09/17 23:43:40 niklas Exp $

BINDIR?=	/usr/sbin
.if (${MACHINE_ARCH} == "alpha")
CFLAGS:=	${CFLAGS:S/^-O2$/-O/}
.endif
