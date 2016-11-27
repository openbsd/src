#!/bin/ksh
#
# $OpenBSD: syspatch.sh,v 1.63 2016/11/27 11:38:50 ajacoutot Exp $
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
umask 0022

sp_err()
{
	echo "${1}" 1>&2 && return ${2:-1}
}

usage()
{
	sp_err "usage: ${0##*/} [-c | -l | -r]"
}

apply_patch()
{
	local _explodir _file _files _patch=$1 _ret=0
	[[ -n ${_patch} ]]

	_explodir=${_TMP}/${_patch}
	install -d ${_explodir}

	_files="$(tar xvzphf ${_TMP}/${_patch}.tgz -C ${_explodir})"
	checkfs ${_files}

	create_rollback ${_patch} "${_files}"

	for _file in ${_files}; do
		[[ ${_ret} == 0 ]] || break
		if [[ ${_file} == @(bsd|bsd.mp) ]]; then
			install_kernel ${_explodir}/${_file} || _ret=$?
		else
			install_file ${_explodir}/${_file} /${_file} || _ret=$?
		fi
	done

	if [[ ${_ret} != 0 ]]; then
		sp_err "Failed to apply ${_patch} (/${_file})" 0
		rollback_patch; return ${_ret}
	fi
}

apply_patches()
{
	local _patch

	for _patch in $(ls_missing); do
		fetch_and_verify "${_patch}"
		trap '' INT
		apply_patch "${_patch}"
		trap exit INT
	done

	sp_cleanup
}

# quick-and-dirty size check:
# - assume old files are about the same size as new ones
# - ignore new (nonexistent) files
# - compute total size of all files per fs, simpler and less margin for error
# - if we install a kernel, double /bsd size (duplicate it in the list) when:
#   - we are on an MP system (/bsd.mp does not exist there)
#   - /bsd.syspatchXX is not present (create_rollback will copy it from /bsd)
checkfs()
{
	local _d _df _dev _files="${@}" _sz
	[[ -n ${_files} ]]

	if echo "${_files}" | grep -qw bsd; then
		${_BSDMP} || [[ ! -f /bsd.syspatch${_RELINT} ]] &&
			_files="bsd ${_files}"
	fi

	eval $(cd / &&
		stat -qf "_dev=\"\${_dev} %Sd\" %Sd=\"\${%Sd:+\${%Sd}\+}%Uz\"" \
			${_files}) || true # ignore nonexistent files

	for _d in $(printf '%s\n' ${_dev} | sort -u); do
		mount | grep -v read-only | grep -q "^/dev/${_d} " ||
			sp_err "Remote or read-only filesystem, aborting"
		_df=$(df -Pk | grep "^/dev/${_d} " | tr -s ' ' | cut -d ' ' -f4)
		_sz=$(($((${_d}))/1024))
		[[ ${_df} -gt ${_sz} ]] ||
			sp_err "No space left on device ${_d}, aborting"
	done
}

