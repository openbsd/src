#!/bin/ksh
#
# $OpenBSD: sysupgrade.sh,v 1.2 2019/04/25 22:12:11 naddy Exp $
#
# Copyright (c) 1997-2015 Todd Miller, Theo de Raadt, Ken Westerback
# Copyright (c) 2015 Robert Peichaer <rpe@openbsd.org>
# Copyright (c) 2016, 2017 Antoine Jacoutot <ajacoutot@openbsd.org>
# Copyright (c) 2019 Christian Weisgerber <naddy@openbsd.org>
# Copyright (c) 2019 Florian Obser <florian@openbsd.org>
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

set -e
umask 0022

ARCH=$(uname -m)
SETSDIR=/home/_sysupgrade

ug_err()
{
	echo "${1}" 1>&2 && return ${2:-1}
}

usage()
{
	ug_err "usage: ${0##*/} [-c] [install URL]"
}

unpriv()
{
	local _file=$2 _user=_syspatch

	if [[ $1 == -f && -n ${_file} ]]; then
		>${_file}
		chown "${_user}" "${_file}"
		shift 2
	fi
	(($# >= 1))

	eval su -s /bin/sh ${_user} -c "'$@'"
}

# Remove all occurrences of first argument from list formed by the remaining
# arguments.
rmel() {
	local _a=$1 _b _c

	shift
	for _b; do
		[[ $_a != "$_b" ]] && _c="${_c:+$_c }$_b"
	done
	echo -n "$_c"
}

CURRENT=false

while getopts c arg; do
        case ${arg} in
        c)      CURRENT=true;;
        *)      usage;;
        esac
done

set -A _KERNV -- $(sysctl -n kern.version |
	sed 's/^OpenBSD \([0-9]\)\.\([0-9]\)\([^ ]*\).*/\1.\2 \3/;q')

shift $(( OPTIND -1 ))

case $# in
0)	MIRROR=$(sed 's/#.*//;/^$/d' /etc/installurl) 2>/dev/null ||
		installurl=https://cdn.openbsd.org/pub/OpenBSD
	;;
1)	MIRROR=$1
	;;
*)	usage
esac

if [[ ${#_KERNV[*]} == 2 ]]; then
	CURRENT=true
else
	NEXT_VERSION=$(echo ${_KERNV[0]} + 0.1 | bc -l)
fi

if $CURRENT; then
	URL=${MIRROR}/snapshots/${ARCH}/
else
	URL=${MIRROR}/${NEXT_VERSION}/${ARCH}/
fi

# XXX be more paranoid who owns this directory

mkdir -p ${SETSDIR}
cd ${SETSDIR}

unpriv -f SHA256.sig ftp -Vmo SHA256.sig ${URL}SHA256.sig

# XXX run this unpriv?
SIGNIFY_KEY=/etc/signify/openbsd-$(sed -n \
	's/^SHA256 (base\([0-9]\{2,3\}\)\.tgz) .*/\1/p' SHA256.sig)-base.pub

[[ -f ${SIGNIFY_KEY} ]] || ug_err "cannot find ${SIGNIFY_KEY}"

unpriv signify -qV -p "${SIGNIFY_KEY}" -x SHA256.sig -e -m /dev/null

# INSTALL.*, bsd*, *.tgz
SETS=$(sed -n -e 's/^SHA256 (\(.*\)) .*/\1/' \
    -e "/^INSTALL\./p;/^bsd/p;/\.tgz\$/p" SHA256.sig)

OLD_FILES=$(ls)
OLD_FILES=$(rmel SHA256.sig $OLD_FILES)
DL=$SETS

for f in $SETS; do
	signify -C -p "${SIGNIFY_KEY}" -x SHA256.sig $f \
	    >/dev/null 2>&1 && {
		DL=$(rmel $f ${DL})
		OLD_FILES=$(rmel $f ${OLD_FILES})
	}
done

[[ -n ${OLD_FILES} ]] && rm ${OLD_FILES}
for f in ${DL}; do
	unpriv -f $f ftp -Vmo ${f} ${URL}${f}
done

unpriv signify -C -p "${SIGNIFY_KEY}" -x SHA256.sig ${SETS}

cp bsd.rd /nbsd.upgrade
ln /nbsd.upgrade /bsd.upgrade
rm /nbsd.upgrade

exec reboot
