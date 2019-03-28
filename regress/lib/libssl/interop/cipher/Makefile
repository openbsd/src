# $OpenBSD: Makefile,v 1.3 2019/03/28 22:24:13 bluhm Exp $

# Connect a client to a server.  Both can be current libressl, or
# openssl 1.0.2, or openssl 1.1.  Create lists of supported ciphers
# and pin client and server to one of the ciphers.  Use server
# certificate with compatible type.  Check that client and server
# have used correct cipher by grepping in their session print out.

check-cipher-ADH-AES128-GCM-SHA256-client-openssl11-server-openssl11 \
check-cipher-ADH-AES128-SHA-client-openssl11-server-openssl11 \
check-cipher-ADH-AES128-SHA256-client-openssl11-server-openssl11 \
check-cipher-ADH-AES256-GCM-SHA384-client-openssl11-server-openssl11 \
check-cipher-ADH-AES256-SHA-client-openssl11-server-openssl11 \
check-cipher-ADH-AES256-SHA256-client-openssl11-server-openssl11 \
check-cipher-ADH-CAMELLIA128-SHA-client-openssl11-server-openssl11 \
check-cipher-ADH-CAMELLIA128-SHA256-client-openssl11-server-openssl11 \
check-cipher-ADH-CAMELLIA256-SHA-client-openssl11-server-openssl11 \
check-cipher-ADH-CAMELLIA256-SHA256-client-openssl11-server-openssl11 \
check-cipher-AECDH-AES128-SHA-client-openssl11-server-openssl11 \
check-cipher-AECDH-AES256-SHA-client-openssl11-server-openssl11 \
check-cipher-AES128-GCM-SHA256-client-openssl11-server-openssl11 \
check-cipher-AES128-SHA-client-openssl11-server-openssl11 \
check-cipher-AES128-SHA256-client-openssl11-server-openssl11 \
check-cipher-AES256-GCM-SHA384-client-openssl11-server-openssl11 \
check-cipher-AES256-SHA-client-openssl11-server-openssl11 \
check-cipher-AES256-SHA256-client-openssl11-server-openssl11 \
check-cipher-CAMELLIA128-SHA-client-openssl11-server-openssl11 \
check-cipher-CAMELLIA128-SHA256-client-openssl11-server-openssl11 \
check-cipher-CAMELLIA256-SHA-client-openssl11-server-openssl11 \
check-cipher-CAMELLIA256-SHA256-client-openssl11-server-openssl11 \
check-cipher-DHE-RSA-AES128-GCM-SHA256-client-openssl11-server-openssl11 \
check-cipher-DHE-RSA-AES128-SHA-client-openssl11-server-openssl11 \
check-cipher-DHE-RSA-AES128-SHA256-client-openssl11-server-openssl11 \
check-cipher-DHE-RSA-AES256-GCM-SHA384-client-openssl11-server-openssl11 \
check-cipher-DHE-RSA-AES256-SHA-client-openssl11-server-openssl11 \
check-cipher-DHE-RSA-AES256-SHA256-client-openssl11-server-openssl11 \
check-cipher-DHE-RSA-CAMELLIA128-SHA-client-openssl11-server-openssl11 \
check-cipher-DHE-RSA-CAMELLIA128-SHA256-client-openssl11-server-openssl11 \
check-cipher-DHE-RSA-CAMELLIA256-SHA-client-openssl11-server-openssl11 \
check-cipher-DHE-RSA-CAMELLIA256-SHA256-client-openssl11-server-openssl11 \
check-cipher-DHE-RSA-CHACHA20-POLY1305-client-openssl11-server-openssl11 \
check-cipher-ECDHE-ECDSA-AES128-GCM-SHA256-client-openssl11-server-openssl11 \
check-cipher-ECDHE-ECDSA-AES128-SHA-client-openssl11-server-openssl11 \
check-cipher-ECDHE-ECDSA-AES128-SHA256-client-openssl11-server-openssl11 \
check-cipher-ECDHE-ECDSA-AES256-GCM-SHA384-client-openssl11-server-openssl11 \
check-cipher-ECDHE-ECDSA-AES256-SHA-client-openssl11-server-openssl11 \
check-cipher-ECDHE-ECDSA-AES256-SHA384-client-openssl11-server-openssl11 \
check-cipher-ECDHE-ECDSA-CHACHA20-POLY1305-client-openssl11-server-openssl11 \
check-cipher-ECDHE-RSA-AES128-GCM-SHA256-client-openssl11-server-openssl11 \
check-cipher-ECDHE-RSA-AES128-SHA-client-openssl11-server-openssl11 \
check-cipher-ECDHE-RSA-AES128-SHA256-client-openssl11-server-openssl11 \
check-cipher-ECDHE-RSA-AES256-GCM-SHA384-client-openssl11-server-openssl11 \
check-cipher-ECDHE-RSA-AES256-SHA-client-openssl11-server-openssl11 \
check-cipher-ECDHE-RSA-AES256-SHA384-client-openssl11-server-openssl11 \
check-cipher-ECDHE-RSA-CHACHA20-POLY1305-client-openssl11-server-openssl11:
	# openssl11 always prints TLS_AES_256_GCM_SHA384 as cipher in out file
	@echo DISABLED

