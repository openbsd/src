#	$OpenBSD: Makefile,v 1.11 2009/05/14 18:57:43 miod Exp $

NOMAN=		noman

CFLAGS+=	${SAABI} -mno-abicalls -D_NO_ABICALLS -nostdinc -D__sgi__ \
		-I${.CURDIR}/../include -I${.CURDIR}/../../../.. \
		-I${.CURDIR}/../../../../lib/libsa \
		-I${.OBJDIR}
CFLAGS+=	-D__INTERNAL_LIBSA_CREAD ${STANDALONE}

AFLAGS+=	${SAABI}

S=		${.CURDIR}/../../../..
SRCS=		start.S boot.c filesystem.c conf.c diskio.c arcbios.c strstr.c

.PATH:		${S}/lib/libsa
SRCS+=		loadfile.c

.PATH:		${S}/lib/libkern/arch/mips64 ${S}/lib/libkern
SRCS+=		strlcpy.c memcpy.c strlen.c strrchr.c strlcat.c strncmp.c \
		strcmp.S

CLEANFILES+=	machine mips64

.if !make(libdep) && !make(sadep) && !make(salibdir) && !make(obj)
.BEGIN:
	@([ -h machine ] || ln -s ${.CURDIR}/../../include machine)
	@([ -h mips64 ] || ln -s ${.CURDIR}/../../../mips64/include mips64)
.endif
