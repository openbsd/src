# $OpenBSD: Makefile,v 1.1 2018/05/09 19:34:53 anton Exp $

REBOUND=	/usr/sbin/rebound

PROG=		rebound-ns
CFLAGS+=	-Wall -Wextra

# IP address prefix.
PREFIX=		172.16.0
# IP address suffixes.
IFS=		1 2 3

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
.  else
.BEGIN:
.    for i in ${IFS}
	-${SUDO} ifconfig lo${i} create
	-${SUDO} ifconfig lo${i} inet ${PREFIX}.${i} netmask 255.255.255.0
.    endfor

.END:
.    for i in ${IFS}
	-${SUDO} ifconfig lo${i} destroy
.    endfor
.  endif
.endif

REGRESS_TARGETS+=	cache localhost record reload

.SUFFIXES: .sh

.sh: rebound-ns
	@echo '\n======== ${@} ========'
	${SUDO} sh ${.CURDIR}/run.sh \
		-n ${.OBJDIR}/rebound-ns -r ${REBOUND} \
		${IFS:C/^/-a ${PREFIX}./} \
		-- ${.CURDIR}/${@}.sh

.include <bsd.regress.mk>
