#!/bin/ksh
#
# $OpenBSD: syspatch.sh,v 1.4 2016/09/05 11:32:28 ajacoutot Exp $
#
# Copyright (c) 2016 Antoine Jacoutot <ajacoutot@openbsd.org>
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

trap "syspatch_trap" 2 3 9 13 15 ERR

set -e

usage()
{
	echo "usage: ${0##*/} [-c | -l | -r patchname ]" >&2 && return 1
}

needs_root()
{
	[[ $(id -u) -eq 0 ]] || \
		(echo "${0##*/}: need root privileges"; return 1)
}

syspatch_sort()
{
	local _p _patch _patches=$(</dev/stdin)
	[[ -n ${_patches} ]] || return 0 # nothing to do

	for _patch in ${_patches}; do
		echo ${_patch##*_}
	done | sort -u | while read _p; do
		echo "${_patches}" | \
			grep -E "syspatch-${_RELINT}-[0-9]{3}_${_p}" | \
			sort -V | tail -1
	done
}

syspatch_trap()
{
	rm -rf ${_TMP}
	exit 1
}

apply_patches()
{
	# XXX cleanup mismatch/old rollback patches and sig (installer should as well)
	local _m _patch _patches="$@"
	[[ -n ${_patches} ]] || return 0 # nothing to do

	for _patch in ${_patches}; do
		fetch_and_verify "${_patch}" || return
		install_patch "${_patch}" || return
	done

	# non-fatal: the syspatch tarball should have correct permissions
	for _m in 4.4BSD BSD.x11; do
		mtree -qdef /etc/mtree/${_m}.dist -p / -U >/dev/null || true
	done
}

create_rollback()
{
	local _patch=$1 _type
	[[ -n ${_patch} ]]
	shift
	local _files="${@}"
	[[ -n ${_files} ]]

	_type=$(tar -tzf ${_TMP}/${_patch}.tgz bsd 2>/dev/null || echo userland)

	_files="$(echo ${_files} \
		| sed "s|var/syspatch/${_REL}/${_patch#syspatch-60-}.patch.sig||g")"

	(cd / && \
		if [[ ${_type} == bsd ]]; then
			# XXX bsd.mp created twice in the tarball
			if ${_BSDMP}; then
				tar -czf ${_PDIR}/${_REL}/rollback-${_patch}.tgz \
					-s '/^bsd$/bsd.mp/' -s '/^bsd.sp$/bsd/' \
					${_files} bsd.sp 2>/dev/null || return # no /bsd.mp
			else
				tar -czf ${_PDIR}/${_REL}/rollback-${_patch}.tgz \
					${_files} || return
			fi
		else
			tar -czf ${_PDIR}/${_REL}/rollback-${_patch}.tgz \
				${_files} || return
		fi
	)
}

fetch_and_verify()
{
	# XXX privsep ala installer
	local _patch="$@"
	[[ -n ${_patch} ]]

	local _key="/etc/signify/openbsd-${_RELINT}-syspatch.pub" _p

	${_FETCH} -o "${_TMP}/SHA256.sig" "${PATCH_PATH}/SHA256.sig"

	for _p in ${_patch}; do
		_p=${_p}.tgz
		 ${_FETCH} -mD "Get/Verify" -o "${_TMP}/${_p}" \
			"${PATCH_PATH}/${_p}"
		(cd ${_TMP} && /usr/bin/signify -qC -p ${_key} -x SHA256.sig ${_p})
	done
}

install_file()
{
	# XXX handle sym/hardlinks?
	# XXX handle dir becoming file and vice-versa?
	local _src=$1 _dst=$2
	[[ -f ${_src} && -f ${_dst} ]]

	local _fmode _fown _fgrp
	eval $(stat -f "_fmode=%OMp%OLp _fown=%Su _fgrp=%Sg" \
		${_src})

	install -DFS -m ${_fmode} -o ${_fown} -g ${_fgrp} \
		${_src} ${_dst}
}

install_kernel()
{
	local _backup=false _bsd=/bsd _kern=$1
	[[ -n ${_kern} ]]

	# we only save the original release kernel once
	[[ -f /bsd.rollback${_RELINT} ]] || _backup=true

	if ${_BSDMP}; then
		[[ ${_kern##*/} == bsd ]] && _bsd=/bsd.sp
	fi

	if ${_backup}; then
		install -FSp /bsd /bsd.rollback${_RELINT} || return
	fi

	if [[ -n ${_bsd} ]]; then
		install -FS ${_kern} ${_bsd} || return
	fi
}

install_patch()
{
	local _explodir _file _files _patch="$1"
	[[ -n ${_patch} ]]

	local _explodir=${_TMP}/${_patch}
	mkdir -p ${_explodir}

	_files="$(tar xvzphf ${_TMP}/${_patch}.tgz -C ${_explodir})"
	create_rollback ${_patch} "${_files}"

	for _file in ${_files}; do
		# can't rely on _type, we need to install 001_foo.patch.sig
		if [[ ${_file} == @(bsd|bsd.mp) ]]; then
			if ! install_kernel ${_explodir}/${_file}; then
				rollback_patch ${_patch}
				return 1
			fi
		else
			if ! install_file ${_explodir}/${_file} /${_file}; then
				rollback_patch ${_patch}
				return 1
			fi
		fi
	done
}

ls_avail()
{
	${_FETCH} -o - "${PATCH_PATH}/index.txt" | sed 's/^.* //;s/^M//;s/.tgz$//' | \
		grep "^syspatch-${_RELINT}-.*$" | sort -V
}

ls_installed()
{
	local _p
	cd ${_PDIR}/${_REL} && set -- * || return 0 # no _REL dir = no patch
	for _p; do
		 [[ ${_p} = rollback-syspatch-${_RELINT}-*.tgz ]] && \
			_p=${_p#rollback-} && echo ${_p%.tgz}
	done | sort -V
}

ls_missing()
{
	local _a _installed
	_installed="$(ls_installed)"

	for _a in $(ls_avail); do
		if [[ -n ${_installed} ]]; then
			echo ${_a} | grep -qw -- "${_installed}" || echo ${_a}
		else
			echo ${_a}
		fi
	done
}

rollback_patch()
{
	local _explodir _file _files _patch=$1 _type
	[[ -n ${_patch} ]]

	_type=$(tar -tzf ${_PDIR}/${_REL}/rollback-${_patch}.tgz bsd \
		2>/dev/null || echo userland)

	# make sure the syspatch is installed and is the latest version
	echo ${_patch} | grep -qw -- "$(ls_installed | syspatch_sort)"

	_explodir=${_TMP}/rollback-${_patch}
	mkdir -p ${_explodir}

	_files="$(tar xvzphf ${_PDIR}/${_REL}/rollback-${_patch}.tgz -C ${_explodir})"
	for _file in ${_files}; do
		if [[ ${_type} == bsd ]]; then
			install_kernel ${_explodir}/${_file} || return
		else
			install_file ${_explodir}/${_file} /${_file} || return
		fi
	done

	rm ${_PDIR}/${_REL}/rollback-${_patch}.tgz \
		${_PDIR}/${_REL}/${_patch#syspatch-${_RELINT}-}.patch.sig
}

# we do not run on current
set -A _KERNV -- $(sysctl -n kern.version | \
	sed 's/^OpenBSD \([0-9]\.[0-9]\)\([^ ]*\).*/\1 \2/;q')
[[ -z ${_KERNV[1]} ]] || [[ ${_KERNV[1]} == "-stable" ]]

# check unallowed args (-ab, -a foo -b, -a -b)
[[ -z $@ || \
	$@ == @(|-[[:alnum:]]@(|+([[:blank:]])[!-]*([![:blank:]])))*([[:blank:]]) ]] || \
	usage

# XXX to be discussed
[[ -n ${PATCH_PATH} ]]
[[ -d ${PATCH_PATH} ]] && PATCH_PATH="file://$(readlink -f ${PATCH_PATH})"

readonly _PDIR="/var/syspatch"
_FETCH="/usr/bin/ftp -MV -k ${FTP_KEEPALIVE-0}"
_REL=${_KERNV[0]}
_RELINT=${_REL%\.*}${_REL#*\.}
_TMP=$(mktemp -d -p /tmp syspatch.XXXXXXXXXX)
[[ $(sysctl -n hw.ncpu) -gt 1 ]] && _BSDMP=true || _BSDMP=false

while getopts clr: arg; do
	case ${arg} in
		c) ls_missing;;
		l) ls_installed;;
		r) needs_root && rollback_patch "${OPTARG}";;
		*) usage;;
	esac
done
shift $(( OPTIND -1 ))
[[ $# -ne 0 ]] && usage

if [[ ${OPTIND} == 1 ]]; then
	needs_root && apply_patches $(ls_missing)
fi

rm -rf ${_TMP}
