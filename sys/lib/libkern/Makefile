#	$OpenBSD: Makefile,v 1.7 1996/05/22 12:06:38 deraadt Exp $
#	$NetBSD: Makefile,v 1.27 1996/05/10 21:27:32 scottr Exp $

LIB=	kern
NOPIC=

DIR=	${KERNREL}${KERNDIR}

CPPFLAGS=	-I${DIR}/arch/${MACHINE_ARCH} \
		${KERNCPPFLAGS:S@-I.@-I${KERNREL}.@g}

.if exists (${DIR}/arch/${MACHINE_ARCH}/Makefile.inc)
.PATH:	${DIR}/arch/${MACHINE_ARCH}
.include "${DIR}/arch/${MACHINE_ARCH}/Makefile.inc"
.endif

.PATH: ${DIR}

.if (${MACHINE_ARCH} != "alpha")
# Quad support
SRCS+=	adddi3.c anddi3.c ashldi3.c ashrdi3.c cmpdi2.c divdi3.c iordi3.c \
	lshldi3.c lshrdi3.c moddi3.c muldi3.c negdi2.c notdi2.c qdivrem.c \
	subdi3.c  ucmpdi2.c udivdi3.c umoddi3.c xordi3.c
.endif

# Other stuff
SRCS+=	md5.c

CLEANFILES+= lib${LIB}.o lib${LIB}.po

# mcount cannot be compiled with profiling
mcount.po: mcount.o
	cp mcount.o mcount.po

install:

.include <bsd.lib.mk>

lib${LIB}.o:: ${OBJS}
	@echo building standard ${LIB} library
	@rm -f lib${LIB}.o
	@${LD} -r -o lib${LIB}.o `lorder ${OBJS} | tsort`

POBJS+=	${OBJS:.o=.po} mcount.po
lib${LIB}.po:: ${POBJS}
	@echo building profiled ${LIB} library
	@rm -f lib${LIB}.po
	@${LD} -r -o lib${LIB}.po `lorder ${POBJS} | tsort`