create_rollback()
{
	local _file _patch=$1 _rbfiles
	[[ -n ${_patch} ]]
	local _rbpatch=${_patch#syspatch${_RELINT}-}.rollback.tgz
	shift
	local _files="${@}"
	[[ -n ${_files} ]]

	[[ -d ${_PDIR}/${_REL} ]] || install -d -m 0755 ${_PDIR}/${_REL}

	for _file in ${_files}; do
		[[ -f /${_file} ]] || continue
		# only save the original release kernel once
		if [[ ${_file} == bsd && ! -f /bsd.syspatch${_RELINT} ]]; then
			install -FSp /bsd /bsd.syspatch${_RELINT}
		fi
		_rbfiles="${_rbfiles} ${_file}"
	done

	if ! (cd / &&
		# GENERIC.MP: substitute bsd.mp->bsd and bsd.sp->bsd
		if ${_BSDMP} &&
			tar -tzf ${_TMP}/${_patch}.tgz bsd >/dev/null 2>&1; then
			tar -czf ${_PDIR}/${_REL}/${_rbpatch} \
				-s '/^bsd.mp$//' -s '/^bsd$/bsd.mp/' \
				-s '/^bsd.sp$/bsd/' bsd.sp ${_rbfiles}
		else
			tar -czf ${_PDIR}/${_REL}/${_rbpatch} ${_rbfiles}
		fi
	); then
		rm ${_PDIR}/${_REL}/${_rbpatch}
		sp_err "Failed to create rollback for ${_patch}"
	fi
}

fetch_and_verify()
{
	local _patch=$1 _sig=${_TMP}/SHA256.sig
	[[ -n ${_patch} ]]

	unpriv -f "${_sig}" ${_FETCH} -o "${_sig}" "${PATCH_PATH}/SHA256.sig"
	unpriv -f "${_TMP}/${_patch}.tgz" ${_FETCH} -mD "Applying" -o \
		"${_TMP}/${_patch}.tgz" "${PATCH_PATH}/${_patch}.tgz"
	(cd ${_TMP} && unpriv signify -qC -p \
		/etc/signify/openbsd-${_RELINT}-syspatch.pub -x SHA256.sig \
		${_patch}.tgz)
}

install_file()
{
	# XXX handle symlinks, dir->file, file->dir?
	local _dst=$2 _fgrp _fmode _fown _src=$1
	[[ -f ${_src} && -f ${_dst} ]]

	eval $(stat -f "_fmode=%OMp%OLp _fown=%Su _fgrp=%Sg" ${_src})

	install -DFS -m ${_fmode} -o ${_fown} -g ${_fgrp} ${_src} ${_dst}
}

install_kernel()
{
	local _bsd=/bsd _kern=$1
	[[ -n ${_kern} ]]

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
	### XXX temporary quirks; remove before 6.1
	local _r _s _t
	if [[ ! -d ${_PDIR}/${_REL} ]]; then
		[[ $(id -u) -ne 0 ]] && sp_err "${0##*/}: need root privileges"
		install -d -m 0755 ${_PDIR}/${_REL}
	fi
	if [[ -f /bsd.rollback${_RELINT} ]]; then
		[[ $(id -u) -ne 0 ]] && sp_err "${0##*/}: need root privileges"
		mv /bsd.rollback${_RELINT} /bsd.syspatch${_RELINT}
	fi
	( cd ${_PDIR}/${_REL} && for _r in *; do
		if [[ ${_r} == rollback-syspatch-${_RELINT}-*.tgz ]]; then
			[[ $(id -u) -ne 0 ]] && \
				sp_err "${0##*/}: need root privileges"
			mv ${_r} rollback${_RELINT}${_r#*-syspatch-${_RELINT}}
		fi
	done )
	( cd ${_PDIR}/${_REL} && for _s in *; do
		if [[ ${_s} == rollback${_RELINT}-*.tgz ]]; then
			[[ $(id -u) -ne 0 ]] && \
				sp_err "${0##*/}: need root privileges"
			_t=${_s#rollback${_RELINT}-}
			_t=${_t%.tgz}
			mv ${_s} ${_t}.rollback.tgz
		fi
	done )
	###
	for _p in ${_PDIR}/${_REL}/*.rollback.tgz; do
		[[ -f ${_p} ]] && _p=${_p##*/} &&
			echo syspatch${_RELINT}-${_p%%.*}
	done | sort -V
}

ls_missing()
{
	# XXX match with installed sets (comp, x...)?
	local _a _index=${_TMP}/index.txt _installed
	_installed="$(ls_installed)"
	
	unpriv -f "${_index}" ${_FETCH} -o "${_index}" "${PATCH_PATH}/index.txt"

	for _a in $(sed 's/^.* //;s/^M//;s/.tgz$//' ${_index} |
		grep "^syspatch${_RELINT}-.*$" | sort -V); do
		if [[ -n ${_installed} ]]; then
			echo ${_a} | grep -qw -- "${_installed}" || echo ${_a}
		else
			echo ${_a}
		fi
	done
}

rollback_patch()
{
	local _explodir _file _files _patch _rbpatch _ret=0

	_patch="$(ls_installed | sort -V | tail -1)"
	[[ -n ${_patch} ]]

	_rbpatch=${_patch#syspatch${_RELINT}-}.rollback.tgz
	_explodir=${_TMP}/${_rbpatch%.tgz}

	echo "Reverting ${_patch}"
	install -d ${_explodir}

	_files="$(tar xvzphf ${_PDIR}/${_REL}/${_rbpatch} -C ${_explodir})"
	checkfs ${_files} ${_PDIR}/${_REL} # check for ro /var/syspatch/${OSREV}

	for _file in ${_files}; do
		[[ ${_ret} == 0 ]] || break
		if [[ ${_file} == @(bsd|bsd.mp) ]]; then
			install_kernel ${_explodir}/${_file} || _ret=$?
			# remove the backup kernel if all kernel syspatches have
			# been reverted; non-fatal (`-f')
			cmp -s /bsd /bsd.syspatch${_RELINT} &&
				rm -f /bsd.syspatch${_RELINT}
		else
			install_file ${_explodir}/${_file} /${_file} || _ret=$?
		fi
	done

	# `-f' in case we failed to install *.patch.sig
	rm -f ${_PDIR}/${_REL}/${_patch#syspatch${_RELINT}-}.patch.sig

	rm ${_PDIR}/${_REL}/${_rbpatch} && [[ ${_ret} == 0 ]] ||
		sp_err "Failed to revert ${_patch}" ${_ret}
}

sp_cleanup()
{
	local _d _k _m

	# remove non matching release /var/syspatch/ content
	for _d in ${_PDIR}/*; do
		[[ -e ${_d} ]] || continue
		[[ ${_d##*/} == ${_REL} ]] || rm -r ${_d}
	done

	# remove non matching release backup kernel
	for _k in /bsd.syspatch*; do
		[[ -f ${_k} ]] || continue
		[[ ${_k} == /bsd.syspatch${_RELINT} ]] || rm ${_k}
	done

	# in case a patch added a new directory (install -D);
	# non-fatal in case some mount point is read-only or remote
	for _m in 4.4BSD BSD.x11; do
		mtree -qdef /etc/mtree/${_m}.dist -p / -U >/dev/null || true
	done
}

unpriv()
{
	# XXX use a dedicated user?
	local _file=$2 _user=_pkgfetch

	if [[ $1 == -f && -n ${_file} ]]; then
		>${_file}
		chown "${_user}" "${_file}"
		chmod 0711 ${_TMP}
		shift 2
	fi
	(( $# >= 1 ))

	eval su -s /bin/sh ${_user} -c "'$@'"
}

# XXX needs a way to match release <=> syspatch
# only run on release (not -current nor -stable)
set -A _KERNV -- $(sysctl -n kern.version |
	sed 's/^OpenBSD \([0-9]\.[0-9]\)\([^ ]*\).*/\1 \2/;q')
[[ -z ${_KERNV[1]} ]]

[[ $@ == @(|-[[:alpha:]]) ]] || usage; [[ $@ == @(|-(c|r)) ]] &&
	[[ $(id -u) -ne 0 ]] && sp_err "${0##*/}: need root privileges"

# XXX to be discussed; check for $ARCH?
[[ -d ${PATCH_PATH} ]] && PATCH_PATH="file://$(readlink -f ${PATCH_PATH})"
[[ ${PATCH_PATH%%://*} == @(file|ftp|http|https) ]] ||
	sp_err "No valid PATCH_PATH set"

[[ $(sysctl -n hw.ncpufound) -gt 1 ]] && _BSDMP=true || _BSDMP=false
_FETCH="ftp -MVk ${FTP_KEEPALIVE-0}"
_PDIR="/var/syspatch"
_REL=${_KERNV[0]}
_RELINT=${_REL%\.*}${_REL#*\.}
_TMP=$(mktemp -d -p /tmp syspatch.XXXXXXXXXX)
readonly _BSDMP _FETCH _PDIR _REL _RELINT _TMP

trap 'set +e; rm -rf "${_TMP}"' EXIT
trap exit HUP INT TERM

[[ -n ${_REL} && -n ${_RELINT} ]]

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
