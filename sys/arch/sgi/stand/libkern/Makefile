#	$OpenBSD: Makefile,v 1.4 2004/09/23 12:55:58 pefo Exp $

LIB=	kern

.PATH:	${.CURDIR}/../../../../lib/libkern

CFLAGS= -O2 ${SAABI} -mno-abicalls -D_NO_ABICALLS ${STANDALONE} \
	-nostdinc -I${.CURDIR}/../include -I${.CURDIR}/../../../.. \
	-I${.CURDIR}/../../../../lib/libkern \
	-I${.CURDIR}/../../../../lib/libkern/arch/mips64 -I${.CURDIR} \
	-I${.OBJDIR}

AFLAGS=	-x assembler-with-cpp -traditional-cpp -I${.CURDIR}/../include \
	-I${.CURDIR}/../../../..  -I${.CURDIR}/../../../../lib/libkern \
	-I${.CURDIR}/../../../../lib/libkern/arch/mips64 \
	-I${.OBJDIR}

# kern routines
SRCS=	__main.c ashrdi3.c bzero.c divdi3.c qdivrem.c strlen.c \
	strcmp.c strlcat.c strlcpy.c strncmp.c umoddi3.c udivdi3.c

${OBJS}:	${.CURDIR}/../Makefile.inc

CLEANFILES += machine mips64

NOPROFILE=
NOPIC=

.if !make(libdep) && !make(sadep) && !make(salibdir) && !make(kernlibdir) && !make(obj)
.BEGIN:
	@([ -h machine ] || ln -s ${.CURDIR}/../../include machine)
	@([ -h mips64 ] || ln -s ${.CURDIR}/../../../mips64/include mips64)
.endif

install:

.include <bsd.lib.mk>
