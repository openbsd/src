#	$OpenBSD: Makefile,v 1.3 2001/10/05 02:52:33 mickey Exp $

FONTS	= 	vt220l.814 vt220h.814 vt220l.808 vt220h.808 \
		vt220l.816 vt220h.816 vt220l.810 vt220h.810 \
		koi8-r-8x08 koi8-r-8x10 koi8-r-8x14 koi8-r-8x16 \
		koi8-u-8x08 koi8-u-8x10 koi8-u-8x14 koi8-u-8x16 \
		iso8859-1-euro.816

LIBMODE	=	444
LIBOWN	=	bin
LIBGRP	=	bin
CLEANFILES= 	${FONTS}
FONTDIR =	${BINDIR}/misc/pcvtfonts

all: $(FONTS)

install: ${FONTS}
	@if [ ! -d ${DESTDIR}${FONTDIR} ]; then mkdir ${DESTDIR}${FONTDIR};fi
	@for i in ${FONTS}; do \
		echo "installing font $$i into ${DESTDIR}${FONTDIR}"; \
		install -c -m ${LIBMODE} -o ${LIBOWN} -g ${LIBGRP} \
		$$i ${DESTDIR}${FONTDIR}; \
	done

clean:
	rm -f ${CLEANFILES}

.include "../../Makefile.inc"
.include <bsd.prog.mk>

# this seems to be the lowest common denominator

vt100pc.814: ${.CURDIR}/vt100pc.814.uu
	uudecode ${.CURDIR}/$@.uu

vt100sg.814: ${.CURDIR}/vt100sg.814.uu
	uudecode ${.CURDIR}/$@.uu

vt220l.814: ${.CURDIR}/vt220l.814.uu
	uudecode ${.CURDIR}/$@.uu

vt220h.814: ${.CURDIR}/vt220h.814.uu
	uudecode ${.CURDIR}/$@.uu

vt220l.808: ${.CURDIR}/vt220l.808.uu
	uudecode ${.CURDIR}/$@.uu

vt220h.808: ${.CURDIR}/vt220h.808.uu
	uudecode ${.CURDIR}/$@.uu

vt220l.816: ${.CURDIR}/vt220l.816.uu
	uudecode ${.CURDIR}/$@.uu

vt220h.816: ${.CURDIR}/vt220h.816.uu
	uudecode ${.CURDIR}/$@.uu

vt220l.810: ${.CURDIR}/vt220l.810.uu
	uudecode ${.CURDIR}/$@.uu

vt220h.810: ${.CURDIR}/vt220h.810.uu
	uudecode ${.CURDIR}/$@.uu

koi8-r-8x08: ${.CURDIR}/koi8-r-8x08.uu
	uudecode ${.CURDIR}/$@.uu

koi8-r-8x10: ${.CURDIR}/koi8-r-8x10.uu
	uudecode ${.CURDIR}/$@.uu

koi8-r-8x14: ${.CURDIR}/koi8-r-8x14.uu
	uudecode ${.CURDIR}/$@.uu

koi8-r-8x16: ${.CURDIR}/koi8-r-8x16.uu
	uudecode ${.CURDIR}/$@.uu

koi8-u-8x08: ${.CURDIR}/koi8-u-8x08.uu
	uudecode ${.CURDIR}/$@.uu

koi8-u-8x10: ${.CURDIR}/koi8-u-8x10.uu
	uudecode ${.CURDIR}/$@.uu

koi8-u-8x14: ${.CURDIR}/koi8-u-8x14.uu
	uudecode ${.CURDIR}/$@.uu

koi8-u-8x16: ${.CURDIR}/koi8-u-8x16.uu
	uudecode ${.CURDIR}/$@.uu

iso8859-1-euro.816: ${.CURDIR}/iso8859-1-euro.816.uu
	uudecode ${.CURDIR}/$@.uu
