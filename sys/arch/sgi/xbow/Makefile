#	$OpenBSD: Makefile,v 1.1 2008/04/07 22:47:40 miod Exp $

AWK=	awk

xbowdevs.h xbowdevs_data.h: xbowdevs devlist2h.awk
	/bin/rm -f xbowdevs.h xbowdevs_data.h
	${AWK} -f devlist2h.awk xbowdevs
