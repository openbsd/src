#	$OpenBSD: Makefile,v 1.2 2004/09/16 18:54:49 pefo Exp $

S=${.CURDIR}/../../../..
ZDST=${.OBJDIR}

.PATH: ${S}/lib/libz

.include "${S}/lib/libz/Makefile"
CFLAGS+= ${SAABI} -mno-abicalls -D_NO_ABICALLS ${CEXTRAFLAGS} ${AFLAGS} -I${S}
