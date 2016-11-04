#!/bin/ksh
#
# $OpenBSD: syspatch.sh,v 1.39 2016/11/04 16:03:45 ajacoutot Exp $
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

apply_patch()
{
	local _explodir _file _files _patch=$1
	[[ -n ${_patch} ]]

	local _explodir=${_TMP}/${_patch}
	mkdir -p ${_explodir}

	_files="$(tar xvzphf ${_TMP}/${_patch}.tgz -C ${_explodir})"
	checkfs ${_files}

	create_rollback ${_patch} "${_files}"

	for _file in ${_files}; do
		if [[ ${_file} == @(bsd|bsd.mp) ]]; then
			if ! install_kernel ${_explodir}/${_file}; then
				rollback_patch
				sp_err "Failed to apply ${_patch} (/${_file})"
			fi
		else
			if ! install_file ${_explodir}/${_file} /${_file}; then
				rollback_patch
				sp_err "Failed to apply ${_patch} (/${_file})"
			fi
		fi
	done
}

apply_patches()
{
	needs_root
	local _m _patch _patches="$(ls_missing)"

	for _patch in ${_patches}; do
		fetch_and_verify "${_patch}"
		trap "" 2
		apply_patch "${_patch}"
		trap "rm -rf ${_TMP}; exit 1" 2
	done

	sp_cleanup
}

checkfs()
{
	local _files="${@}"
	[[ -n ${_files} ]]

	local _d _f

	for _d in $(stat -qf "%Sd" $(for _f in ${_files}; do echo /${_f%/*}
		done | uniq)); do mount | grep -q "^/dev/${_d} .*read-only" &&
			sp_err "Remote or read-only filesystem, aborting"
	done
}

create_rollback()
{
	local _file _patch=$1 _rbfiles
	[[ -n ${_patch} ]]
	shift
	local _files="${@}"
	[[ -n ${_files} ]]

	[[ -d ${_PDIR}/${_REL} ]] || install -d ${_PDIR}/${_REL}

	for _file in ${_files}; do
		[[ -f /${_file} ]] || continue
		_rbfiles="${_rbfiles} ${_file}"
	done

	if ! (cd / &&
		# GENERIC.MP: substitute bsd.mp->bsd and bsd.sp->bsd
		if ${_BSDMP} &&
			tar -tzf ${_TMP}/${_patch}.tgz bsd >/dev/null 2>&1; then
			tar -czf ${_PDIR}/${_REL}/rollback-${_patch}.tgz \
				-s '/^bsd.mp$//' -s '/^bsd$/bsd.mp/' \
				-s '/^bsd.sp$/bsd/' bsd.sp ${_rbfiles}
		else
			tar -czf ${_PDIR}/${_REL}/rollback-${_patch}.tgz \
				${_rbfiles}
		fi
	); then
		rm ${_PDIR}/${_REL}/rollback-${_patch}.tgz
		sp_err "Failed to create rollback for ${_patch}"
	fi
}

fetch_and_verify()
{
	# XXX privsep ala installer
	local _patch=$1
	[[ -n ${_patch} ]]

	local _key="/etc/signify/openbsd-${_RELINT}-syspatch.pub" _p

	${_FETCH} -o "${_TMP}/SHA256.sig" "${PATCH_PATH}/SHA256.sig"
	${_FETCH} -mD "Applying" -o "${_TMP}/${_patch}.tgz" \
		"${PATCH_PATH}/${_patch}.tgz"
	(cd ${_TMP} &&
		/usr/bin/signify -qC -p ${_key} -x SHA256.sig ${_patch}.tgz)
}

install_file()
{
	# XXX handle symlinks, dir->file, file->dir?
	local _src=$1 _dst=$2
	[[ -f ${_src} && -f ${_dst} ]]

	local _fmode _fown _fgrp
	eval $(stat -f "_fmode=%OMp%OLp _fown=%Su _fgrp=%Sg" ${_src})

	install -DFS -m ${_fmode} -o ${_fown} -g ${_fgrp} ${_src} ${_dst}
}

install_kernel()
{
	local _bsd=/bsd _kern=$1
	[[ -n ${_kern} ]]

	# only save the original release kernel once
	[[ -f /bsd.rollback${_RELINT} ]] ||
		install -FSp /bsd /bsd.rollback${_RELINT}

	if ${_BSDMP}; then
		[[ ${_kern##*/} == bsd ]] && _bsd=/bsd.sp
	fi

	if [[ -n ${_bsd} ]]; then
		install -FS ${_kern} ${_bsd}
	fi
}

