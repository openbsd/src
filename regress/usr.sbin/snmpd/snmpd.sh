#!/bin/sh
#
# $OpenBSD: snmpd.sh,v 1.3 2017/08/12 00:13:13 rob Exp $
#/*
# * Copyright (c) Rob Pierce <rob@2keys.ca>
# *
# * Permission to use, copy, modify, and distribute this software for any
# * purpose with or without fee is hereby granted, provided that the above
# * copyright notice and this permission notice appear in all copies.
# *
# * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
# */

# Basic snmpd regression script.

export OBJDIR

FAILED=0
SLEEP=1
PF[0]="disabled"
PF[1]="enabled"

# This file will be creatred by traphandler.c as user _snmpd
TMPFILE=$(mktemp -q /tmp/_snmpd_traptest.XXXXXX)

trap 'skip' INT

if [ "$(pgrep snmpd)" ]
then
	echo "The snmpd daemon is already running."
	echo SKIPPED
	exit 0
fi

cleanup() {
	rm ${TMPFILE} >/dev/null 2>&1
	rm ${OBJDIR}/nohup.out >/dev/null 2>&1
	rm ${OBJDIR}/snmpd.log >/dev/null 2>&1
	rm ${OBJDIR}/snmpd.conf >/dev/null 2>&1
}

fail() {
	echo FAILED
	cleanup
	exit 1
}

skip() {
	echo SKIPPED
	cleanup
	exit 0
}

# # # # # CONFIG ONE # # # # #

cat > ${OBJDIR}/snmpd.conf <<EOF
# This is config template (1) for snmpd regression testing
listen_addr="127.0.0.1"

# Restrict daemon to listen on localhost only
listen on \$listen_addr

# Specify a number of trap receivers
trap receiver localhost

trap handle 1.2.3.4 "/usr/bin/touch ${TMPFILE}"
EOF

(cd ${OBJDIR} && nohup snmpd -dvf ./snmpd.conf > snmpd.log 2>&1) &

sleep ${SLEEP}

[ ! -n "$(pgrep snmpd)" ] && echo "Failed to start snmpd." && fail

# pf (also checks "oid all" which obtains privileged kernel data

pf_enabled="$(pfctl -si | grep ^Status | awk '{ print $2 }' | tr [A-Z] [a-z])"
enabled="$(snmpctl snmp walk localhost oid all | grep -vi parameters | \
   grep -i pfrunning | awk -F= '{ print $2 }')"
if [ ${PF[$enabled]} != ${PF[enabled]} ]
then
	if [ ${PF[$enabled]} != ${PF[disabled]} ]
	then
		echo "Retrieval of pf status failed."
		FAILED=1
	fi
fi

# hostname

sys_name=$(hostname)
name="$(snmpctl snmp get localhost oid 1.3.6.1.2.1.1.5.0 | \
   awk -F= '{ print $2 }' | sed 's/"//g')"
if [ $name != $sys_name ]
then
	echo "Retrieval of hostname failed."
	FAILED=1
fi

# carp allow

carp="$(sysctl net.inet.carp.allow | awk -F= '{ print $2 }')"
carp_allow="$(snmpctl snmp get localhost oid 1.3.6.1.4.1.30155.6.1.1.0 | \
   awk -F= '{ print $2 }')"
if [ "$carp" -ne "$carp_allow" ]
then
	echo "Retrieval of carp.allow failed."
	FAILED=1
fi

# carp allow with default ro community string

carp="$(sysctl net.inet.carp.allow | awk -F= '{ print $2 }')"
carp_allow="$(snmpctl snmp get localhost community public \
   oid 1.3.6.1.4.1.30155.6.1.1.0 | awk -F= '{ print $2 }')"
if [ "$carp" -ne "$carp_allow" ]
then
	echo "Retrieval of carp.allow with default ro cummunity string failed."
	FAILED=1
fi

# trap handler with command execution

rm -f ${TMPFILE}
snmpctl trap send 1.2.3.4
sleep ${SLEEP}
if [ ! -f ${TMPFILE} ]
then
	echo "Trap handler test failed."
	FAILED=1
fi

# system.sysContact set with default rw community string

puffy="puffy@openbsd.org"
snmpset -c private -v 1 localhost system.sysContact.0 s $puffy \
   > /dev/null 2>&1
contact="$(snmpctl snmp get localhost oid 1.3.6.1.2.1.1.4.0 | \
   awk -F= '{ print $2 }' | sed 's/"//g')"
if [ "$contact" !=  "$puffy" ]
then
	echo "Setting with default rw community string failed."
	FAILED=1
fi

kill $(pgrep snmpd) >/dev/null 2>&1
wait

# # # # # CONFIG TWO # # # # #

cat > ${OBJDIR}/snmpd.conf <<EOF
# This is config template (2) for snmpd regression testing
listen_addr="127.0.0.1"

# Restrict daemon to listen on localhost only
listen on \$listen_addr

