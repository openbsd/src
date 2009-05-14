#	$OpenBSD: Makefile,v 1.3 2009/05/14 18:57:43 miod Exp $

S=${.CURDIR}/../../../..
ZDST=${.OBJDIR}

.PATH: ${S}/lib/libz

.include "${S}/lib/libz/Makefile"
CFLAGS+= ${CEXTRAFLAGS} ${SAABI} -mno-abicalls -D_NO_ABICALLS \
	${AFLAGS} -I${S}
