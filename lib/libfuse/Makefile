#	$OpenBSD: Makefile,v 1.9 2016/09/04 09:54:25 natano Exp $

LIB=	fuse
MAN=	fuse_main.3

CFLAGS+=	-Wall -g -Wshadow -Wmissing-prototypes
CFLAGS+=	-Wstrict-prototypes -Wsign-compare

CDIAGFLAGS+=	-Wno-long-long -Wall -W -Wnested-externs -Wformat=2
CDIAGFLAGS+=	-Wmissing-prototypes -Wstrict-prototypes -Wmissing-declarations
CDIAGFLAGS+=	-Wwrite-strings -Wshadow -Wpointer-arith -Wsign-compare
CDIAGFLAGS+=	-Wundef -Wbad-function-cast -Winline -Wcast-align

# XXX Shouldn't we use a common fuse.h with proper ifdef _KERNEL part?
CFLAGS+=	-I${.CURDIR} -DDEBUG

SRCS=	debug.c dict.c fuse.c fuse_ops.c fuse_opt.c fuse_subr.c tree.c
HDRS=	fuse.h fuse_opt.h

PC_FILES=fuse.pc
CLEANFILES+=${PC_FILES}

includes:
	@cd ${.CURDIR}; for i in ${HDRS}; do \
		j="cmp -s $$i ${DESTDIR}/usr/include/$$i || \
		    ${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 \
	            $$i ${DESTDIR}/usr/include"; \
	        echo $$j; \
	        eval "$$j"; \
	done

${PC_FILES}: fuse_private.h
	/bin/sh ${.CURDIR}/generate_pkgconfig.sh -c ${.CURDIR} -o ${.OBJDIR}

beforeinstall: ${PC_FILES}
	${INSTALL} ${INSTALL_COPY} -o root -g ${SHAREGRP} \
	    -m ${SHAREMODE} ${.OBJDIR}/${PC_FILES} ${DESTDIR}/usr/lib/pkgconfig/

.include <bsd.lib.mk>
