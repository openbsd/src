#!/bin/ksh
#
# $OpenBSD: syspatch.sh,v 1.66 2016/11/30 15:19:16 ajacoutot Exp $
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

	echo "Applying patch ${_patch##${_OSrev}-}"
	fetch_and_verify "syspatch${_patch}.tgz"

	trap '' INT
	install -d ${_explodir} ${_PDIR}/${_patch}

	_files="$(tar xvzphf ${_TMP}/syspatch${_patch}.tgz -C ${_explodir})"
	checkfs ${_files}

	create_rollback ${_patch} "${_files}"

	for _file in ${_files}; do
		((${_ret} == 0)) || break
		if [[ ${_file} == @(bsd|bsd.mp) ]]; then
			install_kernel ${_explodir}/${_file} || _ret=$?
		else
			install_file ${_explodir}/${_file} /${_file} || _ret=$?
		fi
	done

	if ((${_ret} != 0)); then
		sp_err "Failed to apply patch ${_patch##${_OSrev}-}" 0
		rollback_patch; return ${_ret}
	fi
	trap exit INT
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
		${_BSDMP} || [[ ! -f /bsd.syspatch${_OSrev} ]] &&
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
	shift
	local _files="${@}"
	[[ -n ${_files} ]]

	for _file in ${_files}; do
		[[ -f /${_file} ]] || continue
		# only save the original release kernel once
		if [[ ${_file} == bsd && ! -f /bsd.syspatch${_OSrev} ]]; then
			install -FSp /bsd /bsd.syspatch${_OSrev}
		fi
		_rbfiles="${_rbfiles} ${_file}"
	done

	if ! (cd / &&
		# GENERIC.MP: substitute bsd.mp->bsd and bsd.sp->bsd
		if ${_BSDMP} &&
			tar -tzf ${_TMP}/syspatch${_patch}.tgz bsd >/dev/null \
				2>&1; then
			tar -czf ${_PDIR}/${_patch}/rollback.tgz \
				-s '/^bsd.mp$//' -s '/^bsd$/bsd.mp/' \
				-s '/^bsd.sp$/bsd/' bsd.sp ${_rbfiles}
		else
			tar -czf ${_PDIR}/${_patch}/rollback.tgz \
				${_rbfiles}
		fi
	); then
		rm -r ${_PDIR}/${_patch}
		sp_err "Failed to create rollback patch ${_patch##${_OSrev}-}"
	fi
}

