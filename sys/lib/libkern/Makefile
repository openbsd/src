#	$NetBSD: Makefile,v 1.24 1995/10/20 01:10:06 cgd Exp $

LIB=	kern
NOPIC=

DIR=${KERNREL}${KERNDIR}

CC=	${KERNCC}
CFLAGS=	-I${DIR}/arch/${MACHINE_ARCH} \
	${KERNCFLAGS:S/-D_KERNEL//:S@-I.@-I${KERNREL}.@g}

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
SRCS+=	

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
