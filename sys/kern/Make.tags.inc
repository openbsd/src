#	$OpenBSD: Make.tags.inc,v 1.8 2004/11/22 17:08:38 pat Exp $

# Common files for "make tags".
# Included by the Makefile for each architecture.

# Put the ../sys stuff near the end so that subroutine definitions win when
# there is a struct tag with the same name (eg., vmmeter).  The real
# solution would probably be for ctags to generate "struct vmmeter" tags.

COMM=	$S/adosfs/*.[ch] \
	$S/altq/*.[ch] \
	$S/compat/*/*.[ch] \
	$S/conf/*.[ch] \
	$S/crypto/*.[ch] \
	$S/ddb/*.[ch] \
	$S/dev/*.[ch] $S/dev/*/*.[ch] $S/dev/*/*/*.[ch] \
	$S/isofs/*/*.[ch] \
	$S/kern/*.[ch] \
	$S/lib/libkern/*.[ch] \
	$S/miscfs/*/*.[ch] \
	$S/msdosfs/*.[ch] \
	$S/net/*.[ch] \
	$S/netatalk/*.[ch] \
	$S/netccitt/*.[ch] \
	$S/netinet/*.[ch] \
	$S/netinet6/*.[ch] \
	$S/netipx/*.[ch] \
	$S/netnatm/*.[ch] \
	$S/netns/*.[ch] \
	$S/nfs/*.[ch] \
	$S/scsi/*.[ch] \
	$S/ufs/*/*.[ch] \
	$S/uvm/*.[ch] \
	$S/sys/*.[ch]
