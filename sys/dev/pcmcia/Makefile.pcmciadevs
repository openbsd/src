#	$OpenBSD: Makefile.pcmciadevs,v 1.1 1998/09/11 10:47:14 fgsch Exp $
#	$NetBSD: Makefile.pcmciadevs,v 1.1 1998/07/19 17:28:15 christos Exp $

AWK=	awk

pcmciadevs.h pcmciadevs_data.h: pcmciadevs devlist2h.awk
	/bin/rm -f pcmciadevs.h pcmciadevs_data.h
	${AWK} -f devlist2h.awk pcmciadevs
