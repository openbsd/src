#	$OpenBSD: Makefile,v 1.3 2004/09/16 18:54:48 pefo Exp $


PROG=		boot
NOMAN=		noman

CFLAGS+=	-O2 ${SAABI} -mno-abicalls -D_NO_ABICALLS -nostdinc -D__sgi__ \
		-I${.CURDIR}/../include -I${.CURDIR}/../../../.. \
		-I${.CURDIR}/../../../../lib/libsa \
		-I${.OBJDIR}
CFLAGS+=	-D__INTERNAL_LIBSA_CREAD ${STANDALONE}

AFLAGS+=	${SAABI}

LDFLAGS+=	${SALDFLAGS} -T ${.CURDIR}/ld.script -e __start

SRCS=		start.S boot.c filesystem.c conf.c diskio.c arcbios.c

CLEANFILES+= machine mips64

.if !make(libdep) && !make(sadep) && !make(salibdir) && !make(kernlibdir) && !make(obj)
.BEGIN:
	@([ -h machine ] || ln -s ${.CURDIR}/../../include machine)
	@([ -h mips64 ] || ln -s ${.CURDIR}/../../../mips64/include mips64)
.endif

${PROG}: $(OBJS) $(LDADD)
	$(LD) $(LDFLAGS) -o boot $(OBJS) -L${LIBSADIR} ${LIBSA} \
	-L${LIBKERNDIR} ${LIBKERN}  -L${LIBZDIR} ${LIBZ}


install:
	install -c -m 555 -g bin -o bin ${PROG} ${DESTDIR}/usr/mdec

.include <bsd.prog.mk>
