#	$NetBSD: Makefile,v 1.1.1.1 1995/03/26 07:12:03 leo Exp $
#
#	from: @(#)Makefile	7.3 (Berkeley) 6/9/91
#
S=${BSDSRCDIR}/sys
COMM=	$S/vm/*.[ch] $S/ufs/*.[ch] $S/sys/*.h $S/compat/sunos/*.[ch] \
	$S/nfs/*.[ch] $S/netns/*.[ch] $S/netiso/*.[ch] \
	$S/netiso/xebec/*.[ch] $S/netinet/*.[ch] $S/netccitt/*.[ch] \
	$S/net/*.[ch] $S/miscfs/*/*.[ch] $S/kern/*.[ch] $S/dev/*.[ch] \
	$S/scsi/*.[ch] $S/lib/libkern/m68k/*.[ch] $S/lib/libkern/*.[ch]

# Makefile for atari tags file

all:
	@echo "make tags or links only"

TATARI=	$S/arch/atari/tags
SATARI=	$S/arch/m68k/m68k/*.c $S/arch/m68k/include/*.h \
	$S/arch/atari/atari/*.[ch] $S/arch/atari/include/*.h \
	$S/arch/atari/dev/*.[ch]
AATARI=	$S/arch/m68k/m68k/*.s $S/arch/atari/atari/*.s

# Directories in which to place atari tags links
DATARI=	atari dev include

TAGS:
	-etags -dt ${COMM} ${SATARI} ${AATARI}
	egrep "^ENTRY(.*)|^ALTENTRY(.*)" ${AATARI} | \
	    sed "s;\([^:]*\):\([^(]*\)(\([^, )]*\)\(.*\);\3 \1 /^\2(\3\4$$/;" \
	    >> ${TATARI}/tags

tags:
	-ctags -dtf ${TATARI} ${COMM} ${SATARI}
	egrep "^ENTRY(.*)|^ALTENTRY(.*)" ${AATARI} | \
	    sed "s;\([^:]*\):\([^(]*\)(\([^, )]*\)\(.*\);\3 \1 /^\2(\3\4$$/;" \
	    >> ${TATARI}
	sort -o ${TATARI} ${TATARI}

links:
	-for i in ${DATARI}; do \
	    rm -f $$i/tags; rm -f $$i/TAGS ; \
	    ln -s ../tags $$i/tags; ln -s ../TAGS $$i/TAGS; done

.include <bsd.prog.mk>

