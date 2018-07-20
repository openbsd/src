#!/bin/sh
#
# $OpenBSD: snmpd.sh,v 1.8 2018/07/20 21:59:53 claudio Exp $
#/*
# * Copyright (c) Rob Pierce <rob@openbsd.org>
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

echo "\nConfiguration: default community strings, trap receiver, trap handle\n"

cat > ${OBJDIR}/snmpd.conf <<EOF
# This is config template (1) for snmpd regression testing
# Restrict daemon to listen on localhost only
listen on 127.0.0.1
listen on ::1

# Specify a number of trap receivers
trap receiver localhost

trap handle 1.2.3.4 "/usr/bin/touch ${TMPFILE}"
EOF

(cd ${OBJDIR} && nohup snmpd -dvf ./snmpd.conf > snmpd.log 2>&1) &

sleep ${SLEEP}

[ ! -n "$(pgrep snmpd)" ] && echo "Failed to start snmpd." && fail

# pf (also checks "oid all" which obtains privileged kernel data

pf_enabled="$(pfctl -si | grep ^Status | awk '{ print $2 }' | tr [A-Z] [a-z])"
snmpctl_command="snmpctl snmp walk localhost oid all"
echo ======= $snmpctl_command
enabled="$(eval $snmpctl_command | grep -vi parameters | grep -i pfrunning | awk -F= '{ print $2 }')"
if [ "${PF[$enabled]}" != "${PF[enabled]}" ]
then
	if [ "${PF[$enabled]}" != "${PF[disabled]}" ]
	then
		echo "Retrieval of pf status failed."
		FAILED=1
	fi
fi

# hostname

sys_name=$(hostname)
snmpctl_command="snmpctl snmp get localhost oid 1.3.6.1.2.1.1.5.0"
echo ======= $snmpctl_command
name="$(eval $snmpctl_command | awk -F= '{ print $2 }' | sed 's/"//g')"
if [ "$name" != "$sys_name" ]
then
	echo "Retrieval of hostname failed."
	FAILED=1
fi

# carp allow

carp="$(sysctl net.inet.carp.allow | awk -F= '{ print $2 }')"
snmpctl_command="snmpctl snmp get localhost oid 1.3.6.1.4.1.30155.6.1.1.0"
echo ======= $snmpctl_command
carp_allow="$(eval $snmpctl_command | awk -F= '{ print $2 }')"
if [ "$carp" -ne "$carp_allow" ]
then
	echo "Retrieval of carp.allow failed."
	FAILED=1
fi

# carp allow with default ro community string

carp="$(sysctl net.inet.carp.allow | awk -F= '{ print $2 }')"
snmpctl_command="snmpctl snmp get localhost community public oid 1.3.6.1.4.1.30155.6.1.1.0"
echo ======= $snmpctl_command
carp_allow="$(eval $snmpctl_command | awk -F= '{ print $2 }')"
if [ "$carp" -ne "$carp_allow" ]
then
	echo "Retrieval of carp.allow with default ro cummunity string failed."
	FAILED=1
fi

# trap handler with command execution

rm -f ${TMPFILE}
snmpctl_command="snmpctl trap send 1.2.3.4"
echo ======= $snmpctl_command
eval $snmpctl_command
sleep ${SLEEP}
if [ ! -f "${TMPFILE}" ]
then
	echo "Trap handler test failed."
	FAILED=1
fi

# system.sysContact set with default rw community string

puffy="puffy@openbsd.org"
snmpset_command="snmpset -c private -v 1 localhost system.sysContact.0 s $puffy"
echo ======= $snmpset_command
eval $snmpset_command > /dev/null 2>&1
snmpctl_command="snmpctl snmp get localhost oid 1.3.6.1.2.1.1.4.0"
echo ======= $snmpctl_command
contact="$(eval $snmpctl_command | awk -F= '{ print $2 }' | sed 's/"//g')"
if [ "$contact" !=  "$puffy" ]
then
	echo "Setting with default rw community string failed."
	FAILED=1
fi

kill $(pgrep snmpd) >/dev/null 2>&1
wait

# # # # # CONFIG TWO # # # # #
echo "\nConfiguration: seclevel auth\n"

cat > ${OBJDIR}/snmpd.conf <<EOF
# This is config template (2) for snmpd regression testing
# Restrict daemon to listen on localhost only
listen on 127.0.0.1
listen on ::1

seclevel auth

user "hans" authkey "password123"
EOF

(cd ${OBJDIR} && nohup snmpd -dvf ./snmpd.conf > snmpd.log 2>&1) &

sleep ${SLEEP}

[ ! -n "$(pgrep snmpd)" ] && echo "Failed to start snmpd." && fail

# make sure we can't get an oid with deault community string

