NOMAN=1
PROG=c++filt

SRCS=	cxxfilt.c
LIBIBERTY_INCDIR=${.CURDIR}/../../lib/libiberty/include

CFLAGS+=-I${LIBIBERTY_INCDIR}
LDADD=-liberty

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

.include <bsd.prog.mk>

.if ${ELF_TOOLCHAIN:L} == "yes"
CFLAGS+=-DTARGET_PREPENDS_UNDERSCORE=0
.else
CFLAGS+=-DTARGET_PREPENDS_UNDERSCORE=1
.endif
