#	$OpenBSD: Makefile,v 1.5 2000/11/05 22:28:19 miod Exp $

SUBDIR= # libdl

CFLAGS += -I${.CURDIR} -DNO_UNDERSCORE -DVERBOSE_DLINKER \
	-DUSE_CACHE -D__PIC__ -I${.CURDIR}/${MACHINE_ARCH}

SRCS=	boot1.c hash.c readelflib1.c vsprintf.c elfinterp.c
PROG=	ld.so
MAN=	ld.so.8
BINDIR=/usr/libexec
INSTALL_STRIP=

.PATH:	${.CURDIR}/${MACHINE_ARCH}

$(PROG):
	${CC} -shared -e _dl_boot -o ${PROG} `${LORDER} ${OBJS}|tsort -q`


.include <bsd.prog.mk>
.include <bsd.subdir.mk>