fetch_and_verify()
{
	local _sig=${_TMP}/SHA256.sig _tgz=$1
	[[ -n ${_tgz} ]]

	[[ -f ${_sig} ]] || \
		unpriv -f "${_sig}" ${_FETCH} -o "${_sig}" "${_URL}/SHA256.sig"

	unpriv -f "${_TMP}/${_tgz}" ${_FETCH} -mD "Get/Verify" -o \
		"${_TMP}/${_tgz}" "${_URL}/${_tgz}"

	(cd ${_TMP} && unpriv signify -qC -p \
		/etc/signify/openbsd-${_OSrev}-syspatch.pub -x SHA256.sig \
		${_tgz})
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
	### XXX temporary quirks; remove before 6.1 ############################
	local _r _s _t _u _v
	if [[ -f /bsd.rollback${_OSrev} ]]; then
		[[ $(id -u) -ne 0 ]] && sp_err "${0##*/}: need root privileges"
		mv /bsd.rollback${_OSrev} /bsd.syspatch${_OSrev}
	fi
	if [[ -d ${_PDIR}/${_KERNV[0]} ]]; then
		( cd ${_PDIR}/${_KERNV[0]} && for _r in *; do
			if [[ ${_r} == rollback-syspatch-${_OSrev}-*.tgz ]]; then
				[[ $(id -u) -ne 0 ]] &&
					sp_err "${0##*/}: need root privileges"
				mv ${_r} rollback${_OSrev}${_r#*-syspatch-${_OSrev}}
			fi
		done )
		( cd ${_PDIR}/${_KERNV[0]} && for _s in *; do
			if [[ ${_s} == rollback${_OSrev}-*.tgz ]]; then
				[[ $(id -u) -ne 0 ]] &&
					sp_err "${0##*/}: need root privileges"
				_t=${_s#rollback${_OSrev}-}
				_t=${_t%.tgz}
				mv ${_s} ${_t}.rollback.tgz
			fi
		done )
		( cd ${_PDIR}/${_KERNV[0]} && for _u in *; do
			if [[ ${_u} == *.rollback.tgz ]]; then
				[[ $(id -u) -ne 0 ]] &&
					sp_err "${0##*/}: need root privileges"
				_v=${_u%.rollback.tgz}
				install -d ${_PDIR}/${_OSrev}-${_v}
				mv ${_u} ${_PDIR}/${_OSrev}-${_v}/rollback.tgz
				mv ${_v}.patch.sig ${_PDIR}/${_OSrev}-${_v}/
			fi
		done )
		rmdir ${_PDIR}/${_KERNV[0]}
	fi
	########################################################################
	for _p in ${_PDIR}/*; do
		[[ -f ${_p}/rollback.tgz ]] && echo ${_p##*/${_OSrev}-}
	done | sort -V
}

ls_missing()
{
	# XXX match with installed sets (comp, x...)?
	local _index=${_TMP}/index.txt _installed _p
	_installed="$(ls_installed)"

	unpriv -f "${_index}" ${_FETCH} -o "${_index}" "${_URL}/index.txt"

	for _p in $(grep -o "syspatch${_OSrev}-[0-9][0-9][0-9]_.*" ${_index} |
		sed "s/^syspatch${_OSrev}-//;s/.tgz$//"| sort -V); do
		if [[ -n ${_installed} ]]; then
			echo ${_p} | grep -qw -- "${_installed}" || echo ${_p}
		else
			echo ${_p}
		fi
	done
}

rollback_patch()
{
	local _explodir _file _files _patch _ret=0

	_patch="$(ls_installed | sort -V | tail -1)"
	[[ -n ${_patch} ]]

	_explodir=${_TMP}/${_patch}-rollback
	_patch=${_OSrev}-${_patch}

	echo "Reverting patch ${_patch##${_OSrev}-}"
	install -d ${_explodir}

	_files="$(tar xvzphf ${_PDIR}/${_patch}/rollback.tgz -C ${_explodir})"
	checkfs ${_files} ${_PDIR} # check for read-only /var/syspatch

	for _file in ${_files}; do
		((${_ret} == 0)) || break
		if [[ ${_file} == @(bsd|bsd.mp) ]]; then
			install_kernel ${_explodir}/${_file} || _ret=$?
			# remove the backup kernel if all kernel syspatches have
			# been reverted; non-fatal (`-f')
			cmp -s /bsd /bsd.syspatch${_OSrev} &&
				rm -f /bsd.syspatch${_OSrev}
		else
			install_file ${_explodir}/${_file} /${_file} || _ret=$?
		fi
	done

	rm -r ${_PDIR}/${_patch} && ((${_ret} == 0)) ||
		sp_err "Failed to revert patch ${_patch##${_OSrev}-}" ${_ret}
}

sp_cleanup()
{
	local _d _k _m

	# remove non matching release /var/syspatch/ content
	for _d in ${_PDIR}/*; do
		[[ -e ${_d} ]] || continue
		[[ ${_d##*/} == ${_OSrev}-@([0-9][0-9][0-9])_* ]] || rm -r ${_d}
	done

	# remove non matching release backup kernel
	for _k in /bsd.syspatch*; do
		[[ -f ${_k} ]] || continue
		[[ ${_k} == /bsd.syspatch${_OSrev} ]] || rm ${_k}
	done

	# in case a patch added a new directory (install -D);
	# non-fatal in case some mount point is read-only or remote
	for _m in /etc/mtree/{4.4BSD,BSD.x11}.dist; do
		[[ -f ${_m} ]] && mtree -qdef ${_m} -p / -U >/dev/null || true
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
	(($# >= 1))

	eval su -s /bin/sh ${_user} -c "'$@'"
}

# XXX needs a way to match release <=> syspatch
# only run on release (not -current nor -stable)
set -A _KERNV -- $(sysctl -n kern.version |
	sed 's/^OpenBSD \([0-9]\.[0-9]\)\([^ ]*\).*/\1 \2/;q')
[[ -z ${_KERNV[1]} ]]

[[ $@ == @(|-[[:alpha:]]) ]] || usage; [[ $@ == @(|-(c|r)) ]] &&
	(($(id -u) != 0)) && sp_err "${0##*/}: need root privileges"

[[ $(sysctl -n hw.ncpufound) -gt 1 ]] && _BSDMP=true || _BSDMP=false
_FETCH="ftp -MVk ${FTP_KEEPALIVE-0}"
_OSrev=${_KERNV[0]%\.*}${_KERNV[0]#*\.}
_PDIR="/var/syspatch"
_TMP=$(mktemp -d -p /tmp syspatch.XXXXXXXXXX)
# XXX to be discussed
_URL=http://syspatch.openbsd.org/pub/OpenBSD/${_KERNV[0]}/syspatch/$(machine)
readonly _BSDMP _FETCH _OSrev _PDIR _REL _TMP _URL

trap 'set +e; rm -rf "${_TMP}"' EXIT
trap exit HUP INT TERM

[[ -n ${_OSrev} ]]

while getopts clr arg; do
	case ${arg} in
		c) ls_missing;;
		l) ls_installed;;
		r) rollback_patch;;
		*) usage;;
	esac
done
shift $((OPTIND -1))
[[ $# -ne 0 ]] && usage

if ((${OPTIND} == 1)); then
	for _PATCH in $(ls_missing); do
		apply_patch ${_OSrev}-${_PATCH}
	done
	sp_cleanup
fi
