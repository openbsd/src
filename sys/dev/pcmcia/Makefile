#	$OpenBSD: Makefile,v 1.2 1998/09/14 19:20:54 deraadt Exp $
#	$NetBSD: Makefile.pcmciadevs,v 1.1 1998/07/19 17:28:15 christos Exp $

AWK=	awk

pcmciadevs.h pcmciadevs_data.h: pcmciadevs devlist2h.awk
	/bin/rm -f pcmciadevs.h pcmciadevs_data.h
	${AWK} -f devlist2h.awk pcmciadevs
