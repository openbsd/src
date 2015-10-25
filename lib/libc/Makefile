#	$OpenBSD: Makefile,v 1.37 2015/10/25 10:22:09 bluhm Exp $
#
# The YP functions are always in libc. To choose that getpwent() and friends
# actually call the YP functions, put -DYP on the CFLAGS line below.

.include <bsd.own.mk>

LIB=c
CLEANFILES+=tags Symbols.map
CFLAGS+=-Wimplicit
#CFLAGS+=-Werror
LDADD=-nodefaultlibs -lgcc

VERSION_SCRIPT=	Symbols.map
SYMBOL_LISTS=	${LIBCSRCDIR}/Symbols.list \
		${LIBCSRCDIR}/arch/${MACHINE_CPU}/Symbols.list


LIBCSRCDIR=${.CURDIR}
.include "${LIBCSRCDIR}/Makefile.inc"

all: tags
tags: ${SRCS}
	ctags -w ${.ALLSRC:M*.c}
	egrep "^SYSENTRY(.*)|^ENTRY(.*)|^FUNC(.*)|^SYSCALL(.*)" \
	    /dev/null ${.ALLSRC:M*.S} | \
	    sed "s;\([^:]*\):\([^(]*\)(\([^, )]*\)\(.*\);\3	\1	/^\2(\3\4$$/;" \
	    >> tags; sort -o tags tags

beforeinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 tags \
		${DESTDIR}/var/db/lib${LIB}.tags

${VERSION_SCRIPT}: ${SYMBOL_LISTS}
	{ printf '{\n\tglobal:\n'; \
	  sed '/^[._a-zA-Z]/s/$$/;/; s/^/		/' ${SYMBOL_LISTS}; \
	  printf '\n\tlocal:\n\t\t*;\n};\n'; } >$@.tmp && mv $@.tmp $@

.include <bsd.lib.mk>
