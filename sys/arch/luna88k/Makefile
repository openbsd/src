#	$OpenBSD: Makefile,v 1.5 2006/09/27 09:13:31 grange Exp $

S=${.CURDIR}/../..
.include "$S/kern/Make.tags.inc"

NOPROG=
NOMAN=
NOOBJ=

SUBDIR=

TLUNA88K=	$S/arch/luna88k/tags
SLUNA88K=	$S/arch/luna88k/luna88k/*.[ch] \
		$S/arch/luna88k/dev/*.[ch] \
		$S/arch/luna88k/include/*.h
ALUNA88K=	$S/arch/luna88k/luna88k/*.[sS]

# Directories in which to place luna88k tags links
DLUNA88K=	conf dev include luna88k

tags:
	-ctags -wdtf ${TLUNA88K} ${COMM} ${SLUNA88K}
	egrep "^ENTRY(.*)|^ALTENTRY(.*)" ${ALUNA88K} | \
	    sed "s;\([^:]*\):\([^(]*\)(\([^, )]*\)\(.*\);\3	\1	/^\2(\3\4$$/;" \
		>> ${TLUNA88K}
	sort -o ${TLUNA88K} ${TLUNA88K}

links:
	-for i in ${DLUNA88K}; do \
	    (cd $$i && rm -f tags; ln -s ${TLUNA88K} tags); done

obj:	_SUBDIRUSE

.include <bsd.prog.mk>
