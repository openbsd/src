#	$OpenBSD: Makefile.inc,v 1.15 1999/10/08 10:52:20 espie Exp $
#	etc.amiga/Makefile.inc -- amiga-specific etc Makefile targets

MACHINE_HAS_TOOLS=Yes

.ifdef DESTDIR
snap_md: distrib bsd tools

bsd:
	# The GENERIC kernel is built as a side-effect of the miniroot
	cp ${.CURDIR}/../sys/arch/amiga/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd

distrib:
	cd ${.CURDIR}/../distrib && \
	    ${MAKE} cleandir && ${MAKE} && ${MAKE} install

tools:
	mkdir -p ${DESTDIR}/snapshot/tools
	cp ${.CURDIR}/../sys/arch/amiga/stand/loadbsd/obj/loadbsd ${DESTDIR}/snapshot/tools
.for I in devtostream rdbinfo streamtodev xdevtostream xstreamtodev \
	device-streams.tgz
	cp ${.CURDIR}/../sys/arch/amiga/stand/device-streams/obj/${I} ${DESTDIR}/snapshot/tools
.endfor

.endif	# DESTDIR check
