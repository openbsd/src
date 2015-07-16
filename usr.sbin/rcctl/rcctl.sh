#!/bin/sh
#
# $OpenBSD: rcctl.sh,v 1.72 2015/07/16 21:52:16 ajacoutot Exp $
#
# Copyright (c) 2014, 2015 Antoine Jacoutot <ajacoutot@openbsd.org>
# Copyright (c) 2014 Ingo Schwarze <schwarze@openbsd.org>
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

_special_svcs="accounting check_quotas ipsec multicast_host multicast_router pf
               spamd_black"
readonly _special_svcs

# get local functions from rc.subr(8)
FUNCS_ONLY=1
. /etc/rc.d/rc.subr
_rc_parse_conf

usage()
{
	_rc_err "usage: ${0##*/} [-df] action|get|getdef|ls|order|set
		[service | daemon [variable [arguments]] | daemons | lsarg]"
}

needs_root()
{
	[ "$(id -u)" -ne 0 ] && _rc_err "${0##*/} $1: need root privileges"
}

rcctl_err()
{
	_rc_err "${0##*/}: ${1}" ${2}
}

ls_rcscripts() {
	local _s

	cd /etc/rc.d && set -- *
	for _s; do
		[ "${_s}" = "rc.subr" ] && continue
		[ ! -d "${_s}" ] && echo "${_s}"
	done
}

pkg_scripts_append()
{
	local _svc=$1
	[ -n "${_svc}" ] || return

	rcconf_edit_begin
	if [ -z "${pkg_scripts}" ]; then
		echo pkg_scripts="${_svc}" >>${_TMP_RCCONF}
	elif ! echo ${pkg_scripts} | grep -qw -- ${_svc}; then
		grep -v "^pkg_scripts.*=" /etc/rc.conf.local >${_TMP_RCCONF}
		echo pkg_scripts="${pkg_scripts} ${_svc}" >>${_TMP_RCCONF}
	fi
	rcconf_edit_end
}

pkg_scripts_order()
{
	local _svcs="$*"
	[ -n "${_svcs}" ] || return

	needs_root ${action}
	local _pkg_scripts _svc
	for _svc in ${_svcs}; do
		if svc_is_base ${_svc} || svc_is_special ${_svc}; then
			rcctl_err "${_svc} is not a pkg script"
		elif ! svc_get ${_svc} status; then
			rcctl_err "${_svc} is not enabled"
		fi
	done
	_pkg_scripts=$(echo "${_svcs} ${pkg_scripts}" | tr "[:blank:]" "\n" | \
		     awk -v ORS=' ' '!x[$0]++')
	rcconf_edit_begin
	grep -v "^pkg_scripts.*=" /etc/rc.conf.local >${_TMP_RCCONF}
	echo pkg_scripts=${_pkg_scripts} >>${_TMP_RCCONF}
	rcconf_edit_end
}

