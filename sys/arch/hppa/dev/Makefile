#	$OpenBSD: Makefile,v 1.1 1998/09/29 07:00:46 mickey Exp $

AWK?=	awk

boards.h boards_data.h cpudevs.h cpudevs_data.h: cpudevs devlist2h.awk
	/bin/rm -f cpudevs.h cpudevs_data.h
	${AWK} -f devlist2h.awk cpudevs

