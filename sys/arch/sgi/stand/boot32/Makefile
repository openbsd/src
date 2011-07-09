#	$OpenBSD: Makefile,v 1.2 2011/07/09 10:44:09 miod Exp $

.include "${.CURDIR}/../Makefile32.inc"
LDSCRIPT=	${.CURDIR}/ld.script
LDFLAGS+=	${SALDFLAGS} -T ${LDSCRIPT} -e __start -s
PROG=		boot32
.PATH:	${.CURDIR}/../boot
.include "${.CURDIR}/../boot/Makefile"

SRCS+=	ashrdi3.c moddi3.c udivdi3.c umoddi3.c qdivrem.c

${PROG}: $(OBJS) $(LDADD)
	$(LD) $(LDFLAGS) -o ${PROG} $(OBJS) -L${LIBSADIR} ${LIBSA} \
	    -L${LIBZDIR} ${LIBZ}

LINKS=		${BINDIR}/${PROG} ${BINDIR}/boot-IP32

.include <bsd.prog.mk>
