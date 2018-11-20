# $OpenBSD: Makefile,v 1.4 2018/11/20 19:43:48 anton Exp $

PROG=	sigio

WARNINGS=	yes

TESTS+=	getown-fcntl
TESTS+=	getown-ioctl
TESTS+=	gpgrp
TESTS+=	setown-fcntl
TESTS+=	setown-ioctl
TESTS+=	spgrp
TESTS+=	sigio

.for d in wskbd0 wsmouse0 wskbd
.  for t in ${TESTS}
REGRESS_TARGETS+=	$d-$t
$d-$t: ${PROG}
	@echo "\n======== ${@} ========"
	if ./${PROG} -p -d /dev/$d $t; then \
		./${PROG} -d /dev/$d $t; \
	else \
		echo "SKIPPED"; \
	fi
.  endfor
.endfor

# SIGIO is limited to wsmux devices.
REGRESS_SKIP_TARGETS+=	wskbd0-sigio wsmouse0-sigio

.include <bsd.regress.mk>
