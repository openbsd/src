#	$OpenBSD: Makefile,v 1.1 1997/07/28 22:03:27 mickey Exp $

S=	${.CURDIR}/../../../..
I386=	${.CURDIR}/../..

all:	machine-links assym.h

machine-links:
	@rm -f machine i386
	@ln -fs ${.CURDIR}/../.. i386
	@ln -fs ${.CURDIR}/../../include machine

assym.h: $S/kern/genassym.sh ${I386}/i386/genassym.cf
	@echo '#define NISA 1' > isa.h
	@echo '#define NAPM 1' > apm.h
	sh $S/kern/genassym.sh ${CC} ${CFLAGS} ${CPPFLAGS} \
	    ${PARAM} < ${I386}/i386/genassym.cf > assym.h.tmp && \
	    mv -f assym.h.tmp assym.h

clean:
	@rm -f isa.h apm.h assym.h

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
.include "../Makefile.inc"
CFLAGS+=${SACFLAGS}
