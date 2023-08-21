#	$OpenBSD: Makefile,v 1.4 2023/08/21 16:58:14 kn Exp $

BIOCTL ?=		/sbin/bioctl

CHUNKFILE =		disk.img
CHUNKDEVFILE =		chunk.txt
VOLDEVFILE =		vol.txt

OLDPW =			oldsecret
NEWPW =			securenew

REGRESS_SETUP_ONCE =	create-chunk

create-chunk:
	dd if=/dev/zero of=${CHUNKFILE} bs=512k count=0 seek=1 status=none
	${SUDO} vnconfig -- ${CHUNKFILE} 1>${CHUNKDEVFILE}
	echo 'RAID *' | ${SUDO} disklabel -wAT- -- "$$(<${CHUNKDEVFILE})"


SUCCESS_TESTS =			scripted-create-volume \
				scripted-change-passphrase
REGRESS_TARGETS =		${SUCCESS_TESTS}
REGRESS_ROOT_TARGETS =		${SUCCESS_TESTS}

scripted-create-volume:
	printf '%s\n' '${OLDPW}' | \
		${SUDO} ${BIOCTL} -s -cC -l"$$(<${CHUNKDEVFILE})a" -- softraid0 | \
		grep -o 'sd[0-9]*$$' -- 1>${VOLDEVFILE}

scripted-change-passphrase:
	printf '%s\n%s\n' '${OLDPW}' '${NEWPW}' | \
		${SUDO} ${BIOCTL} -s -P -- "$$(<${VOLDEVFILE})"


FAILURE_TESTS =			recreate-existing-volume \
				set-empty-passphrase
REGRESS_EXPECTED_FAILURES +=	${FAILURE_TESTS}
REGRESS_TARGETS +=		${FAILURE_TESTS}
REGRESS_ROOT_TARGETS +=		${FAILURE_TESTS}

recreate-existing-volume:
	${SUDO} ${BIOCTL} -cC -l"$$(<${CHUNKDEVFILE})a" -- softraid0

set-empty-passphrase:
	printf '\n' | ${SUDO} ${BIOCTL} -s -P -- "$$(<${VOLDEVFILE})"


CLEANFILES =		*.txt *.img
REGRESS_CLEANUP =	cleanup

cleanup:
	${SUDO} ${BIOCTL} -d -- "$$(<${VOLDEVFILE})" 2>/dev/null
	${SUDO} vnconfig  -u -- "$$(<${CHUNKDEVFILE})" 2>/dev/null

.include <bsd.regress.mk>
