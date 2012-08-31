#	$OpenBSD: Makefile,v 1.5 2012/08/31 08:14:08 pascal Exp $

S=${.CURDIR}/../../../..
ZDST=${.OBJDIR}

.PATH: ${S}/lib/libz

.include "${S}/lib/libz/Makefile"
CFLAGS+= ${CEXTRAFLAGS} ${SAABI} -mno-abicalls -D_NO_ABICALLS \
	${AFLAGS} -fno-pie -I${S}
