#	$OpenBSD: Makefile,v 1.2 2002/09/02 20:01:44 avsm Exp $

REGRESS_TARGETS=		comment
.if (${MACHINE} == "i386")
REGRESS_TARGETS+=	size
.endif

SRC=		${.CURDIR}/../../..

comment:
	fgrep -q '* if [ `size db_hangman.o | awk '"'"'BEGIN {getline} {print $$1+$$2}'"'"'` -gt 1024 ];' ${SRC}/sys/ddb/db_hangman.c

size:	clean
	mkdir build
	/usr/sbin/config -s ${SRC}/sys -b build \
		${SRC}/sys/arch/${MACHINE}/conf/GENERIC
	cd build; \
	make db_hangman.o; \
	if [ `size db_hangman.o | \
		awk 'BEGIN {getline} {print $$1+$$2}'` -gt 1024 ]; \
	then \
		echo 'hangman is too big!!!'; \
		false; \
	fi

clean:
	rm -rf build

.include <bsd.regress.mk>
