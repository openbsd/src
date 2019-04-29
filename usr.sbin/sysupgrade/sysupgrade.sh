#!/bin/ksh
#
# $OpenBSD: sysupgrade.sh,v 1.8 2019/04/29 22:27:39 ian Exp $
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
	ug_err "usage: ${0##*/} [-c] [installurl]"
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
		MIRROR=https://cdn.openbsd.org/pub/OpenBSD
	;;
1)	MIRROR=$1
	;;
*)	usage
esac

if [[ ${#_KERNV[*]} == 2 ]]; then
	CURRENT=true
fi
NEXT_VERSION=$(echo ${_KERNV[0]} + 0.1 | bc)

if $CURRENT; then
	URL=${MIRROR}/snapshots/${ARCH}/
else
	URL=${MIRROR}/${NEXT_VERSION}/${ARCH}/
fi

if [[ -e ${SETSDIR} ]]; then
	eval $(stat -s ${SETSDIR})
	[[ $st_uid -eq 0 ]] ||
		 ug_err "${SETSDIR} needs to be owned by root:wheel"
	[[ $st_gid -eq 0 ]] ||
		 ug_err "${SETSDIR} needs to be owned by root:wheel"
	[[ $st_mode -eq 040755 ]] || 
		ug_err "${SETSDIR} is not a directory with permissions 0755"
else
	mkdir -p ${SETSDIR}
fi

cd ${SETSDIR}

unpriv -f SHA256.sig ftp -Vmo SHA256.sig ${URL}SHA256.sig

_KEY=openbsd-${_KERNV[0]%.*}${_KERNV[0]#*.}-base.pub
_NEXTKEY=openbsd-${NEXT_VERSION%.*}${NEXT_VERSION#*.}-base.pub

read _LINE <SHA256.sig
case ${_LINE} in
*\ ${_KEY})	SIGNIFY_KEY=/etc/signify/${_KEY} ;;
*\ ${_NEXTKEY})	SIGNIFY_KEY=/etc/signify/${_NEXTKEY} ;;
*)		ug_err "invalid signing key" ;;
esac

[[ -f ${SIGNIFY_KEY} ]] || ug_err "cannot find ${SIGNIFY_KEY}"

unpriv -f SHA256 signify -Veq -p "${SIGNIFY_KEY}" -x SHA256.sig -m SHA256

# INSTALL.*, bsd*, *.tgz
SETS=$(sed -n -e 's/^SHA256 (\(.*\)) .*/\1/' \
    -e '/^INSTALL\./p;/^bsd/p;/\.tgz$/p' SHA256)

OLD_FILES=$(ls)
OLD_FILES=$(rmel SHA256 $OLD_FILES)
OLD_FILES=$(rmel SHA256.sig $OLD_FILES)
DL=$SETS

for f in $SETS; do
	if cksum -C SHA256 $f >/dev/null 2>&1; then
		DL=$(rmel $f ${DL})
		OLD_FILES=$(rmel $f ${OLD_FILES})
	fi
done

[[ -n ${OLD_FILES} ]] && rm ${OLD_FILES}
for f in ${DL}; do
	unpriv -f $f ftp -Vmo ${f} ${URL}${f}
done

# re-check signature after downloads
echo Verifying sets.
unpriv signify -qC -p "${SIGNIFY_KEY}" -x SHA256.sig ${SETS}

cp bsd.rd /nbsd.upgrade
ln -f /nbsd.upgrade /bsd.upgrade
rm /nbsd.upgrade

echo Upgrading.
exec reboot
