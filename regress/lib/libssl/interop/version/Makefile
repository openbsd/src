# $OpenBSD: Makefile,v 1.2 2020/09/21 15:13:24 beck Exp $

# Connect a client to a server.  Both can be current libressl, or
# openssl 1.0.2, or openssl 1.1.  Pin client or server to a fixed TLS
# version number.  Incompatible versions must fail.  Check that client 
# and server have used correct version by grepping in their session
# print out.

LIBRARIES =		libressl
.if exists(/usr/local/bin/eopenssl)
LIBRARIES +=		openssl
.endif
.if exists(/usr/local/bin/eopenssl11)
LIBRARIES +=		openssl11
.endif

VERSIONS =	any TLS1 TLS1_1 TLS1_2 TLS1_3

.for cver in ${VERSIONS}
.for sver in ${VERSIONS}

.if "${cver}" == any || "${sver}" == any || "${cver}" == "${sver}"
FAIL_${cver}_${sver} =
.else
FAIL_${cver}_${sver} = !
.endif

.for clib in ${LIBRARIES}
.for slib in ${LIBRARIES}

.if ("${clib}" != openssl && "${slib}" != openssl) || \
    ("${cver}" != TLS1_3 && "${sver}" != TLS1_3)

.if ("${clib}" == "libressl" || "${slib}" == "libressl")
REGRESS_TARGETS +=	run-version-client-${clib}-${cver}-server-${slib}-${sver}
.else
REGRESS_SLOW_TARGETS +=	run-version-client-${clib}-${cver}-server-${slib}-${sver}
.endif

run-version-client-${clib}-${cver}-server-${slib}-${sver} \
client-version-client-${clib}-${cver}-server-${slib}-${sver}.out \
server-version-client-${clib}-${cver}-server-${slib}-${sver}.out: \
    127.0.0.1.crt ../${clib}/client ../${slib}/server
	@echo '\n======== $@ ========'
	LD_LIBRARY_PATH=/usr/local/lib/e${slib} \
	    ../${slib}/server >${@:S/^run/server/}.out \
	    -c 127.0.0.1.crt -k 127.0.0.1.key \
	    ${sver:Nany:S/^/-V /} \
	    127.0.0.1 0
	${FAIL_${cver}_${sver}} \
	LD_LIBRARY_PATH=/usr/local/lib/e${clib} \
	    ../${clib}/client >${@:S/^run/client/}.out \
	    ${cver:Nany:S/^/-V /} \
	    `sed -n 's/listen sock: //p' ${@:S/^run/server/}.out`
.if empty(${FAIL_${cver}_${sver}})
	grep -q '^success$$' ${@:S/^run/server/}.out || \
	    { sleep 1; grep -q '^success$$' ${@:S/^run/server/}.out; }
	grep -q '^success$$' ${@:S/^run/client/}.out
.endif

.if empty(${FAIL_${cver}_${sver}})

.if ("${clib}" == "libressl" || "${slib}" == "libressl")
REGRESS_TARGETS +=	check-version-client-${clib}-${cver}-server-${slib}-${sver}
.else
REGRESS_SLOW_TARGETS +=	check-version-client-${clib}-${cver}-server-${slib}-${sver}
.endif

check-version-client-${clib}-${cver}-server-${slib}-${sver}: \
    client-version-client-${clib}-${cver}-server-${slib}-${sver}.out \
    server-version-client-${clib}-${cver}-server-${slib}-${sver}.out
	@echo '\n======== $@ ========'
	@grep ' Protocol *: ' ${@:S/^check/client/}.out
	@grep ' Protocol *: ' ${@:S/^check/server/}.out
.if "${cver}" == any
.if "${sver}" == any
.if "${clib}" == openssl || "${slib}" == openssl
	grep -q ' Protocol *: TLSv1.2$$' ${@:S/^check/client/}.out
	grep -q ' Protocol *: TLSv1.2$$' ${@:S/^check/server/}.out
.else
	grep -q ' Protocol *: TLSv1.3$$' ${@:S/^check/client/}.out
	grep -q ' Protocol *: TLSv1.3$$' ${@:S/^check/server/}.out
.endif
.else
	grep -q ' Protocol *: ${sver:S/TLS/TLSv/:S/_/./}$$' \
	    ${@:S/^check/client/}.out
	grep -q ' Protocol *: ${sver:S/TLS/TLSv/:S/_/./}$$' \
	    ${@:S/^check/server/}.out
.endif
.else
	grep -q ' Protocol *: ${cver:S/TLS/TLSv/:S/_/./}$$' \
	    ${@:S/^check/client/}.out
	grep -q ' Protocol *: ${cver:S/TLS/TLSv/:S/_/./}$$' \
	    ${@:S/^check/server/}.out
.endif
.endif

.endif

.endfor
.endfor
.endfor
.endfor

.include <bsd.regress.mk>
