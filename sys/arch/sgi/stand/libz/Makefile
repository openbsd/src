#	$OpenBSD: Makefile,v 1.1 2004/08/23 14:22:40 pefo Exp $

S=${.CURDIR}/../../../..
ZDST=${.OBJDIR}

.PATH: ${S}/lib/libz

.include "${S}/lib/libz/Makefile"
CFLAGS+= -mno-abicalls -D_NO_ABICALLS ${CEXTRAFLAGS} ${AFLAGS} -I${S}
