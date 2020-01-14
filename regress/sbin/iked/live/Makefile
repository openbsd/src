#	$OpenBSD: Makefile,v 1.1 2020/01/14 11:16:36 tobhe Exp $

# Copyright (c) 2020 Tobias Heider <tobhe@openbsd.org>
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

REGRESS_CLEANUP =	cleanup
CLEANFILES =		*.conf

SSHLEFT ?=
SSHRIGHT ?=
LEFTGW ?=
RIGHTGW ?=

.if empty (SSHLEFT) || empty (SSHRIGHT) || empty (LEFTGW) || empty (RIGHTGW)
regress:
	@echo this test needs two remote machines to operate
	@echo SSHLEFT SSHRIGHT RIGHTGW LEFTGW are not defined
	@echo SKIPPED
.endif

TEST_FLOWS = \
	success=false; \
	count=0; \
	while [[ $$count -le 3 ]]; do \
		sasleft=`ssh ${SSHLEFT} ipsecctl -sa`; \
		flowleft=`echo "$$sasleft" \
		    | sed -n "/^flow $$flowtype in from ${RIGHTGW} to ${LEFTGW}/p"`; \
		sasright=`ssh ${SSHRIGHT} ipsecctl -sa`; \
		flowright=`echo "$$sasright" \
		    | sed -n "/^flow $$flowtype in from ${LEFTGW} to ${RIGHTGW}/p"`; \
		if [[ -n "$$flowleft" && -n "$$flowright" ]]; then \
			 success=true; \
			 break; \
		fi; \
		let count=$$count+1; \
	done; \
	if [[ "$$success" = false ]]; then \
		echo "error: SAs not found : $$sasleft $$sasright"; \
		exit 1; \
	fi

TEST_PING = \
	dump=`ssh ${SSHLEFT} "tcpdump -c2 -i enc0 -w '/tmp/test.pcap' > /dev/null & \
	     ping -c 5 ${RIGHTGW} > /dev/null && tcpdump -r /tmp/test.pcap" && rm -f /tmp/test.pcap`; \
	rtol=`echo "$$dump" \
	    | sed -n "/(authentic,confidential): SPI 0x[0-9a-f]\{8\}: ${LEFTGW} > ${RIGHTGW}/p"`; \
	ltor=`echo "$$dump" \
	    | sed -n "/(authentic,confidential): SPI 0x[0-9a-f]\{8\}: ${RIGHTGW} > ${LEFTGW}/p"`; \
	if [[ -z "$$rtol" || -z "$$ltor" ]]; then \
		echo "error: no esp traffic."; \
		exit 1; \
	fi; \
	echo "$$dump"

SETUP_CONFIGS = \
	psk=`openssl rand -hex 20`; \
	echo "LEFTGW=\"${LEFTGW}\"" > $@_left.conf; \
	echo "RIGHTGW=\"${RIGHTGW}\"" >> $@_left.conf; \
	ipcomp=""; \
	if [[ "$$flowtype" = "ipcomp" ]]; then \
		ipcomp="ipcomp"; \
	fi; \
	echo "IPCOMP=\"$$ipcomp\"" >> $@_left.conf; \
	echo "PSK=\"$$psk\"" >> $@_left.conf; \
	cat ${.CURDIR}/iked.in >> $@_left.conf; \
	chmod 0600 $@_left.conf; \
	echo "cd /tmp\nrm test.conf\nput $@_left.conf test.conf" | sftp -q ${SSHLEFT}; \
	echo "LEFTGW=\"${RIGHTGW}\"" > $@_right.conf; \
	echo "RIGHTGW=\"${LEFTGW}\"" >> $@_right.conf; \
	echo "IPCOMP=\"$$ipcomp\"" >> $@_right.conf; \
	echo "PSK=\"$$psk\"" >> $@_right.conf; \
	cat ${.CURDIR}/iked.in >> $@_right.conf; \
	chmod 0600 $@_right.conf; \
	echo "cd /tmp\nrm test.conf\nput $@_right.conf test.conf" | sftp -q ${SSHRIGHT}

SETUP_SYSCTL = \
	ssh ${SSHLEFT} "sysctl net.inet.ipcomp.enable=1"; \
	ssh ${SSHRIGHT} "sysctl net.inet.ipcomp.enable=1"

SETUP_START = \
	ssh ${SSHLEFT} "ipsecctl -F; pkill iked; iked -6 -f /tmp/test.conf"; \
	ssh ${SSHRIGHT} "ipsecctl -F; pkill iked; iked -6 -f /tmp/test.conf"

cleanup:
	-ssh ${SSHLEFT} 'rm -f /tmp/test.conf; ipsecctl -F; pkill iked'
	-ssh ${SSHRIGHT} 'rm -f /tmp/test.conf; ipsecctl -F; pkill iked'

REGRESS_TARGETS = 	run-basic run-ipcomp

run-basic:
	@echo '======= $@ ========'
	flowtype=esp; \
	${SETUP_CONFIGS}; \
	${SETUP_START}; \
	${TEST_FLOWS}; \
	${TEST_PING}

run-ipcomp:
	@echo '======= $@ ========'
	flowtype=ipcomp; \
	${SETUP_CONFIGS}; \
	${SETUP_SYSCTL}; \
	${SETUP_START}; \
	${TEST_FLOWS}; \
	${TEST_PING}

.include <bsd.regress.mk>
