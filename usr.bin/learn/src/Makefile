#	$OpenBSD: Makefile,v 1.3 2001/01/29 01:57:59 niklas Exp $

PROG = learn

LLIB = /usr/share/learn	# must agree with pathnames.h

CFLAGS += -g

all:	learn tee lcount

# tee and lcount must be installed in LLIB!
# old makefile installed learn into LLIB as well - is it needed there?
install:	all
	echo install -o ${BINOWN} -g ${BINGRP} -m 444 learn \
			${DESTDIR}${BINDIR}/learn; \
	install -o ${BINOWN} -g ${BINGRP} tee lcount $(LLIB)

check:
	-@test -r $(LLIB)/tee || echo 'tee not present; make tee'
	-@test -r $(LLIB)/lcount || echo 'lcount not present; make lcount'


# clean rule should also remove tee and lcount

.include <bsd.prog.mk>
