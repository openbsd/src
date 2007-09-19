#	$OpenBSD: Makefile,v 1.8 2007/09/19 13:35:53 otto Exp $
#
# Makefile for the cpp part of pcc.
#
PROG=   	ccom
PREFIX=		/usr/local
BINDIR=		${PREFIX}/libexec
MANDIR=		${PREFIX}/man/man
TARGOS=		openbsd
.if ${MACHINE_ARCH} == "i386"
TARGMACH=	x86
.else
TARGMACH=	${MACHINE_ARCH}
.endif
CLEANFILES=	external.h external.c mkext

CFLAGS+=	-DPCC_DEBUG -DGCC_COMPAT
CFLAGS+=	-Wall -Wmissing-prototypes -Wstrict-prototypes -Werror
CFLAGS+=	-DLIBEXECDIR=\"${PREFIX}/libexec\"
CPPFLAGS+=	-I. -I${.CURDIR} -I${.CURDIR}/../../mip
CPPFLAGS+=	-I${.CURDIR}/../../arch/${TARGMACH}
CPPFLAGS+=	-I${.CURDIR}/../../os/${TARGOS}
CPPFLAGS+=      -Dmach_${TARGMACH}

.PATH:	${.CURDIR}/../../arch/${TARGMACH}
.PATH:	${.CURDIR}/../../mip

SRCS=optim.c pftn.c scan.l trees.c cgram.y inline.c symtabs.c \
	gcc_compat.c init.c local.c code.c stabs.c match.c reader.c optim2.c \
	regs.c local2.c order.c table.c common.c main.c external.c

cgram.c: cgram.y
	$(YACC) $(YFLAGS) -d $<
	mv y.tab.c cgram.c
	mv y.tab.h cgram.h

mkext: mkext.c table.c common.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -DMKEXT -o mkext 		\
		${.CURDIR}/../../mip/mkext.c			\
		${.CURDIR}/../../arch/${TARGMACH}/table.c	\
		${.CURDIR}/../../mip/common.c

external.h external.c: mkext
	./mkext

cleandepend:
	rm -f .depend ${.CURDIR}/tags cgram.h

.include <bsd.prog.mk>