pkg_scripts_rm()
{
	local _svc=$1
	[ -n "${_svc}" ] || return

	[ -z "${pkg_scripts}" ] && return

	rcconf_edit_begin
	sed "/^pkg_scripts[[:>:]]/{s/[[:<:]]${_svc}[[:>:]]//g
	    s/['\"]//g;s/ *= */=/;s/   */ /g;s/ $//;/=$/d;}" \
	    /etc/rc.conf.local >${_TMP_RCCONF}
	rcconf_edit_end
}

rcconf_edit_begin()
{
	_TMP_RCCONF=$(mktemp -p /etc -t rc.conf.local.XXXXXXXXXX) || \
		rcctl_err "cannot create temporary file under /etc"
	if [ -f /etc/rc.conf.local ]; then
		cat /etc/rc.conf.local >${_TMP_RCCONF} || \
			rcctl_err "cannot append to ${_TMP_RCCONF}"
	else
		touch /etc/rc.conf.local || \
			rcctl_err "cannot create /etc/rc.conf.local"
	fi
}

rcconf_edit_end()
{
	sort -u -o ${_TMP_RCCONF} ${_TMP_RCCONF} || \
		rcctl_err "cannot modify ${_TMP_RCCONF}"
	cat ${_TMP_RCCONF} >/etc/rc.conf.local || \
		rcctl_err "cannot append to /etc/rc.conf.local"
	if [ ! -s /etc/rc.conf.local ]; then
		rm /etc/rc.conf.local || \
			rcctl_err "cannot remove /etc/rc.conf.local"
	fi
	rm -f ${_TMP_RCCONF}
	_rc_parse_conf # reload new values
}

svc_is_avail()
{
	local _svc=$1
	[ -n "${_svc}" ] || return

	[ -x "/etc/rc.d/${_svc}" ] && return 0
	svc_is_special ${_svc}
}

svc_is_base()
{
	local _svc=$1
	[ -n "${_svc}" ] || return

	grep -E 'start_daemon[[:space:]]+[[:alnum:]]' /etc/rc | \
		cut -d ' ' -f2- | grep -qw -- ${_svc}
}

svc_is_special()
{
	local _svc=$1
	[ -n "${_svc}" ] || return

	echo ${_special_svcs} | grep -qw -- ${_svc}
}

svc_ls()
{
	local _lsarg=$1
	[ -n "${_lsarg}" ] || return

	# we do not want to return the "status" nor the rc.d(8) script retcode
	local _ret=0 _on _svc _started

	case ${_lsarg} in
		all)
			(
				ls_rcscripts
				echo ${_special_svcs} | tr "[:blank:]" "\n"
			) | sort
			;;
		faulty)
			for _svc in $(svc_ls on); do
				svc_is_base ${_svc} && \
					! /etc/rc.d/${_svc} check >/dev/null && \
					echo ${_svc} && _ret=1
			done
			;;
		off|on)
			for _svc in $(svc_ls all); do
				svc_get ${_svc} status && _on=1
					[ "${_lsarg}" = "on" -a -n "${_on}" ] || \
						[ "${_lsarg}" = "off" -a -z "${_on}" ] && \
					echo ${_svc}
				unset _on
			done
			;;
		started|stopped)
			for _svc in $(ls_rcscripts); do
				/etc/rc.d/${_svc} check >/dev/null && _started=1
				[ "${_lsarg}" = "started" -a -n "${_started}" ] || \
					[ "${_lsarg}" = "stopped" -a -z "${_started}" ] && \
					echo ${_svc}
				unset _started
			done
			;;
		*)
			_ret=1
	esac

	return ${_ret}
}

svc_get()
{
	local _svc=$1
	[ -n "${_svc}" ] || return

	local _status=0 _val _var=$2
	local daemon_class daemon_flags daemon_timeout daemon_user

	if svc_is_special ${_svc}; then
		daemon_flags="$(eval echo \${${_svc}})"
	else
		# set pkg daemon_flags to "NO" to match base svc
		if ! svc_is_base ${_svc}; then
			if ! echo ${pkg_scripts} | grep -qw -- ${_svc}; then
				daemon_flags="NO"
			fi
		fi

		# these are expensive, make sure they are explicitely requested
		if [ -z "${_var}" -o "${_var}" = "class" ]; then
			getcap -f /etc/login.conf ${_svc} 1>/dev/null 2>&1 && \
				daemon_class=${_svc}
			[ -z "${daemon_class}" ] && \
				daemon_class="$(svc_getdef ${_svc} class)"
		fi
		if [[ -z ${_var} || ${_var} == @(flags|status) ]]; then
			[ -z "${daemon_flags}" ] && \
				daemon_flags="$(eval echo \"\${${_svc}_flags}\")"
			[ -z "${daemon_flags}" ] && \
				daemon_flags="$(svc_getdef ${_svc} flags)"
		fi
		if [ -z "${_var}" -o "${_var}" = "timeout" ]; then
			[ -z "${daemon_timeout}" ] && \
				daemon_timeout="$(eval echo \"\${${_svc}_timeout}\")"
			[ -z "${daemon_timeout}" ] && \
				daemon_timeout="$(svc_getdef ${_svc} timeout)"
		fi
		if [ -z "${_var}" -o "${_var}" = "user" ]; then
			[ -z "${daemon_user}" ] && \
				daemon_user="$(eval echo \"\${${_svc}_user}\")"
			[ -z "${daemon_user}" ] && \
				daemon_user="$(svc_getdef ${_svc} user)"
		fi
	fi

	[ "${daemon_flags}" = "NO" ] && _status=1

	if [ -n "${_var}" ]; then
		[ "${_var}" = "status" ] && return ${_status}
		eval _val=\${daemon_${_var}}
		[ -z "${_val}" ] || print -r -- "${_val}"
	else
		if svc_is_special ${_svc}; then
			echo "${_svc}=${daemon_flags}"
		else
			echo "${_svc}_class=${daemon_class}"
			echo "${_svc}_flags=${daemon_flags}"
			echo "${_svc}_timeout=${daemon_timeout}"
			echo "${_svc}_user=${daemon_user}"
		fi
		return ${_status}
	fi
}

