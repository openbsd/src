#	$OpenBSD: Makefile,v 1.20 2019/10/29 02:55:52 deraadt Exp $

NOMAN=		noman

CFLAGS+=	${SAABI} -mno-abicalls -D_NO_ABICALLS -nostdinc -D__sgi__ \
		-I${.CURDIR}/../include -I${.CURDIR}/../../../.. \
		-I${.CURDIR}/../../../../lib/libsa \
		-I${.OBJDIR}
CFLAGS+=	-D__INTERNAL_LIBSA_CREAD ${STANDALONE} -fno-pie
LDFLAGS+=	-nopie -znorelro

AFLAGS+=	${SAABI}

S=		${.CURDIR}/../../../..
SRCS=		start.S arcbios.c boot.c conf.c diskio.c filesystem.c \
		netfs.c netio.c strstr.c

.PATH:		${S}/lib/libsa
SRCS+=		loadfile.c arc4.c

.PATH:		${S}/lib/libkern/arch/mips64 ${S}/lib/libkern
SRCS+=		memcpy.c memmove.c strchr.c strcmp.S strlcat.c strlcpy.c strlen.c \
		strncmp.c strrchr.c

CLEANFILES+=	machine mips64

.if !make(clean) && !make(cleandir) && !make(includes) && !make(libdep) && \
    !make(sadep) && !make(salibdir) && !make(obj)
.BEGIN:
	@([ -h machine ] || ln -s ${.CURDIR}/../../include machine)
	@([ -h mips64 ] || ln -s ${.CURDIR}/../../../mips64/include mips64)
.endif
