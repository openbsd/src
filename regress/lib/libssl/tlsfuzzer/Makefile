#	$OpenBSD: Makefile,v 1.1 2020/05/21 10:38:43 tb Exp $

.if !exists(/usr/local/share/tlsfuzzer)
regress:
	@echo package py3-tlsfuzzer is required for this regress
	@echo SKIPPED
.else

localhost.key localhost.crt:
	openssl req -x509 -newkey rsa -keyout localhost.key -out localhost.crt \
		-subj /CN=localhost -nodes -batch

certs: localhost.key localhost.crt

CLEANFILES	+= localhost.key localhost.crt

PORT		?= 4433
SLOW		= -s
TIMING		= # -t
VERBOSE		= # -v

all: certs
	python3 ${.CURDIR}/tlsfuzzer.py ${SLOW} ${TIMING} ${VERBOSE}

failing: certs
	python3 ${.CURDIR}/tlsfuzzer.py -f ${SLOW} ${TIMING} ${VERBOSE}


port: certs
	python3 ${.CURDIR}/tlsfuzzer.py ${SLOW} ${TIMING} ${VERBOSE} -p ${PORT}

list:
	@python3 ${.CURDIR}/tlsfuzzer.py -l

list-failing:
	@python3 ${.CURDIR}/tlsfuzzer.py -l -f

missing:
	@python3 ${.CURDIR}/tlsfuzzer.py -m

.PHONY: all certs failing list list-failing missing port

.endif

.include <bsd.regress.mk>