# to prevent namespace pollution, only call in a subshell
svc_getdef()
{
	local _svc=$1
	[ -n "${_svc}" ] || return

	local _status=0 _val _var=$2
	local daemon_class daemon_flags daemon_timeout daemon_user

	if svc_is_special ${_svc}; then
		# unconditionally parse: we always output flags and/or status
		_rc_parse_conf /etc/rc.conf
		daemon_flags="$(eval echo \${${_svc}})"
		[ "${daemon_flags}" = "NO" ] && _status=1
	else
		if ! svc_is_base ${_svc}; then
			_status=1 # all pkg_scripts are off by default
		else
			
			# abuse /etc/rc.conf behavior of only setting flags
			# to empty or "NO" to get our default status;
			# we'll get our default flags from the rc.d script
			[[ -z ${_var} || ${_var} == status ]] && \
				_rc_parse_conf /etc/rc.conf
			[ "$(eval echo \${${_svc}_flags})" = "NO" ] && _status=1
		fi

		rc_cmd() { }
		. /etc/rc.d/${_svc} >/dev/null 2>&1

		daemon_class=daemon
		[ -z "${daemon_timeout}" ] && daemon_timeout=30
		[ -z "${daemon_user}" ] && daemon_user=root
	fi

	if [ -n "${_var}" ]; then
		[ "${_var}" = "status" ] && return ${_status}
		eval _val=\${daemon_${_var}}
		[ -z "${_val}" ] || print -r -- "${_val}"
	else
		if svc_is_special ${_svc}; then
			echo "${_svc}=${daemon_flags}"
		else
			echo "${_svc}_class=${daemon_class}"
			echo "${_svc}_flags=${daemon_flags}"
			echo "${_svc}_timeout=${daemon_timeout}"
			echo "${_svc}_user=${daemon_user}"
		fi
		return ${_status}
	fi
}

svc_rm()
{
	local _svc=$1
	[ -n "${_svc}" ] || return

	rcconf_edit_begin
	if svc_is_special ${_svc}; then
		grep -v "^${_svc}.*=" /etc/rc.conf.local >${_TMP_RCCONF}
		( svc_getdef ${_svc} status ) && \
			echo "${_svc}=NO" >>${_TMP_RCCONF}
	else
		grep -Ev "^${_svc}_(flags|timeout|user).*=" \
			/etc/rc.conf.local >${_TMP_RCCONF}
		( svc_getdef ${_svc} status ) && \
			echo "${_svc}_flags=NO" >>${_TMP_RCCONF}
	fi
	rcconf_edit_end
}

svc_set()
{
	local _svc=$1 _var=$2
	[ -n "${_svc}" -a -n "${_var}" ] || return

	shift 2
	local _args="$*"

	# don't check if we are already enabled or disabled because rc.conf(8)
	# defaults may have changed in which case we may have a matching
	# redundant entry in rc.conf.local that we can drop
	if [ "${_var}" = "status" ]; then
		if [ "${_args}" = "on" ]; then
			_var="flags"
			# keep our flags if we're already enabled
			eval "_args=\"\${${_svc}_${_var}}\""
			[ "${_args}" = "NO" ] && unset _args
			if ! svc_is_base ${_svc} && ! svc_is_special ${_svc}; then
				pkg_scripts_append ${_svc}
			fi
		elif [ "${_args}" = "off" ]; then
			if ! svc_is_base ${_svc} && ! svc_is_special ${_svc}; then
				pkg_scripts_rm ${_svc}
			fi
			svc_rm ${_svc}
			return
		else
			rcctl_err "invalid status \"${_args}\""
		fi
	else
		svc_get ${_svc} status || \
			rcctl_err "${svc} is not enabled"
	fi

	if svc_is_special ${_svc}; then
		[ "${_var}" = "flags" ] || return
		rcconf_edit_begin
		grep -v "^${_svc}.*=" /etc/rc.conf.local >${_TMP_RCCONF}
		( svc_getdef ${_svc} status ) || \
			echo "${_svc}=YES" >>${_TMP_RCCONF}
		rcconf_edit_end
		return
	fi

	if [ -n "${_args}" ]; then
		if [ "${_var}" = "timeout" ]; then
			[[ ${_args} != +([[:digit:]]) || ${_args} -le 0 ]] && \
				rcctl_err "\"${_args}\" is not a positive integer"
		fi
		if [ "${_var}" = "user" ]; then
			getent passwd "${_args}" >/dev/null || \
				rcctl_err "user \"${_args}\" does not exist"
		fi
		# unset flags if they match the default enabled ones
		[ "${_args}" = "$(svc_getdef ${_svc} ${_var})" ] && \
			unset _args
	fi

	# protect leading whitespace
	[ "${_args}" = "${_args# }" ] || _args="\"${_args}\""

	# reset: value may have changed
	unset ${_svc}_${_var}

	rcconf_edit_begin
	grep -v "^${_svc}_${_var}.*=" /etc/rc.conf.local >${_TMP_RCCONF}
	if [ -n "${_args}" ] || \
	   ( svc_is_base ${_svc} && ! svc_getdef ${_svc} status && [ "${_var}" == "flags" ] ); then
		echo "${_svc}_${_var}=${_args}" >>${_TMP_RCCONF}
	fi
	rcconf_edit_end
}

