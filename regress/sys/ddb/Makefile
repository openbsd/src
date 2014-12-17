#	$OpenBSD: Makefile,v 1.4 2014/12/17 00:10:22 daniel Exp $

.if (${MACHINE} == "i386")
REGRESS_TARGETS+=	size
.endif

SRC=		${.CURDIR}/../../..

size:	clean
	mkdir build
	/usr/sbin/config -s ${SRC}/sys -b build \
		${SRC}/sys/arch/${MACHINE}/conf/GENERIC
	cd build; \
	make db_hangman.o; \
	if [ `size db_hangman.o | \
		awk 'BEGIN {getline} {print $$1+$$2}'` -gt 2048 ]; \
	then \
		echo 'hangman is too big!!!'; \
		false; \
	fi

clean:
	rm -rf build

.include <bsd.regress.mk>
