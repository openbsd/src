# $OpenBSD: Makefile,v 1.9 2022/02/05 18:34:06 tb Exp $

# Connect a client to a server.  Both can be current libressl, or
# openssl 1.0.2, or openssl 1.1.  Create lists of supported ciphers
# and pin client and server to one of the ciphers.  Use server
# certificate with compatible type.  Check that client and server
# have used correct cipher by grepping in their session print out.

run-cipher-GOST2001-GOST89-GOST89-client-libressl-server-libressl \
run-cipher-GOST2012256-GOST89-GOST89-client-libressl-server-libressl \
client-cipher-GOST2012256-GOST89-GOST89-client-libressl-server-libressl.out \
client-cipher-GOST2001-GOST89-GOST89-client-libressl-server-libressl.out \
server-cipher-GOST2001-GOST89-GOST89-client-libressl-server-libressl.out \
server-cipher-GOST2012256-GOST89-GOST89-client-libressl-server-libressl.out \
check-cipher-GOST2001-GOST89-GOST89-client-libressl-server-libressl \
check-cipher-GOST2012256-GOST89-GOST89-client-libressl-server-libressl:
	# gost does not work with libressl TLS 1.3 right now
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
. if "${clib}" == "openssl11" || "${slib}" == "openssl11"
	# OpenSSL 1.1's SSL_CTX_set_cipher_list doesn't accept TLSv1.3 ciphers
	sed -i '/^TLS_/d' $@.tmp
. endif
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

.if ("${clib}" == "libressl" || "${slib}" == "libressl")
REGRESS_TARGETS +=	run-cipher-${cipher}-client-${clib}-server-${slib}
.else
REGRESS_SLOW_TARGETS +=	run-cipher-${cipher}-client-${clib}-server-${slib}
.endif
run-cipher-${cipher}-client-${clib}-server-${slib} \
client-cipher-${cipher}-client-${clib}-server-${slib}.out \
server-cipher-${cipher}-client-${clib}-server-${slib}.out: dh.param \
    127.0.0.1.crt ${TYPE_${cipher}}.crt ../${clib}/client ../${slib}/server
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

.if ("${clib}" == "libressl" || "${slib}" == "libressl")
REGRESS_TARGETS +=	check-cipher-${cipher}-client-${clib}-server-${slib}
.else
REGRESS_SLOW_TARGETS +=	check-cipher-${cipher}-client-${clib}-server-${slib}
.endif
check-cipher-${cipher}-client-${clib}-server-${slib}: \
    client-cipher-${cipher}-client-${clib}-server-${slib}.out \
    server-cipher-${cipher}-client-${clib}-server-${slib}.out
.if "${clib}" != "openssl" && "${slib}" != "openssl" && \
    "${cipher:C/TLS_(AES.*_GCM|CHACHA.*_POLY.*)_SHA.*/TLS1_3/}" != TLS1_3
	# client and server 1.3 capable, not TLS 1.3 cipher
. if "${clib}" == "libressl"
	# libressl client may prefer chacha-poly if aes-ni is not supported
	egrep -q ' Cipher *: TLS_(AES_256_GCM_SHA384|CHACHA20_POLY1305_SHA256)$$' ${@:S/^check/client/}.out
. else
	# openssl 1.1 generic client cipher
	grep -q ' Cipher *: TLS_AES_256_GCM_SHA384$$' ${@:S/^check/client/}.out
. endif
. if "${clib}" == "libressl"
	# libressl client may prefer chacha-poly if aes-ni is not supported
.  if "${slib}" == "openssl11"
	egrep -q ' Cipher *: TLS_(AES_256_GCM_SHA384|CHACHA20_POLY1305_SHA256)$$' ${@:S/^check/server/}.out
.  else
	egrep -q ' Cipher *: TLS_(AES_256_GCM_SHA384|CHACHA20_POLY1305_SHA256)$$' ${@:S/^check/server/}.out
.  endif
. else
	# generic server cipher
	grep -q ' Cipher *: TLS_AES_256_GCM_SHA384$$' ${@:S/^check/server/}.out
. endif
.else
	grep -q ' Cipher *: ${cipher}$$' ${@:S/^check/client/}.out
	grep -q ' Cipher *: ${cipher}$$' ${@:S/^check/server/}.out
.endif

.endfor
.endfor
.endfor

.include <bsd.regress.mk>