ls_installed()
{
	local _p
	for _p in ${_PDIR}/${_REL}/*; do
		_p=${_p:##*/}
		[[ ${_p} = rollback-syspatch-${_RELINT}-*.tgz ]] &&
			_p=${_p#rollback-} && echo ${_p%.tgz}
	done | sort -V
}

ls_missing()
{
	# XXX match with installed sets (comp, x...)?
	local _a _installed
	_installed="$(ls_installed)"

	${_FETCH} -o "${_TMP}/index.txt" "${PATCH_PATH}/index.txt"

	for _a in $(sed 's/^.* //;s/^M//;s/.tgz$//' ${_TMP}/index.txt |
		grep "^syspatch-${_RELINT}-.*$" | sort -V); do
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

	echo "Reverting ${_patch}"
	_explodir=${_TMP}/rollback-${_patch}
	mkdir -p ${_explodir}

	_files="$(tar xvzphf ${_PDIR}/${_REL}/rollback-${_patch}.tgz -C \
		${_explodir})"
	checkfs ${_files}

	for _file in ${_files}; do
		if [[ ${_file} == @(bsd|bsd.mp) ]]; then
			install_kernel ${_explodir}/${_file} ||
				sp_err "Failed to revert ${_patch} (/${_file})"
		else
			install_file ${_explodir}/${_file} /${_file} ||
				sp_err "Failed to revert ${_patch} (/${_file})"
		fi
	done

	rm ${_PDIR}/${_REL}/rollback-${_patch}.tgz \
		${_PDIR}/${_REL}/${_patch#syspatch-${_RELINT}-}.patch.sig

	sp_cleanup
}

sp_cleanup()
{
	local _d _k _m

	# remove non matching release /var/syspatch/ content
	for _d in ${_PDIR}/*; do
		[[ -e ${_d} ]] || continue
		[[ ${_d:##*/} == ${_REL} ]] || rm -r ${_d}
	done

	# remove non matching release rollback kernel
	for _k in /bsd.rollback*; do
		[[ -f ${_k} ]] || continue
		[[ ${_k} == /bsd.rollback${_RELINT} ]] || rm ${_k}
	done

	# remove rollback kernel if all kernel syspatches have been reverted
	cmp -s /bsd /bsd.rollback${_RELINT} && rm /bsd.rollback${_RELINT}

	# non-fatal: the syspatch|rollback tarball should have correct perms
	for _m in 4.4BSD BSD.x11; do
		mtree -qdef /etc/mtree/${_m}.dist -p / -U >/dev/null || true
	done
}

# only run on release (not -current nor -stable)
set -A _KERNV -- $(sysctl -n kern.version |
	sed 's/^OpenBSD \([0-9]\.[0-9]\)\([^ ]*\).*/\1 \2/;q')
[[ -z ${_KERNV[1]} ]]

# check args
[[ $@ == @(|-[[:alpha:]]) ]] || usage

# XXX to be discussed; check for $ARCH?
[[ -d ${PATCH_PATH} ]] && PATCH_PATH="file://$(readlink -f ${PATCH_PATH})"
[[ ${PATCH_PATH:%%://*} == @(file|ftp|http|https) ]] ||
	sp_err "No valid PATCH_PATH set"

[[ $(sysctl -n hw.ncpufound) -gt 1 ]] && _BSDMP=true || _BSDMP=false
_FETCH="/usr/bin/ftp -MVk ${FTP_KEEPALIVE-0}"
_PDIR="/var/syspatch"
_REL=${_KERNV[0]}
_RELINT=${_REL%\.*}${_REL#*\.}
_TMP=$(mktemp -d -p /tmp syspatch.XXXXXXXXXX)
readonly _BSDMP _FETCH _PDIR _REL _RELINT _TMP
[[ -n ${_REL} && -n ${_RELINT} ]]

trap "rm -rf ${_TMP}; exit 1" 2 3 9 13 15 ERR

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

[[ ${OPTIND} != 1 ]] || apply_patches

rm -rf ${_TMP}
