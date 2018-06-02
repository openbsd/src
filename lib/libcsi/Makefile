#	$OpenBSD: Makefile,v 1.1 2018/06/02 17:40:33 jsing Exp $

.include <bsd.own.mk>
.ifndef NOMAN
#SUBDIR=	man
.endif

CFLAGS+= -Wall -Wimplicit -Wundef
.if ${COMPILER_VERSION:L} == "clang"
CFLAGS+= -Werror
.endif
CFLAGS+= -DLIBRESSL_INTERNAL
CFLAGS+= -I ${.CURDIR}

CLEANFILES= ${VERSION_SCRIPT}

WARNINGS= Yes

LIB=	csi

DPADD=	${LIBCRYPTO}

LDADD+= -L${BSDOBJDIR}/lib/libcrypto -lcrypto

VERSION_SCRIPT=	Symbols.map
SYMBOL_LIST=	${.CURDIR}/Symbols.list

HDRS=	csi.h

SRCS=	csi.c \
	csi_dh.c \
	csi_dh_groups.c \
	csi_util.c

#includes:
#	@cd ${.CURDIR}; for i in $(HDRS); do \
#	    j="cmp -s $$i ${DESTDIR}/usr/include/$$i || \
#	    ${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 $$i\
#		${DESTDIR}/usr/include/"; \
#	    echo $$j; \
#	    eval "$$j"; \
#	done;

${VERSION_SCRIPT}: ${SYMBOL_LIST}
	{ printf '{\n\tglobal:\n'; \
	  sed '/^[._a-zA-Z]/s/$$/;/; s/^/		/' ${SYMBOL_LIST}; \
	  printf '\n\tlocal:\n\t\t*;\n};\n'; } >$@.tmp && mv $@.tmp $@

.include <bsd.lib.mk>
