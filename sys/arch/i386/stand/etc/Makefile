#	$OpenBSD: Makefile,v 1.3 1997/07/30 20:47:43 mickey Exp $

S=	${.CURDIR}/../../../..
I386=	${.CURDIR}/../..

all:	machine-links assym.h

machine-links:
	@rm -f machine i386
	@ln -fs ${.CURDIR}/../.. i386
	@ln -fs ${.CURDIR}/../../include machine

assym.h: $S/kern/genassym.sh ${.CURDIR}/genassym.cf
	sh $S/kern/genassym.sh ${CC} ${CFLAGS} ${CPPFLAGS} \
	    ${PARAM} < ${.CURDIR}/genassym.cf > assym.h.tmp && \
	    mv -f assym.h.tmp assym.h

clean:
	@rm -f isa.h apm.h assym.h

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
.include "../Makefile.inc"
CFLAGS+=${SACFLAGS}
