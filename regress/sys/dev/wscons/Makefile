# $OpenBSD: Makefile,v 1.5 2018/12/17 19:26:25 anton Exp $

WARNINGS=	yes

DEVICES=	wskbd0 wsmouse0 wskbd

PROG_sigio=	sigio
PROGS+=		${PROG_sigio}
SRCS_sigio=	sigio.c util.c

TESTS_sigio+=	getown-fcntl
TESTS_sigio+=	getown-ioctl
TESTS_sigio+=	gpgrp
TESTS_sigio+=	setown-fcntl
TESTS_sigio+=	setown-ioctl
TESTS_sigio+=	spgrp
TESTS_sigio+=	sigio

.for d in ${DEVICES}
.  for t in ${TESTS_sigio}
.    if "$t" == "sigio" && ("$d" == "wskbd0" || "$d" == "wsmouse0")
# SIGIO using event injection is limited to wsmux devices.
.    else
REGRESS_TARGETS+=	${PROG_sigio}-$d-$t
${PROG_sigio}-$d-$t: ${PROG_sigio}
	@echo "\n======== ${@} ========"
	./${PROG_sigio} -d /dev/$d $t
.    endif
.  endfor
.endfor

.include <bsd.regress.mk>
