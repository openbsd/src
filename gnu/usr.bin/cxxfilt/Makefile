NOMAN=1
PROG=c++filt

SRCS=	cxxfilt.c
LIBIBERTY_INCDIR=${.CURDIR}/../../lib/libiberty/include

CFLAGS+=-I${LIBIBERTY_INCDIR}
LDADD=-liberty

cxxmain.c:
	ln -s ${LIBIBERTY_SRCDIR}/cplus-dem.c $@

.include <bsd.prog.mk>

CFLAGS+=-DTARGET_PREPENDS_UNDERSCORE=0
