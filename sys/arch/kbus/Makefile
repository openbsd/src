#	$NetBSD: Makefile,v 1.2 1994/10/26 08:23:50 cgd Exp $

#	@(#)Makefile	7.3 (Berkeley) 6/9/91

COMM=	../net/*.[ch] ../netimp/*.[ch] ../netinet/*.[ch] ../netns/*.[ch] \
	../netiso/*.[ch] ../netccitt/*.[ch] \
	../kern/*.c ../ufs/*.[ch] ../nfs/*.[ch] ../vm/*.[ch] ../sys/*.h

# Makefile for pc532 tags file

SUBDIR= stand

TPC532=	../pc532/tags
SPC532=	../pc532/pc532/*.[ch] ../pc532/include/*.h \
	../pc532/dev/*.[ch] ../pc532/scsi/*.[ch]
APC532=	../pc532/pc532/*.s

# Directories in which to place pc532 tags links
DPC532=	dev include scsi

tags:
	-ctags -dtf ${TPC532} ${COMM} ${SPC532}
	egrep "^ENTRY(.*)|^ALTENTRY(.*)" ${APC532} | \
	    sed "s;\([^:]*\):\([^(]*\)(\([^, )]*\)\(.*\);\3 \1 /^\2(\3\4$$/;" \
		>> ${TPC532}
	sort -o ${TPC532} ${TPC532}

links:
	-for i in ${DPC532}; do \
	    cd ../$$i && rm -f tags; ln -s ../tags tags; done

.include <bsd.prog.mk>
