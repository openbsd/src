#	$OpenBSD: Makefile,v 1.2 1997/07/30 19:40:50 flipk Exp $

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
	cat ${I386}/i386/genassym.cf ${.CURDIR}/genassym.cf | \
	sh $S/kern/genassym.sh ${CC} ${CFLAGS} ${CPPFLAGS} \
	    ${PARAM} > assym.h.tmp && mv -f assym.h.tmp assym.h

clean cleandir:
	rm -f isa.h apm.h assym.h

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
.include "../Makefile.inc"
CFLAGS+=${SACFLAGS}
