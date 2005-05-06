#	$OpenBSD: Makefile,v 1.3 2005/05/06 11:16:48 pedro Exp $

S=${.CURDIR}/../..
.include "$S/kern/Make.tags.inc"

NOPROG=	noprog
NOMAN=	noman

.if ${MACHINE} == "mvmeppc"
SUBDIR= stand
.endif

TMVMEPPC=	$S/arch/mvmeppc/tags
SMVMEPPC=	$S/arch/mvmeppc/mvmeppc/*.[ch] \
		$S/arch/mvmeppc/include/*.h    \
		$S/arch/mvmeppc/isa/*.[ch]     \
		$S/arch/mvmeppc/pci/*.[ch]
AMVMEPPC=	$S/arch/mvmeppc/mvmeppc/*.[sS]

# Directories in which to place mvmeppc tags links
DMVMEPPC=	conf mvmeppc include isa pci

tags:
	-ctags -wdtf ${TMVMEPPC} ${COMM} ${SMVMEPPC}
	egrep "^ENTRY(.*)|^ALTENTRY(.*)" ${AMVMEPPC} | \
	    sed "s;\([^:]*\):\([^(]*\)(\([^, )]*\)\(.*\);\3	\1	/^\2(\3\4$$/;" \
		>> ${TMVMEPPC}
	sort -o ${TMVMEPPC} ${TMVMEPPC}

links:
	-for i in ${DMVMEPPC}; do \
	    cd $$i && rm -f tags; ln -s ${TMVMEPPC} tags; done

obj:	_SUBDIRUSE

.include <bsd.prog.mk>
