#	$OpenBSD: Makefile,v 1.2 2020/04/06 16:23:44 visa Exp $

.include "${.CURDIR}/../Makefile32.inc"
STRIP?=		strip
LDSCRIPT=	${.CURDIR}/ld.script
LDFLAGS+=	${SALDFLAGS} -T ${LDSCRIPT} -e __start -N -s
PROG=		bootecoff
.PATH:	${.CURDIR}/../boot
.include "${.CURDIR}/../boot/Makefile"

SRCS+=	ashrdi3.c divdi3.c moddi3.c udivdi3.c umoddi3.c qdivrem.c

${PROG}: $(OBJS) $(LDADD)
	$(LD) $(LDFLAGS) -o ${PROG} $(OBJS) -L${LIBSADIR} ${LIBSA} \
	    -L${LIBZDIR} ${LIBZ}

LINKS=		${BINDIR}/${PROG} ${BINDIR}/boot-IP22

.include <bsd.prog.mk>
