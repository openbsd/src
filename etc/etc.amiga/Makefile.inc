#	$OpenBSD: Makefile.inc,v 1.16 2002/05/13 07:22:42 espie Exp $
#	etc.amiga/Makefile.inc -- amiga-specific etc Makefile targets

MACHINE_HAS_TOOLS=Yes

.ifdef DESTDIR
snap_md: distrib bsd tools
	cp ${.CURDIR}/../sys/arch/amiga/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd

bsd:
	# The GENERIC kernel is built as a side-effect of the miniroot

tools:
	mkdir -p ${DESTDIR}/snapshot/tools
	cp ${.CURDIR}/../sys/arch/amiga/stand/loadbsd/obj/loadbsd ${DESTDIR}/snapshot/tools
.for I in devtostream rdbinfo streamtodev xdevtostream xstreamtodev \
	device-streams.tgz
	cp ${.CURDIR}/../sys/arch/amiga/stand/device-streams/obj/${I} ${DESTDIR}/snapshot/tools
.endfor

.PHONY: bsd tools
.endif	# DESTDIR check
