#	$OpenBSD: Makefile,v 1.1 1998/06/23 20:33:49 mickey Exp $

LIB=	spmath
NOPIC=

MACHINE=	${XMACHINE}
MACHINE_ARCH=	${XMACHINE_ARCH}

M=	${.CURDIR}/arch/${MACHINE_ARCH}

CPPFLAGS=	-I$M ${KERNCPPFLAGS}

SRCS+=	divu.S impys.S impyu.S dfadd.c dfcmp.c dfdiv.c dfmpy.c dfrem.c \
	dfsqrt.c dfsub.c divsfm.c divsfr.c divsim.c divsir.c divufr.c \
	divuir.c fcnvff.c fcnvfx.c fcnvfxt.c fcnvxf.c frnd.c mdrr.c \
	mpyaccs.c mpyaccu.c mpys.c mpyscv.c mpyu.c mpyucv.c setovfl.c \
	sfadd.c sfcmp.c sfdiv.c sfmpy.c sfrem.c sfsqrt.c sfsub.c

install:

.include <bsd.lib.mk>

lib${LIB}.o:: ${OBJS}
	@echo building standard ${LIB} library
	@rm -f lib${LIB}.o
	@${LD} -r -o lib${LIB}.o `${LORDER} ${OBJS} | tsort`

POBJS+=	${OBJS:.o=.po}
lib${LIB}.po:: ${POBJS}
	@echo building profiled ${LIB} library
	@rm -f lib${LIB}.po
	@${LD} -r -o lib${LIB}.po `${LORDER} ${POBJS} | tsort`
