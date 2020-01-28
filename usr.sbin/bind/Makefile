#	$OpenBSD: Makefile,v 1.1 2020/01/28 17:17:04 florian Exp $

PROG=		dig
MAN=
LINKS=		${BINDIR}/dig ${BINDIR}/host \
		${BINDIR}/dig ${BINDIR}/nslookup
CLEANFILES+=	code.h enumclass.h enumtype.h gen gen.d rdatastruct.h

.include "${.CURDIR}/bin/dig/Makefile.inc"
.include "${.CURDIR}/lib/dns/Makefile.inc"
.include "${.CURDIR}/lib/isc/Makefile.inc"
.include "${.CURDIR}/lib/isc/unix/Makefile.inc"
.include "${.CURDIR}/lib/isccfg/Makefile.inc"
.include "${.CURDIR}/lib/lwres/Makefile.inc"

CFLAGS+= -Wall -I${.CURDIR} -I${.OBJDIR}
CFLAGS+= -I ${.CURDIR}/bin/dig/include
CFLAGS+= -I ${.CURDIR}/lib/dns/include
CFLAGS+= -I ${.CURDIR}/lib/isc/include
CFLAGS+= -I ${.CURDIR}/lib/isc/unix/include
CFLAGS+= -I ${.CURDIR}/lib/isccfg/include
CFLAGS+= -I ${.CURDIR}/lib/lwres/include
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations
CFLAGS+= -Wshadow -Wpointer-arith
CFLAGS+= -Wsign-compare
CFLAGS+= -DVERSION=\"9.10.8-P1\"
YFLAGS=
LDADD+=	-lcrypto
DPADD+= ${LIBCRYPTO}

BUILDFIRST = enumtype.h enumclass.h rdatastruct.h code.h

gen: lib/dns/gen.c
	${HOSTCC} ${CFLAGS} ${.CURDIR}/lib/dns/gen.c -o $@

enumtype.h: gen
	./gen -s ${.CURDIR}/lib/dns -t > $@

enumclass.h: gen
	./gen -s ${.CURDIR}/lib/dns -c > $@

rdatastruct.h: gen lib/dns/rdata/rdatastructpre.h \
	lib/dns/rdata/rdatastructsuf.h
	./gen -s ${.CURDIR}/lib/dns -i \
		-P ${.CURDIR}/lib/dns/rdata/rdatastructpre.h \
		-S ${.CURDIR}/lib/dns/rdata/rdatastructsuf.h > $@

code.h:	gen
	./gen -s ${.CURDIR}/lib/dns > code.h

.include <bsd.prog.mk>
