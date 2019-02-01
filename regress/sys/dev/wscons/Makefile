# $OpenBSD: Makefile,v 1.7 2019/02/01 07:15:01 anton Exp $

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

# SIGIO using event injection is limited to wsmux devices.
IGNORE_sigio+=	wskbd0-sigio
IGNORE_sigio+=	wsmouse0-sigio

.for d in ${DEVICES}
.  for t in ${TESTS_sigio}
.    if empty(IGNORE_sigio:M$d-$t)
REGRESS_TARGETS+=	${PROG_sigio}-$d-$t
${PROG_sigio}-$d-$t: ${PROG_sigio}
	@echo "\n======== ${@} ========"
	./${PROG_sigio} -d /dev/$d $t
.    endif
.  endfor
.endfor

PROG_wscons=	wscons
PROGS+=		${PROG_wscons}
SRCS_wscons=	wscons.c util.c

TESTS_wscons+=	ioctl-unknown

.for d in ${DEVICES}
.  for t in ${TESTS_wscons}
REGRESS_TARGETS+=	${PROG_wscons}-$d-$t
${PROG_wscons}-$d-$t: ${PROG_wscons}
	@echo "\n======== ${@} ========"
	./${PROG_wscons} -d /dev/$d $t
.  endfor
.endfor

.include <bsd.regress.mk>
