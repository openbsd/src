#	$OpenBSD: Makefile,v 1.3 2000/01/11 21:26:24 mickey Exp $

LIB=	spmath
NOPIC=

CPPFLAGS+=-I../..

SRCS=	dfadd.c dfcmp.c dfdiv.c dfmpy.c dfrem.c dfsqrt.c dfsub.c divsfm.c \
	divsfr.c divsim.c divsir.c divu.S divufr.c divuir.c fcnvff.c fcnvfx.c \
	fcnvfxt.c fcnvxf.c frnd.c impys.S impyu.S mpyaccs.c mpyaccu.c mpys.c \
	mpyscv.c mpyu.c mpyucv.c setovfl.c sfadd.c sfcmp.c sfdiv.c sfmpy.c \
	sfrem.c sfsqrt.c sfsub.c

MACHINE=	${XMACHINE}
MACHINE_ARCH=	${XMACHINE_ARCH}

# Files to clean up
CLEANFILES+= ${LIB}.o ${LIB}.po

install:

.include <bsd.lib.mk>

${LIB}.o:: ${OBJS}
	@echo building standard ${LIB} library
	@rm -f ${LIB}.o
	@${LD} -r -o ${LIB}.o `${LORDER} ${OBJS} | tsort`

POBJS+=	${OBJS:.o=.po} mcount.po
${LIB}.po:: ${POBJS}
	@echo building profiled ${LIB} library
	@rm -f ${LIB}.po
	@${LD} -r -o ${LIB}.po `${LORDER} ${POBJS} | tsort`

