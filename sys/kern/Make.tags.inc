#	$OpenBSD: Make.tags.inc,v 1.6 2001/06/12 21:51:10 deraadt Exp $

# Common files for "make tags".
# Included by the Makefile for each architecture.

# Put the ../sys stuff near the end so that subroutine definitions win when
# there is a struct tag with the same name (eg., vmmeter).  The real
# solution would probably be for ctags to generate "struct vmmeter" tags.

COMM=	/sys/adosfs/*.[ch] \
	/sys/compat/*/*.[ch] \
	/sys/conf/*.[ch] \
	/sys/ddb/*.[ch] \
	/sys/dev/*.[ch] /sys/dev/*/*.[ch] \
	/sys/isofs/*/*.[ch] \
	/sys/kern/*.[ch] \
	/sys/lib/libkern/*.[ch] \
	/sys/miscfs/*/*.[ch] \
	/sys/net/*.[ch] \
	/sys/netccitt/*.[ch] \
	/sys/netinet/*.[ch] \
	/sys/netiso/*.[ch] \
	/sys/netns/*.[ch] \
	/sys/nfs/*.[ch] \
	/sys/scsi/*.[ch] \
	/sys/ufs/*/*.[ch] \
	/sys/vm/*.[ch] \
	/sys/sys/*.[ch]
