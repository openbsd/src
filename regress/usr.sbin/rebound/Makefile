# $OpenBSD: Makefile,v 1.4 2018/10/31 19:59:37 anton Exp $

REBOUND=	/usr/sbin/rebound

PROG=		rebound-ns
CFLAGS+=	-Wall -Wextra

TESTS=	cache localhost record reload

# IP address prefix.
PREFIX=		172.16.0
# IP address suffixes.
IFS=		100 101 102

.if ! (make(clean) || make(cleandir) || make(obj) || make(rebound-ns))
PREREQ1!=	for v in ${IFS}; do \
			ifconfig lo$$v >/dev/null 2>&1 && echo lo$$v; \
		done; \
		exit 0
PREREQ2!=	pgrep rebound || exit 0

.  if ! empty(PREREQ1)
regress:
	@echo "${PREREQ1}: interface(s) already exists"
	@echo "SKIPPED"
.  elif ! empty(PREREQ2)
regress:
	@echo "rebound: already running"
	@echo "SKIPPED"
.  endif
.endif

REGRESS_SETUP_ONCE=	setup
setup: rebound-ns
	@echo '\n======== ${@} ========'
.for i in ${IFS}
	${SUDO} ifconfig lo${i} create
	${SUDO} ifconfig lo${i} inet ${PREFIX}.${i} netmask 255.255.255.0
.endfor

REGRESS_CLEANUP=	cleanup
cleanup:
	@echo '\n======== ${@} ========'
.for i in ${IFS}
	-${SUDO} ifconfig lo${i} destroy
.endfor

.for t in ${TESTS}
REGRESS_TARGETS+=	$t
${t}:
	@echo '\n======== $t ========'
	${SUDO} sh ${.CURDIR}/run.sh \
		-n ${.OBJDIR}/rebound-ns \
		-r ${REBOUND} \
		${IFS:C/^/-a ${PREFIX}./} \
		-- ${.CURDIR}/$t.sh
.endfor


.include <bsd.regress.mk>
