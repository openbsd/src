#!/bin/ksh
#
# $OpenBSD: syspatch.sh,v 1.13 2016/09/10 16:19:14 ajacoutot Exp $
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

set -e

trap "rm -rf ${_TMP}; exit 1" 2 3 9 13 15 ERR

sp_err()
{
	echo "${@}" 1>&2 && return 1
}

usage()
{
	sp_err "usage: ${0##*/} [-c | -l | -r]"
}

needs_root()
{
	[[ $(id -u) -ne 0 ]] && sp_err "${0##*/}: need root privileges"
}

apply_patches()
{
	needs_root
	# XXX cleanup mismatch/old rollback patches and sig (installer should as well)
	local _m _patch _patches="$(ls_missing)"
	[[ -n ${_patches} ]] || return 0 # nothing to do

	for _patch in ${_patches}; do
		fetch_and_verify "${_patch}" && \
			install_patch "${_patch}" || return
	done

	# non-fatal: the syspatch tarball should have correct permissions
	for _m in 4.4BSD BSD.x11; do
		mtree -qdef /etc/mtree/${_m}.dist -p / -U >/dev/null || true
	done
}

create_rollback()
{
	local _file _patch=$1 _rbfiles
	[[ -n ${_patch} ]]
	shift
	local _files="${@}"
	[[ -n ${_files} ]]

	for _file in ${_files}; do
		[[ -f /${_file} ]] || continue
		_rbfiles="${_rbfiles} ${_file}"
	done

	(cd / && \
		# GENERIC.MP: substitute bsd.mp->bsd and bsd.sp->bsd
		if ${_BSDMP} && \
			tar -tzf ${_TMP}/${_patch}.tgz bsd >/dev/null 2>&1; then
			tar -czf ${_PDIR}/${_REL}/rollback-${_patch}.tgz \
				-s '/^bsd.mp$//' -s '/^bsd$/bsd.mp/' \
				-s '/^bsd.sp$/bsd/' bsd.sp ${_rbfiles}
		else
			tar -czf ${_PDIR}/${_REL}/rollback-${_patch}.tgz \
				${_rbfiles}
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
	local _bsd=/bsd _kern=$1
	[[ -n ${_kern} ]]

	# we only save the original release kernel once
	[[ -f /bsd.rollback${_RELINT} ]] || \
		install -FSp /bsd /bsd.rollback${_RELINT}

	if ${_BSDMP}; then
		[[ ${_kern##*/} == bsd ]] && _bsd=/bsd.sp
	fi

	if [[ -n ${_bsd} ]]; then
		install -FS ${_kern} ${_bsd}
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
		if [[ ${_file} == @(bsd|bsd.mp) ]]; then
			if ! install_kernel ${_explodir}/${_file}; then
				rollback_patch
				return 1
			fi
		else
			if ! install_file ${_explodir}/${_file} /${_file}; then
				rollback_patch
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
	# no _REL dir = no installed patch
	cd ${_PDIR}/${_REL} 2>/dev/null && set -- * || return 0
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
	needs_root
	local _explodir _file _files _patch

	_patch="$(ls_installed | sort -V | tail -1)"
	[[ -n ${_patch} ]]

	_explodir=${_TMP}/rollback-${_patch}
	mkdir -p ${_explodir}

	_files="$(tar xvzphf ${_PDIR}/${_REL}/rollback-${_patch}.tgz -C ${_explodir})"
	for _file in ${_files}; do
		if [[ ${_file} == @(bsd|bsd.mp) ]]; then
			install_kernel ${_explodir}/${_file}
		else
			install_file ${_explodir}/${_file} /${_file}
		fi
	done

	rm ${_PDIR}/${_REL}/rollback-${_patch}.tgz \
		${_PDIR}/${_REL}/${_patch#syspatch-${_RELINT}-}.patch.sig
}

# we do not run on current
set -A _KERNV -- $(sysctl -n kern.version | \
	sed 's/^OpenBSD \([0-9]\.[0-9]\)\([^ ]*\).*/\1 \2/;q')
[[ -z ${_KERNV[1]} ]] || [[ ${_KERNV[1]} == "-stable" ]]

# check args
[[ $@ == @(|-[[:alpha:]]) ]] || usage

# XXX to be discussed
[[ -n ${PATCH_PATH} ]]
[[ -d ${PATCH_PATH} ]] && PATCH_PATH="file://$(readlink -f ${PATCH_PATH})"

[[ $(sysctl -n hw.ncpu) -gt 1 ]] && _BSDMP=true || _BSDMP=false
readonly _BSDMP
readonly _FETCH="/usr/bin/ftp -MV -k ${FTP_KEEPALIVE-0}"
readonly _PDIR="/var/syspatch"
readonly _REL=${_KERNV[0]}
readonly _RELINT=${_REL%\.*}${_REL#*\.}
readonly _TMP=$(mktemp -d -p /tmp syspatch.XXXXXXXXXX)

while getopts clr arg; do
	case ${arg} in
		c) ls_missing;;
		l) ls_installed;;
		r) rollback_patch;;
		*) usage;;
	esac
done
shift $(( OPTIND -1 ))
[[ $# -ne 0 ]] && usage

if [[ ${OPTIND} == 1 ]]; then
	apply_patches
fi

rm -rf ${_TMP}
