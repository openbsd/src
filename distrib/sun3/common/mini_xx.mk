#
# $OpenBSD: mini_xx.mk,v 1.3 2000/03/01 22:10:10 todd Exp $
# $NetBSD: mini_xx.mk,v 1.1.6.1 1996/08/29 03:17:15 gwr Exp $
# Hacks for re-linking some programs -static
#

MINI_XX = grep less tip vi
mini_xx : ${MINI_XX}

clean_xx:
	-rm -f ${MINI_XX}

grep :
	cd ${BSDSRCDIR}/gnu/usr.bin/grep ;\
	$(MAKE) -f Makefile -f ${TOP}/common/Make.static \
	    OUTDIR=${.CURDIR} ${.CURDIR}/grep

less :
	cd ${BSDSRCDIR}/usr.bin/less/less ;\
	$(MAKE) -f Makefile -f ${TOP}/common/Make.static \
	    OUTDIR=${.CURDIR} ${.CURDIR}/less

tip :
	cd ${BSDSRCDIR}/usr.bin/tip ;\
	$(MAKE) -f Makefile -f ${TOP}/common/Make.static \
	    OUTDIR=${.CURDIR} ${.CURDIR}/tip

vi :
	cd ${BSDSRCDIR}/usr.bin/vi/build ;\
	$(MAKE) -f Makefile -f ${TOP}/common/Make.static \
	    OUTDIR=${.CURDIR} ${.CURDIR}/vi