seclevel auth

user "hans" authkey "password123"
EOF

(cd ${OBJDIR} && nohup snmpd -dvf ./snmpd.conf > snmpd.log 2>&1) &

sleep ${SLEEP}

[ ! -n "$(pgrep snmpd)" ] && echo "Failed to start snmpd." && fail

# make sure we can't get an oid with deault community string

snmpctl snmp get localhost oid 1.3.6.1.2.1.1.5.0 > /dev/null 2>&1
if [ $? -eq 0 ]
then
	echo "Non-defaut ro community string test failed."
	fail=1
fi

# get with SHA authentication

os="$(uname -s)"
system="$(snmpget -Oq -l authNoPriv -u hans -a SHA -A password123 localhost \
   system.sysDescr.0 | awk '{ print $2 }')"
if [ "$system" != "$os" ]
then
	echo "Retrieval test with seclevel auth and SHA failed."
	fail=1
fi

kill $(pgrep snmpd) >/dev/null 2>&1
wait

# # # # # CONFIG THREE # # # # #

cat > ${OBJDIR}/snmpd.conf <<EOF
# This is config template (3) for snmpd regression testing
listen_addr="127.0.0.1"

# Restrict daemon to listen on localhost only
listen on \$listen_addr

seclevel enc

user "hans" authkey "password123" enc aes enckey "321drowssap"
EOF

(cd ${OBJDIR} && nohup snmpd -dvf ./snmpd.conf > snmpd.log 2>&1) &

sleep ${SLEEP}

[ ! -n "$(pgrep snmpd)" ] && echo "Failed to start snmpd." && fail

# get with SHA authentication and AES encryption

os="$(uname -s)"
system="$(snmpget -Oq -l authPriv -u hans -a SHA -A password123 -x AES \
   -X 321drowssap localhost system.sysDescr.0 | awk '{ print $2 }')"
if [ "$system" != "$os" ]
then
	echo "seclevel auth with SHA failed"
	fail=1
fi

kill $(pgrep snmpd) >/dev/null 2>&1
wait

# # # # # CONFIG FOUR # # # # #

cat > ${OBJDIR}/snmpd.conf <<EOF
# This is config template (4) for snmpd regression testing
listen_addr="127.0.0.1"

# Restrict daemon to listen on localhost only
listen on \$listen_addr

read-only community non-default-ro

read-write community non-default-rw

oid 1.3.6.1.4.1.30155.42.1 name myName read-only string "humppa"
oid 1.3.6.1.4.1.30155.42.2 name myStatus read-only integer 1
EOF

(cd ${OBJDIR} && nohup snmpd -dvf ./snmpd.conf > snmpd.log 2>&1) &

sleep ${SLEEP}

[ ! -n "$(pgrep snmpd)" ] && echo "Failed to start snmpd." && fail

# carp allow with non-default ro community string

carp="$(sysctl net.inet.carp.allow | awk -F= '{ print $2 }')"
carp_allow="$(snmpctl snmp get localhost community non-default-ro \
   oid 1.3.6.1.4.1.30155.6.1.1.0 | awk -F= '{ print $2 }')"
if [ "$carp" -ne "$carp_allow" ]
then
	echo "Retrieval test with default ro cummunity string failed."
	FAILED=1
fi

# system.sysContact set with non-default rw/ro community strings

puffy="puffy@openbsd.org"
snmpset -c non-default-rw -v 1 localhost system.sysContact.0 s $puffy \
   > /dev/null 2>&1
contact="$(snmpctl snmp get localhost community non-default-ro \
   oid 1.3.6.1.2.1.1.4.0 | awk -F= '{ print $2 }' | sed 's/"//g')"
if [ "$contact" !=  "$puffy" ]
then
	echo "Setting with default rw community string failed."
	FAILED=1
fi

# custom oids, with a ro that we should not be able to set

string="$(snmpctl snmp get localhost community non-default-rw \
   oid 1.3.6.1.4.1.30155.42.1.0 | awk -F= '{ print $2 }' | sed 's/"//g')"
if [ "$string" !=  "humppa" ]
then
	echo "couldn't get customer oid string"
	FAILED=1
fi

integer="$(snmpctl snmp get localhost community non-default-rw \
   oid 1.3.6.1.4.1.30155.42.2.0 | awk -F= '{ print $2 }' | sed 's/"//g')"
if [ $integer -ne  1 ]
then
	echo "Retrieval of customer oid integer failed."
	FAILED=1
fi

snmpset -c non-default-rw -v 1 localhost 1.3.6.1.4.1.30155.42.1 s "bula" \
   > /dev/null 2>&1
if [ $? -eq 0 ]
then
	echo "Setting of a ro custom oid test unexpectedly succeeded."
	fail=1
fi

kill $(pgrep snmpd) >/dev/null 2>&1

case $FAILED in
0)	echo PASSED
	cleanup
	exit 0
	;;
1)	fail
	;;
esac