LIBRARIES =		libressl
.if exists(/usr/local/bin/eopenssl)
LIBRARIES +=		openssl
.endif
.if exists(/usr/local/bin/eopenssl11)
LIBRARIES +=		openssl11
.endif

CLEANFILES =	*.tmp *.ciphers ciphers.mk

.for clib in ${LIBRARIES}
client-${clib}.ciphers:
	LD_LIBRARY_PATH=/usr/local/lib/e${clib} \
	    ../${clib}/client -l ALL -L >$@.tmp
	sed -n 's/^cipher //p' <$@.tmp | sort -u >$@
	rm $@.tmp
.endfor
.for slib in ${LIBRARIES}
server-${slib}.ciphers: 127.0.0.1.crt dsa.crt ec.crt rsa.crt
	LD_LIBRARY_PATH=/usr/local/lib/e${slib} \
	    ../${slib}/server -l ALL -L >$@.tmp
	sed -n 's/^cipher //p' <$@.tmp | sort -u >$@
	rm $@.tmp
.endfor

.for clib in ${LIBRARIES}
.for slib in ${LIBRARIES}
ciphers.mk: client-${clib}-server-${slib}.ciphers
client-${clib}-server-${slib}.ciphers: \
    client-${clib}.ciphers server-${slib}.ciphers client-libressl.ciphers
	# get ciphers shared between client and server
	sort client-${clib}.ciphers server-${slib}.ciphers >$@.tmp
	uniq -d <$@.tmp >$@
	# we are only interested in ciphers supported by libressl
	sort $@ client-libressl.ciphers >$@.tmp
	uniq -d <$@.tmp >$@
	rm $@.tmp
.endfor
.endfor

ciphers.mk:
	rm -f $@ $@.tmp
.for clib in ${LIBRARIES}
.for slib in ${LIBRARIES}
	echo 'CIPHERS_${clib}_${slib} =' >>$@.tmp \
	    `cat client-${clib}-server-${slib}.ciphers`
.endfor
.endfor
	mv $@.tmp $@

# hack to convert generated lists into usable make variables
.if exists(ciphers.mk)
.include "ciphers.mk"
.else
regress: ciphers.mk
	${MAKE} -C ${.CURDIR} regress
.endif

LEVEL_libressl =
LEVEL_openssl =
LEVEL_openssl11 = ,@SECLEVEL=0

.for clib in ${LIBRARIES}
.for slib in ${LIBRARIES}
.for cipher in ${CIPHERS_${clib}_${slib}}

.if "${cipher:M*-DSS-*}" != ""
TYPE_${cipher} =	dsa
.elif "${cipher:M*-ECDSA-*}" != ""
TYPE_${cipher} =	ec
.elif "${cipher:M*-GOST89-*}" != ""
TYPE_${cipher} =	gost
.elif "${cipher:M*-RSA-*}" != ""
TYPE_${cipher} =	rsa
.else
TYPE_${cipher} =	127.0.0.1
.endif

.if "${slib}" == "openssl" && \
    "${cipher:MADH-*}${cipher:MEDH-*}${cipher:MDHE-*}" != ""
DHPARAM_${cipher}_${slib} =	-p dh.param
.else
DHPARAM_${cipher}_${slib} =
.endif

REGRESS_TARGETS +=	run-cipher-${cipher}-client-${clib}-server-${slib}
run-cipher-${cipher}-client-${clib}-server-${slib} \
client-cipher-${cipher}-client-${clib}-server-${slib}.out \
server-cipher-${cipher}-client-${clib}-server-${slib}.out: dh.param \
    127.0.0.1.crt ${TYPE_${cipher}}.crt ../${clib}/client ../${slib}/server
	@echo '\n======== $@ ========'
	LD_LIBRARY_PATH=/usr/local/lib/e${slib} \
	    ../${slib}/server >${@:S/^run/server/}.out \
	    -c ${TYPE_${cipher}}.crt -k ${TYPE_${cipher}}.key \
	    -l ${cipher}${LEVEL_${slib}} ${DHPARAM_${cipher}_${slib}} \
	    127.0.0.1 0
	LD_LIBRARY_PATH=/usr/local/lib/e${clib} \
	    ../${clib}/client >${@:S/^run/client/}.out \
	    -l ${cipher}${LEVEL_${clib}} \
	    `sed -n 's/listen sock: //p' ${@:S/^run/server/}.out`
	grep -q '^success$$' ${@:S/^run/server/}.out || \
	    { sleep 1; grep -q '^success$$' ${@:S/^run/server/}.out; }
	grep -q '^success$$' ${@:S/^run/client/}.out

REGRESS_TARGETS +=	check-cipher-${cipher}-client-${clib}-server-${slib}
check-cipher-${cipher}-client-${clib}-server-${slib}: \
    client-cipher-${cipher}-client-${clib}-server-${slib}.out \
    server-cipher-${cipher}-client-${clib}-server-${slib}.out
	@echo '\n======== $@ ========'
	grep -q ' Cipher *: ${cipher}$$' ${@:S/^check/server/}.out
	grep -q ' Cipher *: ${cipher}$$' ${@:S/^check/client/}.out

.endfor
.endfor
.endfor

.include <bsd.regress.mk>
