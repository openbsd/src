#	$OpenBSD: Makefile,v 1.8 1998/05/25 19:04:54 mickey Exp $

S=	${.CURDIR}/../../../..
I386=	${.CURDIR}/../..
CLEANFILES=	i386 machine

all:	machine-links assym.h

machine-links:
	@rm -f machine i386
	@ln -fs ${.CURDIR}/../.. i386
	@ln -fs ${.CURDIR}/../../include machine

depend:: machine-links assym.h

assym.h: $S/kern/genassym.sh ${.CURDIR}/genassym.cf
	sh $S/kern/genassym.sh ${CC} ${CFLAGS} ${CPPFLAGS} \
	    ${PARAM} < ${.CURDIR}/genassym.cf > assym.h.tmp && \
	    mv -f assym.h.tmp assym.h

cleandir: clean

clean:
	@rm -f isa.h apm.h assym.h

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
.include "../Makefile.inc"
CFLAGS+=${SACFLAGS}
