#	$OpenBSD: Makefile,v 1.4 2020/09/14 08:39:12 florian Exp $

PROG=		dig
SRCS=		dig.c dighost.c host.c nslookup.c
MAN=		dig.1 host.1 nslookup.1
LINKS=		${BINDIR}/dig ${BINDIR}/host \
		${BINDIR}/dig ${BINDIR}/nslookup
CLEANFILES+=	code.h gen gen.d

.include "${.CURDIR}/lib/dns/Makefile.inc"
.include "${.CURDIR}/lib/isc/Makefile.inc"
.include "${.CURDIR}/lib/isc/unix/Makefile.inc"
.include "${.CURDIR}/lib/isccfg/Makefile.inc"
.include "${.CURDIR}/lib/lwres/Makefile.inc"

CFLAGS+= -Wall -I${.CURDIR} -I${.OBJDIR}
CFLAGS+= -I ${.CURDIR}/bin/dig/include
CFLAGS+= -I ${.CURDIR}/lib/dns/include
CFLAGS+= -I ${.CURDIR}/lib/isc/include
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

BUILDFIRST = code.h

gen: lib/dns/gen.c
	${HOSTCC} ${CFLAGS} ${.CURDIR}/lib/dns/gen.c -o $@

code.h:	gen
	./gen -s ${.CURDIR}/lib/dns > code.h

.include <bsd.prog.mk>