unset _RC_DEBUG _RC_FORCE
while getopts "df" c; do
	case "$c" in
		d) _RC_DEBUG=-d;;
		f) _RC_FORCE=-f;;
		*) usage;;
	esac
done
shift $((OPTIND-1))
[ $# -gt 0 ] || usage

action=$1
if [ "${action}" = "ls" ]; then
	lsarg=$2
elif [ "${action}" = "order" ]; then
	shift 1
	svcs="$*"
else
	svc=$2
	var=$3
	[ $# -ge 3 ] && shift 3 || shift $#
	args="$*"
fi

[ -n "${lsarg}" ] && [[ ${lsarg} != @(all|faulty|off|on|started|stopped) ]] && \
	usage

if [ -n "${svc}" ]; then
	[[ ${action} == @(disable|enable|get|getdef|set|start|stop|restart|reload|check) ]] || \
		usage
	svc_is_avail ${svc} || \
		rcctl_err "service ${svc} does not exist" 2
elif [[ ${action} != @(getall|ls|order) ]] ; then
	usage
fi

if [ -n "${var}" ]; then
	[[ ${var} != @(class|flags|status|timeout|user) ]] && usage
	[[ ${action} == set && ${var} = flags && ${args} = NO ]] && \
		rcctl_err "\"flags NO\" contradicts \"${action}\""
	[[ ${action} == set && ${var} == class ]] && \
		rcctl_err "\"${svc}_class\" is a read-only variable set in login.conf(5)"
	if svc_is_special ${svc}; then
		if [[ ${action} == set && ${var} != status ]] || \
			[[ ${action} == @(get|getdef) && ${var} == @(timeout|user) ]]; then
			rcctl_err "\"${svc}\" is a special variable, cannot \"${action} ${svc} ${var}\""
		fi
	fi
elif [ ${action} = "set" ]; then
	usage
fi

case ${action} in
	disable) # undocumented, deprecated
		needs_root ${action}
		svc_set ${svc} status off
		;;
	enable) # undocumented, deprecated
		needs_root ${action}
		svc_set ${svc} status on
		;;
	get)
		svc_get ${svc} "${var}"
		;;
	getall) # undocumented, deprecated
		for i in $(svc_ls all); do svc_get ${i}; done
		;;
	getdef)
		( svc_getdef ${svc} "${var}" )
		;;
	ls)
		# some rc.d(8) scripts need root for rc_check()
		[[ ${lsarg} == @(started|stopped|faulty) ]] && needs_root
		svc_ls ${lsarg}
		;;
	order)
		if [ -n "${svcs}" ]; then
			needs_root ${action}
			pkg_scripts_order ${svcs}
		else
			[[ -z ${pkg_scripts} ]] || echo ${pkg_scripts}
		fi
		;;
	set)
		needs_root ${action}
		svc_set ${svc} "${var}" "${args}"
		;;
	start|stop|restart|reload|check)
		if svc_is_special ${svc}; then
			rcctl_err "\"${svc}\" is a special variable, no rc.d(8) script"
		fi
		/etc/rc.d/${svc} ${_RC_DEBUG} ${_RC_FORCE} ${action}
		;;
	*)
		usage
		;;
esac
