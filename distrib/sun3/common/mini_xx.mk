#
# $NetBSD: mini_xx.mk,v 1.1 1995/11/21 21:19:04 gwr Exp $
# Hacks for re-linking some programs -static
#

MINI_XX = awk grep more tip vi
mini_xx : ${MINI_XX}

clean_xx:
	-rm -f mini_xx ${MINI_XX}

awk : FORCE
	cd ${BSDSRCDIR}/gnu/usr.bin/gawk ;\
	$(MAKE) -f Makefile -f ${TOP}/common/Make.static \
	    OUTDIR=${.CURDIR} ${.CURDIR}/awk

grep : FORCE
	cd ${BSDSRCDIR}/gnu/usr.bin/grep ;\
	$(MAKE) -f Makefile -f ${TOP}/common/Make.static \
	    OUTDIR=${.CURDIR} ${.CURDIR}/grep

more : FORCE
	cd ${BSDSRCDIR}/usr.bin/more ;\
	$(MAKE) -f Makefile -f ${TOP}/common/Make.static \
	    OUTDIR=${.CURDIR} ${.CURDIR}/more

tip : FORCE
	cd ${BSDSRCDIR}/usr.bin/tip ;\
	$(MAKE) -f Makefile -f ${TOP}/common/Make.static \
	    OUTDIR=${.CURDIR} ${.CURDIR}/tip

vi : FORCE
	cd ${BSDSRCDIR}/usr.bin/vi/common ;\
	$(MAKE) -f Makefile -f ${TOP}/common/Make.static \
	    OUTDIR=${.CURDIR} ${.CURDIR}/vi

FORCE:
