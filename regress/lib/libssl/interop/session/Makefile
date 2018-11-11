# $OpenBSD: Makefile,v 1.3 2018/11/11 07:39:35 bluhm Exp $

LIBRARIES =		libressl
.if exists(/usr/local/bin/eopenssl)
LIBRARIES +=		openssl
.endif
.if exists(/usr/local/bin/eopenssl11)
LIBRARIES +=		openssl11
.endif

run-session-client-openssl11-server-openssl11:
	@echo '\n======== $@ ========'
	# TLS 1.3 needs some extra setup for session reuse
	@echo DISABLED

.for clib in ${LIBRARIES}
.for slib in ${LIBRARIES}

REGRESS_TARGETS +=	run-session-client-${clib}-server-${slib}

run-session-client-${clib}-server-${slib}: \
    127.0.0.1.crt ../${clib}/client ../${slib}/server
	@echo '\n======== $@ ========'
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
