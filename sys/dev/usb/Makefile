#	$OpenBSD: Makefile,v 1.1 1999/08/13 05:28:03 fgsch Exp $
#	$NetBSD: Makefile.usbdevs,v 1.2 1999/01/07 22:18:23 augustss Exp $

AWK=	awk
UNAME=	uname

usbdevs.h usbdevs_data.h: usbdevs devlist2h.awk
	/bin/rm -f usbdevs.h usbdevs_data.h
	${AWK} -v os=`${UNAME} -s` -f devlist2h.awk usbdevs
