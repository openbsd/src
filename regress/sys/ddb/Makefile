#	$OpenBSD: Makefile,v 1.1 2002/06/11 09:12:38 niklas Exp $

REGRESSTARGETS=		comment
.if (${MACHINE} == "i386")
REGRESSTARGETS+=	size
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
