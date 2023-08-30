#	$OpenBSD: Makefile,v 1.7 2023/08/30 16:45:57 kn Exp $

BIOCTL ?=		/sbin/bioctl

CHUNKFILE =		disk.img
CHUNKDEVFILE =		chunk.txt
VOLDEVFILE =		vol.txt
ROUNDSFILE =		rounds.txt

OLDPW =			oldsecret
NEWPW =			securenew
NROUNDS ?=		17

REGRESS_SETUP_ONCE =	create-chunk

create-chunk:
	dd if=/dev/zero of=${CHUNKFILE} bs=512k count=0 seek=1 status=none
	${SUDO} vnconfig -- ${CHUNKFILE} 1>${CHUNKDEVFILE}
	echo 'RAID *' | ${SUDO} disklabel -wAT- -- "$$(<${CHUNKDEVFILE})"


SUCCESS_TESTS =			scripted-create-volume \
				scripted-recreate-volume \
				scripted-change-passphrase \
				verify-increased-rounds
REGRESS_TARGETS =		${SUCCESS_TESTS}
REGRESS_ROOT_TARGETS =		${SUCCESS_TESTS}

scripted-create-volume:
	printf '%s\n' '${OLDPW}' | \
		${SUDO} ${BIOCTL} -s -cC -l"$$(<${CHUNKDEVFILE})a" -- softraid0 | \
		grep -o 'sd[0-9]*$$' -- 1>${VOLDEVFILE}

scripted-recreate-volume: detach-volume
	printf '%s\n' '${OLDPW}' | \
		${SUDO} ${BIOCTL} -s -Cforce -r${NROUNDS} -cC -l"$$(<${CHUNKDEVFILE})a" -- softraid0 | \
		grep -o 'sd[0-9]*$$' -- 1>${VOLDEVFILE}

scripted-change-passphrase:
	printf '%s\n%s\n' '${OLDPW}' '${NEWPW}' | \
		${SUDO} ${BIOCTL} -s -P -v -- "$$(<${VOLDEVFILE})" | \
		tee -- ${ROUNDSFILE}

verify-increased-rounds:
	awk -safe -vOLD=${NROUNDS} -- '\
		NR == 1 { old = $$7 }; \
		NR == 2 { new = $$7 }; \
		END     { exit !(OLD == old && old <= new) }' ${ROUNDSFILE}


FAILURE_TESTS =			reuse-active-chunk \
				set-empty-passphrase
REGRESS_EXPECTED_FAILURES +=	${FAILURE_TESTS}
REGRESS_TARGETS +=		${FAILURE_TESTS}
REGRESS_ROOT_TARGETS +=		${FAILURE_TESTS}

reuse-active-chunk:
	${SUDO} ${BIOCTL} -cC -l"$$(<${CHUNKDEVFILE})a" -- softraid0

set-empty-passphrase:
	printf '\n' | ${SUDO} ${BIOCTL} -s -P -- "$$(<${VOLDEVFILE})"


CLEANFILES =		*.txt *.img
REGRESS_CLEANUP =	cleanup

detach-volume:
	${SUDO} ${BIOCTL} -d -- "$$(<${VOLDEVFILE})" 2>/dev/null
cleanup: detach-volume
	${SUDO} vnconfig  -u -- "$$(<${CHUNKDEVFILE})" 2>/dev/null

.include <bsd.regress.mk>
