NOMAN=1
PROG=c++filt

SRCS=	cxxmain.c underscore.c
LIBIBERTY_SRCDIR=${.CURDIR}/../../lib/libiberty/src
LIBIBERTY_INCDIR=${.CURDIR}/../../lib/libiberty/include

CFLAGS+=-I${LIBIBERTY_INCDIR} -DMAIN -DVERSION=\"${VERSION}\"
VERSION=2.95.3

LDADD=	-liberty

underscore.c:
	@case ${ELF_TOOLCHAIN:L} in \
	    "yes") v=0;; \
	    *) v=1;; \
	esac; \
	c="int prepends_underscore=$$v;"; \
	echo "echo \"$$c\" >$@"; \
	echo $$c >$@

cxxmain.c:
	ln -s ${LIBIBERTY_SRCDIR}/cplus-dem.c $@

CLEANFILES+=${SRCS}
.include <bsd.prog.mk>
