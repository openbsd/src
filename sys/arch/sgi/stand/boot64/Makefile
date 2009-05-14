#	$OpenBSD: Makefile,v 1.1 2009/05/14 18:57:43 miod Exp $

LDFLAGS+=	${SALDFLAGS} -r --oformat=elf64-tradbigmips -e __start
STRIP=		strip
INSTALL_STRIP=

PROG=		boot64
.PATH:	${.CURDIR}/../boot
.include "${.CURDIR}/../boot/Makefile"

${PROG}: $(OBJS) $(LDADD)
	$(LD) $(LDFLAGS) -o ${PROG} $(OBJS) -L${LIBSADIR} ${LIBSA} \
	    -L${LIBZDIR} ${LIBZ}
	$(STRIP) --strip-unneeded ${PROG}

LINKS=		${BINDIR}/${PROG} ${BINDIR}/boot-IP27
LINKS+=		${BINDIR}/${PROG} ${BINDIR}/boot-IP30

.include <bsd.prog.mk>
