#	from: @(#)Makefile	7.3 (Berkeley) 6/9/91
#	$Id: Makefile,v 1.1.1.1 1995/10/18 10:54:18 deraadt Exp $
S=${BSDSRCDIR}/sys
COMM=	$S/vm/*.[ch] $S/ufs/*.[ch] $S/sys/*.h $S/compat/sunos/*.[ch] \
	$S/nfs/*.[ch] $S/netns/*.[ch] $S/netiso/*.[ch] \
	$S/netiso/xebec/*.[ch] $S/netinet/*.[ch] $S/netccitt/*.[ch] \
	$S/net/*.[ch] $S/miscfs/*/*.[ch] $S/kern/*.[ch] $S/dev/*.[ch] \
	$S/scsi/*.[ch] $S/lib/libkern/m88k/*.[ch] $S/lib/libkern/*.[ch]

# Makefile for m88k tags file

all:
	@echo "make tags or links only"

TM88K=	$S/arch/m88k/tags
SM88K=	$S/arch/m88k/m88k/*.[ch] $S/arch/m88k/include/*.h \
	$S/arch/m88k/dev/*.[ch]
AM88K=	$S/arch/m88k/m88k/*.s

# Directories in which to place m88k tags links
DM88K=	m88k dev include

TAGS:
	-etags -dt ${COMM} ${SM88K} ${AM88K}
	egrep "^ENTRY(.*)|^ALTENTRY(.*)" ${AM88K} | \
	    sed "s;\([^:]*\):\([^(]*\)(\([^, )]*\)\(.*\);\3 \1 /^\2(\3\4$$/;" \
	    >> ${TM88K}/tags

tags:
	-ctags -dtf ${TM88K} ${COMM} ${SM88K}
	egrep "^ENTRY(.*)|^ALTENTRY(.*)" ${AM88K} | \
	    sed "s;\([^:]*\):\([^(]*\)(\([^, )]*\)\(.*\);\3 \1 /^\2(\3\4$$/;" \
	    >> ${TM88K}
	sort -o ${TM88K} ${TM88K}

links:
	-for i in ${DM88K}; do \
	    rm -f $$i/tags; rm -f $$i/TAGS ; \
	    ln -s ../tags $$i/tags; ln -s ../TAGS $$i/TAGS; done

.include <bsd.prog.mk>

