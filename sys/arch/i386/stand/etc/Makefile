#	$OpenBSD: Makefile,v 1.6 1997/10/14 20:19:19 weingart Exp $

S=	${.CURDIR}/../../../..
I386=	${.CURDIR}/../..

all:	machine-links assym.h

machine-links:
	@rm -f machine i386
	@ln -fs ${.CURDIR}/../.. i386
	@ln -fs ${.CURDIR}/../../include machine

depend:: assym.h

assym.h: $S/kern/genassym.sh ${.CURDIR}/genassym.cf machine-links
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
