#	$OpenBSD: Makefile,v 1.1 2000/04/27 02:26:27 bjc Exp $	
#	$NetBSD: Makefile,v 1.2 1999/10/23 14:40:39 ragge Exp $

S=../../../..

PROG=	xxboot
LINKS=	${BINDIR}/xxboot ${BINDIR}/raboot 
LINKS+=	${BINDIR}/xxboot ${BINDIR}/rdboot
LINKS+=	${BINDIR}/xxboot ${BINDIR}/sdboot
LINKS+=	${BINDIR}/xxboot ${BINDIR}/hpboot

SRCS=	start.s bootxx.c romread.s urem.s udiv.s str.s

STRIPFLAG=
CPPFLAGS+=-D_STANDALONE -DLIBSA_NO_FD_CHECKING -DLIBSA_NO_RAW_ACCESS \
	-DLIBSA_NO_TWIDDLE -DLIBSA_SINGLE_DEVICE=rom \
	-DLIBSA_SINGLE_FILESYSTEM=ufs
BINDIR=	/usr/mdec
NOMAN=	1

SA_AS=	library
.include "${S}/lib/libsa/Makefile.inc"
LIBSA=	${SALIB}

${PROG}: ${OBJS} ${LIBSA}
	ld -N -Ttext 100000 -o a.out ${OBJS} ${LIBSA}
	strip a.out
	size a.out
	dd if=a.out of=${PROG} bs=32 skip=1
#	rm -f a.out

clean::
	rm -f a.out [Ee]rrs mklog core *.core ${PROG} ${OBJS} ${LOBJS} \
		${CLEANFILES} 

.include <bsd.prog.mk>
