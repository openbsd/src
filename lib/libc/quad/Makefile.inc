#	$OpenBSD: Makefile.inc,v 1.3 1998/11/20 11:18:45 d Exp $

# Quad support
SRCS+=	adddi3.c anddi3.c ashldi3.c ashrdi3.c cmpdi2.c divdi3.c fixdfdi.c \
	fixsfdi.c fixunsdfdi.c fixunssfdi.c floatdidf.c floatdisf.c \
	floatunsdidf.c iordi3.c lshldi3.c lshrdi3.c moddi3.c muldi3.c \
	negdi2.c notdi2.c qdivrem.c subdi3.c ucmpdi2.c udivdi3.c umoddi3.c \
	xordi3.c

.PATH: ${LIBCSRCDIR}/arch/${MACHINE_ARCH}/quad ${LIBCSRCDIR}/quad
