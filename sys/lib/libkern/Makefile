#	$OpenBSD: Makefile,v 1.16 2004/05/06 01:12:05 deraadt Exp $
#	$NetBSD: Makefile,v 1.31 1996/08/10 04:01:31 mycroft Exp $

LIB=	kern
NOPIC=

MACHINE=	${XMACHINE}
MACHINE_ARCH=	${XMACHINE_ARCH}

M=	${.CURDIR}/arch/${MACHINE_ARCH}

CPPFLAGS=	-I$M ${KERNCPPFLAGS}

.if exists ($M/Makefile.inc)
.PATH:	$M
.include "$M/Makefile.inc"
.endif

.if (${MACHINE_ARCH} != "alpha" && ${MACHINE} != "sgi" && \
    ${MACHINE_ARCH} != "sparc64") && ${MACHINE} != "amd64"
# Quad support
SRCS+=	adddi3.c anddi3.c ashldi3.c ashrdi3.c cmpdi2.c divdi3.c iordi3.c \
	lshldi3.c lshrdi3.c moddi3.c muldi3.c negdi2.c notdi2.c qdivrem.c \
	subdi3.c  ucmpdi2.c udivdi3.c umoddi3.c xordi3.c
.endif

# Other stuff
SRCS+=	md5.c getsn.c srandom.c bcd.c strchr.c

# Files to clean up
CLEANFILES+= lib${LIB}.o lib${LIB}.po

# mcount cannot be compiled with profiling
mcount.po: mcount.o
	cp mcount.o mcount.po

install:

.include <bsd.lib.mk>

lib${LIB}.o:: ${OBJS}
	@echo building standard ${LIB} library
	@rm -f lib${LIB}.o
	@${LD} -r -o lib${LIB}.o `${LORDER} ${OBJS} | tsort`

POBJS+=	${OBJS:.o=.po} mcount.po
lib${LIB}.po:: ${POBJS}
	@echo building profiled ${LIB} library
	@rm -f lib${LIB}.po
	@${LD} -r -o lib${LIB}.po `${LORDER} ${POBJS} | tsort`
