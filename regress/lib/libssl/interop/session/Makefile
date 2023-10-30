# $OpenBSD: Makefile,v 1.9 2023/10/30 17:15:21 tb Exp $

LIBRARIES =		libressl
.if exists(/usr/local/bin/eopenssl11)
LIBRARIES +=		openssl11
.endif
.if exists(/usr/local/bin/eopenssl30)
#LIBRARIES +=		openssl30
.endif
.if exists(/usr/local/bin/eopenssl31)
#LIBRARIES +=		openssl31
.endif

run-session-client-libressl-server-libressl \
run-session-client-libressl-server-openssl11 \
run-session-client-openssl11-server-libressl \
run-session-client-openssl11-server-openssl11:
	# TLS 1.3 needs some extra setup for session reuse
	@echo DISABLED

.for clib in ${LIBRARIES}
.for slib in ${LIBRARIES}

REGRESS_TARGETS +=	run-session-client-${clib}-server-${slib}

run-session-client-${clib}-server-${slib}: \
    127.0.0.1.crt ../${clib}/client ../${slib}/server
	LD_LIBRARY_PATH=/usr/local/lib/e${slib} \
	    ../${slib}/server >${@:S/^run/server/}.out \
	    -ss \
	    127.0.0.1 0
	LD_LIBRARY_PATH=/usr/local/lib/e${clib} \
	    ../${clib}/client >${@:S/^run/client/}.out \
	    -ss \
	    `sed -n 's/listen sock: //p' ${@:S/^run/server/}.out`
	grep '^success$$' ${@:S/^run/server/}.out || \
	    { sleep 1; grep '^success$$' ${@:S/^run/server/}.out; }
	grep '^success$$' ${@:S/^run/client/}.out
	grep '^session 2: new$$' ${@:S/^run/server/}.out
	grep '^session 2: new$$' ${@:S/^run/client/}.out
	grep '^session 1: reuse$$' ${@:S/^run/server/}.out
	grep '^session 1: reuse$$' ${@:S/^run/client/}.out
	grep '^session 0: reuse$$' ${@:S/^run/server/}.out
	grep '^session 0: reuse$$' ${@:S/^run/client/}.out

.endfor
.endfor

.include <bsd.regress.mk>
