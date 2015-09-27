#	$OpenBSD: Makefile,v 1.4 2015/09/27 19:11:37 miod Exp $

LDFLAGS+=	${SALDFLAGS} --oformat=elf64-tradbigmips -e __start
STRIP=		strip
INSTALL_STRIP=

PROG=		boot64
.PATH:	${.CURDIR}/../boot
.include "${.CURDIR}/../boot/Makefile"

${PROG}: $(OBJS) $(LDADD)
	@# Link first as self-contained binary to enforce there are no
	@# unresolved symbols
	$(LD) $(LDFLAGS) -o ${PROG} $(OBJS) -L${LIBSADIR} ${LIBSA} \
	    -L${LIBZDIR} ${LIBZ}
	@# then link as a relocatable binary
	$(LD) $(LDFLAGS) -r -o ${PROG} $(OBJS) -L${LIBSADIR} ${LIBSA} \
	    -L${LIBZDIR} ${LIBZ}
	$(STRIP) --strip-unneeded ${PROG}

LINKS=		${BINDIR}/${PROG} ${BINDIR}/boot-IP26
LINKS+=		${BINDIR}/${PROG} ${BINDIR}/boot-IP27
LINKS+=		${BINDIR}/${PROG} ${BINDIR}/boot-IP28
LINKS+=		${BINDIR}/${PROG} ${BINDIR}/boot-IP30

.include <bsd.prog.mk>