snmpctl_command="snmpctl snmp get localhost oid 1.3.6.1.2.1.1.5.0"
echo ======= $snmpctl_command
eval $snmpctl_command > /dev/null 2>&1
if [ $? -eq 0 ]
then
	echo "Non-defaut ro community string test failed."
	FAILED=1
fi

# get with SHA authentication

os="$(uname -s)"
snmpget_command="snmpget -Oq -l authNoPriv -u hans -a SHA -A password123 \
   localhost system.sysDescr.0"
echo ======= $snmpget_command
system="$(eval $snmpget_command | awk '{ print $2 }')"
if [ "$system" != "$os" ]
then
	echo "Retrieval test with seclevel auth and SHA failed."
	FAILED=1
fi

kill $(pgrep snmpd) >/dev/null 2>&1
wait

# # # # # CONFIG THREE # # # # #
echo "\nConfiguration: seclevel enc\n"

cat > ${OBJDIR}/snmpd.conf <<EOF
# This is config template (3) for snmpd regression testing
# Restrict daemon to listen on localhost only
listen on 127.0.0.1
listen on ::1

seclevel enc

user "hans" authkey "password123" enc aes enckey "321drowssap"
EOF

(cd ${OBJDIR} && nohup snmpd -dvf ./snmpd.conf > snmpd.log 2>&1) &

sleep ${SLEEP}

[ ! -n "$(pgrep snmpd)" ] && echo "Failed to start snmpd." && fail

# get with SHA authentication and AES encryption

os="$(uname -s)"
snmpget_command="snmpget -Oq -l authPriv -u hans -a SHA -A password123 -x AES \
   -X 321drowssap localhost system.sysDescr.0"
echo ======= $snmpget_command
system="$(eval $snmpget_command | awk '{ print $2 }')"
if [ "$system" != "$os" ]
then
	echo "seclevel auth with SHA failed"
	FAILED=1
fi

kill $(pgrep snmpd) >/dev/null 2>&1
wait

# # # # # CONFIG FOUR # # # # #
echo "\nConfiguration: non-default community strings, custom oids\n"

cat > ${OBJDIR}/snmpd.conf <<EOF
# This is config template (4) for snmpd regression testing
# Restrict daemon to listen on localhost only
listen on 127.0.0.1
listen on ::1

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
snmpctl_command="snmpctl snmp get localhost community non-default-ro \
   oid 1.3.6.1.4.1.30155.6.1.1.0"
echo ======= $snmpctl_command
carp_allow="$(eval $snmpctl_command | awk -F= '{ print $2 }')"
if [ "$carp" -ne "$carp_allow" ]
then
	echo "Retrieval test with default ro cummunity string failed."
	FAILED=1
fi

# system.sysContact set with non-default rw/ro community strings

puffy="puffy@openbsd.org"
snmpset_command="snmpset -c non-default-rw -v 1 localhost system.sysContact.0 \
   s $puffy"
echo ======= $snmpset_command
eval $snmpset_command > /dev/null 2>&1
snmpctl_command="snmpctl snmp get localhost community non-default-ro \
   oid 1.3.6.1.2.1.1.4.0"
echo ======= $snmpctl_command
contact="$(eval $snmpctl_command | awk -F= '{ print $2 }' | sed 's/"//g')"
if [ "$contact" !=  "$puffy" ]
then
	echo "Setting with default rw community string failed."
	FAILED=1
fi

# custom oids, with a ro that we should not be able to set

snmpctl_command="snmpctl snmp get localhost community non-default-rw \
   oid 1.3.6.1.4.1.30155.42.1.0"
echo ======= $snmpctl_command
string="$(eval $snmpctl_command | awk -F= '{ print $2 }' | sed 's/"//g')"
if [ "$string" !=  "humppa" ]
then
	echo "couldn't get customer oid string"
	FAILED=1
fi

snmpctl_command="snmpctl snmp get localhost community non-default-rw \
   oid 1.3.6.1.4.1.30155.42.2.0"
echo ======= $snmpctl_command
integer="$(eval $snmpctl_command | awk -F= '{ print $2 }' | sed 's/"//g')"
if [ $integer -ne  1 ]
then
	echo "Retrieval of customer oid integer failed."
	FAILED=1
fi

snmpset_command="snmpset -c non-default-rw -v 1 localhost \
   1.3.6.1.4.1.30155.42.1.0 s \"bula\""
echo ======= $snmpset_command
eval $snmpset_command > /dev/null 2>&1
if [ $? -eq 0  ]
then
	echo "Setting of a ro custom oid test unexpectedly succeeded."
	FAILED=1
fi

kill $(pgrep snmpd) >/dev/null 2>&1

case $FAILED in
0)	echo
	cleanup
	exit 0
	;;
1)	fail
	;;
esac
