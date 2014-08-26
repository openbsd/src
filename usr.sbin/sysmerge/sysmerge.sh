#!/bin/ksh -
#
# $OpenBSD: sysmerge.sh,v 1.152 2014/08/26 21:29:56 ajacoutot Exp $
#
# Copyright (c) 2008-2014 Antoine Jacoutot <ajacoutot@openbsd.org>
# Copyright (c) 1998-2003 Douglas Barton <DougB@FreeBSD.org>
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
#

umask 0022

unset AUTO_INSTALLED_FILES BATCHMODE DIFFMODE EGSUM ETCSUM
unset NEED_NEWALIASES NEWGRP NEWUSR NEED_REBOOT NOSIGCHECK PKGMODE
unset PKGSUM TGZ XETCSUM XTGZ

# forced variables
WRKDIR=$(mktemp -d -p ${TMPDIR:=/var/tmp} sysmerge.XXXXXXXXXX) || exit 1
SWIDTH=$(stty size | awk '{w=$2} END {if (w==0) {w=80} print w}')
RELINT=$(uname -r | tr -d '.')
if [ -z "${VISUAL}" ]; then
	EDIT="${EDITOR:=/usr/bin/vi}"
else
	EDIT="${VISUAL}"
fi

# sysmerge specific variables (overridable)
MERGE_CMD="${MERGE_CMD:=sdiff -as -w ${SWIDTH} -o}"
REPORT="${REPORT:=${WRKDIR}/sysmerge.log}"
DBDIR="${DBDIR:=/usr/share/sysmerge}"

# system-wide variables (overridable)
PAGER="${PAGER:=/usr/bin/more}"

# restore sum files from backups or remove the newly generated ones if
# they did not exist
restore_sum() {
	local i _i
	for i in ${DESTDIR}/${DBDIR}/.{${ETCSUM},${XETCSUM},${EGSUM},${PKGSUM}}.bak; do
		_i=$(basename ${i} .bak)
		if [ -f "${i}" ]; then
			mv ${i} ${DESTDIR}/${DBDIR}/${_i#.}
		elif [ -f "${DESTDIR}/${DBDIR}/${_i#.}" ]; then
			rm ${DESTDIR}/${DBDIR}/${_i#.}
		fi
	done
	rm -f ${WRKDIR}/*sum
}

usage() {
	echo "usage: ${0##*/} [-bdS] [-p | [-x xetcXX.tgz]]" >&2
}

warn() {
	echo "**** WARNING: $@"
}

report() {
	echo "$@" >> ${REPORT}
}

# from OpenBSD's /etc/rc,v 1.438
stripcom() {
	local _file="$1"
	local _line

	{
		while read _line ; do
			_line=${_line%%#*}		# strip comments
			test -z "$_line" && continue
			echo $_line
		done
	} < $_file
}

# remove newly created work directory and exit with status 1
error_rm_wrkdir() {
	(($#)) && echo "!!!! ERROR: $@"
	restore_sum
	# do not remove the entire WRKDIR in case sysmerge stopped half
	# way since it contains our backup files
	rm -rf ${TEMPROOT}
	rm -f ${WRKDIR}/*.tgz
	rm -f ${WRKDIR}/SHA256.sig
	rmdir ${WRKDIR} 2>/dev/null
	exit 1
}

trap "error_rm_wrkdir; exit 1" 1 2 3 13 15

if (($(id -u) != 0)); then
	usage
	error_rm_wrkdir "need root privileges"
fi

# extract (x)etcXX.tgz and create cksum file(s);
# stores sum filename in ETCSUM or XETCSUM (see eval);
extract_sets() {
	[[ -n ${PKGMODE} ]] && return
	local _e _x _set _tgz

	[[ -f ${WRKDIR}/${TGZ##*/} ]] && _e=etc
	[[ -f ${WRKDIR}/${XTGZ##*/} ]] && _x=xetc

	for _set in ${_e} ${_x}; do
		typeset -u _SETSUM=${_set}sum
		eval ${_SETSUM}=${_set}sum
		[[ ${_set} == etc ]] && _tgz=${WRKDIR}/${TGZ##*/}
		[[ ${_set} == xetc ]] && _tgz=${WRKDIR}/${XTGZ##*/}

		tar -tzf "${_tgz}" .${DBDIR}/${_set}sum >/dev/null ||
			error_rm_wrkdir "${_tgz##*/}: badly formed \"${_set}\" set, lacks .${DBDIR}/${_set}sum"

		(cd ${TEMPROOT} && tar -xzphf "${_tgz}" && \
			find . -type f | xargs sha256 -h ${WRKDIR}/${_set}sum) || \
			error_rm_wrkdir "failed to extract ${_tgz} and create checksum file"
		rm "${_tgz}"
	done
}

# fetch and verify sets, abort on failure
sm_fetch_and_verify() {
	[[ -n ${PKGMODE} ]] && return
	local _file _url;
	local _key="/etc/signify/openbsd-${RELINT}-base.pub"

	for _url in ${TGZ} ${XTGZ}; do
		[[ -f ${_url} ]] && _url="file://$(readlink -f ${_url})"
		_file=${WRKDIR}/${_url##*/}
		[[ ${_url} == @(file|ftp|http|https)://*/*[!/] ]] ||
			error_rm_wrkdir "${_url}: invalid URL"
		echo "===> Fetching ${_url}"
		/usr/bin/ftp -Vm -k "${FTP_KEEPALIVE-0}" -o "${_file}" "${_url}" >/dev/null || \
			error_rm_wrkdir "could not retrieve ${_url##*/}"
	done
	if [ -z "${NOSIGCHECK}" -a -n "${XTGZ}" ]; then
		echo "===> Fetching ${XTGZ%/*}/SHA256.sig"
		/usr/bin/ftp -Vm -k "${FTP_KEEPALIVE-0}" -o "${WRKDIR}/SHA256.sig" "${XTGZ%/*}/SHA256.sig" >/dev/null || \
			error_rm_wrkdir "could not retrieve SHA256.sig"
		echo "===> Verifying ${XTGZ##*/} against ${_key}"
		(cd ${WRKDIR} && /usr/bin/signify -qC -p ${_key} -x SHA256.sig ${XTGZ##*/}) || \
			error_rm_wrkdir "${XTGZ##*/}: signature/checksum failed"
	fi

	[[ -z ${NOSIGCHECK} ]] && rm ${WRKDIR}/SHA256.sig
}

# get pkg @sample information
exec_espie() {
	TEMPROOT=${TEMPROOT} /usr/bin/perl <<'EOF'
use strict;
use warnings;

package OpenBSD::PackingElement;

sub walk_sample
{
}

package OpenBSD::PackingElement::Sampledir;
sub walk_sample
{
	my $item = shift;
	print "0-DIR", " ",
	      $item->{owner} // "root", " ",
	      $item->{group} // "wheel", " ",
	      $item->{mode} // "0755", " ",
	      $ENV{'TEMPROOT'}, $item->fullname,
	      "\n";
}

package OpenBSD::PackingElement::Sample;
sub walk_sample
{
	my $item = shift;
	print "1-FILE", " ",
	      $item->{owner} // "root", " ",
	      $item->{group} // "wheel", " ",
	      $item->{mode} // "0644", " ",
	      $item->{copyfrom}->fullname, " ",
	      $ENV{'TEMPROOT'}, $item->fullname,
	      "\n";
}

package main;
use OpenBSD::PackageInfo;
use OpenBSD::PackingList;

for my $i (installed_packages()) {
	my $plist = OpenBSD::PackingList->from_installation($i);
	$plist->walk_sample();
}
EOF
}

copy_pkg_samples() {
	[[ -z ${PKGMODE} ]] && return

	local _install_args _l _ret _sample
	PKGSUM=pkgsum

	# access to base system hierarchy is implied in packages
	mtree -qdef ${DESTDIR}/etc/mtree/4.4BSD.dist -p ${TEMPROOT} -U >/dev/null
	mtree -qdef ${DESTDIR}/etc/mtree/BSD.x11.dist -p ${TEMPROOT} -U >/dev/null

	# @sample directories are processed first
	exec_espie | sort -u | while read _l; do
		set -A _sample -- ${_l}
		_install_args="-o ${_sample[1]} -g ${_sample[2]} -m ${_sample[3]}"
		if [ "${_sample[0]}" = "0-DIR" ]; then
			install -d ${_install_args} ${_sample[4]} || return 1
		else
			# the directory we want to copy the @sample file
			# into does not exist, ignore @sample and move on for now:
			# - if the directory was a @sample, it would have
			#   been created at this point
			# - we have no knowledge of the required owner,
			#   group and mode of the directory hierarchy
			#   because the dir is not a @sample (e.g. mail/femail,-chroot)
			# - it's not the job of sysmerge to repair
			#   broken pkg installations
			#_pkghier=$(dirname ${_sample[5]})
			#if [ ! -d "${_pkghier}" ]; then
			#	install -d -o root -g wheel -m 0755 ${_pkghier}
			#fi
			if [ -d "${DESTDIR}/$(dirname ${_sample[5]})" ]; then
				install ${_install_args} ${_sample[4]} ${_sample[5]} || return 1
			fi
		fi
	done
	_ret=$?
	if [ "${_ret}" -eq 0 ]; then
		cd ${TEMPROOT} && find . -type f | sort | xargs sha256 -h ${WRKDIR}/${PKGSUM} || \
			_ret=1
	fi
	if [ "${_ret}" -ne 0 ]; then
		error_rm_wrkdir "failed to populate packages @samples and create checksum file"
	fi
}

sm_populate() {
	local cf i _array _d _r _D _R CF_DIFF CF_FILES CURSUM IGNORE_FILES
	echo "===> Populating temporary root under ${TEMPROOT}"
	mkdir -p ${TEMPROOT}

	if [ ! -d ${DESTDIR}/${DBDIR} ]; then
		mkdir -p ${DESTDIR}/${DBDIR} || exit 1
	fi

	# automatically install missing user(s) and group(s) from the
	# new master.passwd and group files after extracting the sets
	# (so we have the new files)
	extract_sets
	copy_pkg_samples
	install_user_group

	# EGSUM is used differently, see sm_check_an_eg()
	for i in ${ETCSUM} ${XETCSUM} ${PKGSUM}; do
		if [ -f ${DESTDIR}/${DBDIR}/${i} ]; then
			# delete file in temproot if it has not changed since last release
			# and is present in current installation
			if [ -z "${DIFFMODE}" ]; then
				# 2>/dev/null: if file got removed manually but is still in the sum file
				_R=$(cd ${TEMPROOT} && \
					sha256 -c ${DESTDIR}/${DBDIR}/${i} 2>/dev/null | awk '/OK/ { print $2 }' | sed 's/[:]//')
				for _r in ${_R}; do
					if [ -f ${DESTDIR}/${_r} -a -f ${TEMPROOT}/${_r} ]; then
						rm -f ${TEMPROOT}/${_r}
					fi
				done
			fi

			# set auto-upgradable files
			_D=$(diff -u ${WRKDIR}/${i} ${DESTDIR}/${DBDIR}/${i} | sed -n 's/^+SHA256 (\(.*\)).*/\1/p')
			for _d in ${_D}; do
				# 2>/dev/null: if file got removed manually but is still in the sum file
				CURSUM=$(cd ${DESTDIR:=/} && sha256 ${_d} 2>/dev/null)
				[ -n "$(grep "${CURSUM}" ${DESTDIR}/${DBDIR}/${i})" -a -z "$(grep "${CURSUM}" ${WRKDIR}/${i})" ] && \
					_array="${_array} ${_d}"
			done
			[[ -n ${_array} ]] && set -A AUTO_UPG -- ${_array}

			mv ${DESTDIR}/${DBDIR}/${i} ${DESTDIR}/${DBDIR}/.${i}.bak
		fi
		mv ${WRKDIR}/${i} ${DESTDIR}/${DBDIR}/${i}
	done

	# files we don't want/need to deal with
	IGNORE_FILES="/etc/*.db
		      /etc/group
		      /etc/localtime
		      /etc/mail/*.db
		      /etc/master.passwd
		      /etc/passwd
		      /etc/motd
		      /etc/myname
		      /usr/share/sysmerge/{etc,examples,xetc}sum
		      /var/db/locate.database
		      /var/mail/root"
	CF_FILES="/etc/mail/localhost.cf /etc/mail/sendmail.cf /etc/mail/submit.cf"
	for cf in ${CF_FILES}; do
		CF_DIFF=$(diff -q -I "##### " ${TEMPROOT}/${cf} ${DESTDIR}/${cf} 2>/dev/null)
		[[ -z ${CF_DIFF} ]] && IGNORE_FILES="${IGNORE_FILES} ${cf}"
	done
	if [ -f /etc/sysmerge.ignore ]; then
		IGNORE_FILES="${IGNORE_FILES} $(stripcom /etc/sysmerge.ignore)"
	fi
	for i in ${IGNORE_FILES}; do
		rm -f ${TEMPROOT}/${i}
	done
}

install_and_rm() {
	if [ -f "${5}/${4##*/}" ]; then
		mkdir -p ${BKPDIR}/${4%/*}
		cp ${5}/${4##*/} ${BKPDIR}/${4%/*}
	fi

	if ! install -m "${1}" -o "${2}" -g "${3}" "${4}" "${5}"; then
		rm -f ${BKPDIR}/${4%/*}/${4##*/}
		return 1
	fi
	rm -f "${4}"
}

install_file() {
	local DIR_MODE FILE_MODE FILE_OWN FILE_GRP INSTDIR
	INSTDIR=${1#.}
	INSTDIR=${INSTDIR%/*}

	[[ -z ${INSTDIR} ]] && INSTDIR=/

	DIR_MODE=$(stat -f "%OMp%OLp" "${TEMPROOT}/${INSTDIR}")
	eval $(stat -f "FILE_MODE=%OMp%OLp FILE_OWN=%Su FILE_GRP=%Sg" ${1})

	[ -n "${DESTDIR}${INSTDIR}" -a ! -d "${DESTDIR}${INSTDIR}" ] && \
		install -d -o root -g wheel -m "${DIR_MODE}" "${DESTDIR}${INSTDIR}"

	install_and_rm "${FILE_MODE}" "${FILE_OWN}" "${FILE_GRP}" "${1}" "${DESTDIR}${INSTDIR}" || return

	case "${1#.}" in
	/dev/MAKEDEV)
		echo " (running MAKEDEV(8))"
		(cd ${DESTDIR}/dev && /bin/sh MAKEDEV all)
		export NEED_REBOOT=1
		;;
	/etc/login.conf)
		if [ -f ${DESTDIR}/etc/login.conf.db ]; then
			echo " (running cap_mkdb(1))"
			cap_mkdb ${DESTDIR}/etc/login.conf
		else
			echo ""
		fi
		export NEED_REBOOT=1
		;;
	/etc/mail/@(access|genericstable|mailertable|virtusertable))
		echo " (running makemap(8))"
		/usr/libexec/sendmail/makemap hash ${DESTDIR}/${1#.} < ${DESTDIR}/${1#.}
		;;
	/etc/mail/aliases)
		echo " (running newaliases(8))"
		${DESTDIR:+chroot ${DESTDIR}} newaliases >/dev/null || export NEED_NEWALIASES=1
		;;
	*)
		echo ""
		;;
	esac
}

install_link() {
	local _LINKF _LINKT DIR_MODE 
	_LINKT=$(readlink ${COMPFILE})
	_LINKF=$(dirname ${DESTDIR}${COMPFILE#.})

	DIR_MODE=$(stat -f "%OMp%OLp" "${TEMPROOT}/${_LINKF}")
	[[ ! -d ${_LINKF} ]] && \
		install -d -o root -g wheel -m "${DIR_MODE}" "${_LINKF}"

	rm -f ${COMPFILE}
	(cd ${_LINKF} && ln -sf ${_LINKT} .)
	return
}

install_user_group() {
	local _g _gid _u
	local _pw="${TEMPROOT}/etc/master.passwd"
	local _gr="${TEMPROOT}/etc/group"

	# when running with '-x' only or in PKGMODE
	[ ! -f ${_pw} -o ! -f ${_gr} ] && return

	while read l; do
		_u=$(echo ${l} | awk -F ':' '{ print $1 }')
		if [ "${_u}" != "root" ]; then
			if [ -z "$(grep -E "^${_u}:" ${DESTDIR}/etc/master.passwd)" ]; then
				echo "===> Adding the ${_u} user"
				if ${DESTDIR:+chroot ${DESTDIR}} chpass -la "${l}"; then
					set -A NEWUSR -- ${NEWUSR[@]} ${_u}
				fi
			fi
		fi
	done < ${_pw}

	while read l; do
		_g=$(echo ${l} | awk -F ':' '{ print $1 }')
		_gid=$(echo ${l} | awk -F ':' '{ print $3 }')
		if [ -z "$(grep -E "^${_g}:" ${DESTDIR}/etc/group)" ]; then
			echo "===> Adding the ${_g} group"
			if ${DESTDIR:+chroot ${DESTDIR}} groupadd -g "${_gid}" "${_g}"; then
				set -A NEWGRP -- ${NEWGRP[@]} ${_g}
			fi
		fi
	done < ${_gr}
}

merge_loop() {
	local INSTALL_MERGED MERGE_AGAIN
	[[ ${MERGE_CMD} == sdiff* ]] && \
		echo "===> Type h at the sdiff prompt (%) to get usage help\n"
	MERGE_AGAIN=1
	while [ -n "${MERGE_AGAIN}" ]; do
		cp -p "${COMPFILE}" "${COMPFILE}.merged"
		${MERGE_CMD} "${COMPFILE}.merged" \
			"${DESTDIR}${COMPFILE#.}" "${COMPFILE}"
		INSTALL_MERGED=v
		while [ "${INSTALL_MERGED}" = "v" ]; do
			echo ""
			echo "  Use 'e' to edit the merged file"
			echo "  Use 'i' to install the merged file"
			echo "  Use 'n' to view a diff between the merged and new files"
			echo "  Use 'o' to view a diff between the old and merged files"
			echo "  Use 'r' to re-do the merge"
			echo "  Use 'v' to view the merged file"
			echo "  Use 'x' to delete the merged file and go back to previous menu"
			echo "  Default is to leave the temporary file to deal with by hand"
			echo ""
			echo -n "===> How should I deal with the merged file? [Leave it for later] "
			read INSTALL_MERGED
			case "${INSTALL_MERGED}" in
			[eE])
				echo "editing merged file...\n"
				${EDIT} ${COMPFILE}.merged
				INSTALL_MERGED=v
				;;
			[iI])
				mv "${COMPFILE}.merged" "${COMPFILE}"
				echo -n "\n===> Merging ${COMPFILE#.}"
				install_file "${COMPFILE}" || \
					warn "problem merging ${COMPFILE#.}"
				unset MERGE_AGAIN
				;;
			[nN])
				(
					echo "comparison between merged and new files:\n"
					diff -u ${COMPFILE}.merged ${COMPFILE}
				) | ${PAGER}
				INSTALL_MERGED=v
				;;
			[oO])
				(
					echo "comparison between old and merged files:\n"
					diff -u ${DESTDIR}${COMPFILE#.} ${COMPFILE}.merged
				) | ${PAGER}
				INSTALL_MERGED=v
				;;
			[rR])
				rm "${COMPFILE}.merged"
				;;
			[vV])
				${PAGER} "${COMPFILE}.merged"
				;;
			[xX])
				rm "${COMPFILE}.merged"
				return 1
				;;
			'')
				echo "===> ${COMPFILE} will remain for your consideration"
				unset MERGE_AGAIN
				;;
			*)
				echo "invalid choice: ${INSTALL_MERGED}"
				INSTALL_MERGED=v
				;;
			esac
		done
	done
}

diff_loop() {
	local i CAN_INSTALL HANDLE_COMPFILE NO_INSTALLED
	if [ -n "${BATCHMODE}" ]; then
		HANDLE_COMPFILE=todo
	else
		HANDLE_COMPFILE=v
	fi

	unset NO_INSTALLED CAN_INSTALL FORCE_UPG

	while [[ ${HANDLE_COMPFILE} == @(v|todo) ]]; do
		if [ -f "${DESTDIR}${COMPFILE#.}" -a -f "${COMPFILE}" -a -z "${IS_LINK}" ]; then
			if [ -z "${DIFFMODE}" ]; then
				# automatically install files if current != new and current = old
				for i in "${AUTO_UPG[@]}"; do
					[[ ${i} == ${COMPFILE} ]] && FORCE_UPG=1
				done
				# automatically install files which differ only by CVS Id or that are binaries
				if [ -z "$(diff -q -I'[$]OpenBSD:.*$' "${DESTDIR}${COMPFILE#.}" "${COMPFILE}")" -o -n "${FORCE_UPG}" -o -n "${IS_BINFILE}" ]; then
					echo -n "===> Updating ${COMPFILE#.}"
					if install_file "${COMPFILE}"; then
						AUTO_INSTALLED_FILES="${AUTO_INSTALLED_FILES}${DESTDIR}${COMPFILE#.}\n"
					else
						warn "problem updating ${COMPFILE#.}"
					fi
					return
				fi
			fi
			if [ "${HANDLE_COMPFILE}" = "v" ]; then
				(
					echo "\n========================================================================\n"
					echo "===> Displaying differences between ${COMPFILE} and installed version:"
					echo ""
					diff -u "${DESTDIR}${COMPFILE#.}" "${COMPFILE}"
				) | ${PAGER}
				echo ""
			fi
		else
			# file does not exist on the target system
			if [ -n "${IS_LINK}" ]; then
				if [ -n "${DIFFMODE}" ]; then
					echo ""
					NO_INSTALLED=1
				else
					if install_link; then
						echo "===> ${COMPFILE#.} link created successfully"
						AUTO_INSTALLED_FILES="${AUTO_INSTALLED_FILES}${DESTDIR}${COMPFILE#.}\n"
					else
						warn "problem creating ${COMPFILE#.} link"
					fi
					return
				fi
			fi
			if [ -n "${DIFFMODE}" ]; then
				echo ""
				NO_INSTALLED=1
			else
				echo -n "===> Installing ${COMPFILE#.}"
				if install_file "${COMPFILE}"; then
					AUTO_INSTALLED_FILES="${AUTO_INSTALLED_FILES}${DESTDIR}${COMPFILE#.}\n"
				else
					warn "problem installing ${COMPFILE#.}"
				fi
				return
			fi
		fi

		if [ -z "${BATCHMODE}" ]; then
			echo "  Use 'd' to delete the temporary ${COMPFILE}"
			if [ "${COMPFILE}" != ./etc/hosts ]; then
				CAN_INSTALL=1
				echo "  Use 'i' to install the temporary ${COMPFILE}"
			fi
			if [ -z "${NO_INSTALLED}" -a -z "${IS_BINFILE}" -a -z "${IS_LINK}" ]; then
				echo "  Use 'm' to merge the temporary and installed versions"
				echo "  Use 'v' to view the diff results again"
			fi
			echo ""
			echo "  Default is to leave the temporary file to deal with by hand"
			echo ""
			echo -n "How should I deal with this? [Leave it for later] "
			read HANDLE_COMPFILE
		else
			unset HANDLE_COMPFILE
		fi

		case "${HANDLE_COMPFILE}" in
		[dD])
			rm "${COMPFILE}"
			echo "\n===> Deleting ${COMPFILE}"
			;;
		[iI])
			if [ -n "${CAN_INSTALL}" ]; then
				echo ""
				if [ -n "${IS_LINK}" ]; then
					if install_link; then
						echo "===> ${COMPFILE#.} link created successfully"
						MERGED_FILES="${MERGED_FILES}${DESTDIR}${COMPFILE#.}\n"
					else
						warn "problem creating ${COMPFILE#.} link"
					fi
				else
					echo -n "===> Updating ${COMPFILE#.}"
					if install_file "${COMPFILE}"; then
						MERGED_FILES="${MERGED_FILES}${DESTDIR}${COMPFILE#.}\n"
					else
						warn "problem updating ${COMPFILE#.}"
					fi
				fi
			else
				echo "invalid choice: ${HANDLE_COMPFILE}\n"
				HANDLE_COMPFILE="todo"
			fi
				
			;;
		[mM])
			if [ -z "${NO_INSTALLED}" -a -z "${IS_BINFILE}" -a -z "${IS_LINK}" ]; then
				merge_loop && \
					MERGED_FILES="${MERGED_FILES}${DESTDIR}${COMPFILE#.}\n" || \
					HANDLE_COMPFILE="todo"
			else
				echo "invalid choice: ${HANDLE_COMPFILE}\n"
				HANDLE_COMPFILE="todo"
			fi
			;;
		[vV])
			if [ -z "${NO_INSTALLED}" -a -z "${IS_BINFILE}" -a -z "${IS_LINK}" ]; then
				HANDLE_COMPFILE="v"
			else
				echo "invalid choice: ${HANDLE_COMPFILE}\n"
				HANDLE_COMPFILE="todo"
			fi
			;;
		'')
			echo "===> ${COMPFILE} will remain for your consideration"
			;;
		*)
			echo "invalid choice: ${HANDLE_COMPFILE}\n"
			HANDLE_COMPFILE="todo"
			continue
			;;
		esac
	done
}

sm_compare() {
	local _c1 _c2 COMPFILE CVSID1 CVSID2
	echo "===> Starting comparison"

	cd ${TEMPROOT} || error_rm_wrkdir "cannot enter ${TEMPROOT}"

	# aliases(5) needs to be handled last in case smtpd.conf(5) syntax changes
	_c1=$(find . -type f -or -type l | grep -vE '^./etc/mail/aliases$')
	_c2=$(find . -type f -name aliases)
	for COMPFILE in ${_c1} ${_c2}; do
		unset IS_BINFILE IS_LINK
		# treat empty files the same as IS_BINFILE to avoid comparing them;
		# only process them (i.e. install) if they don't exist on the target system
		if [ ! -s "${COMPFILE}" ]; then
			if [ -f "${DESTDIR}${COMPFILE#.}" ]; then
				[[ -f ${COMPFILE} ]] && rm ${COMPFILE}
			else
				IS_BINFILE=1
			fi
		fi

		# links need to be treated in a different way
		[[ -h ${COMPFILE} ]] && IS_LINK=1
		if [ -n "${IS_LINK}" -a -h "${DESTDIR}${COMPFILE#.}" ]; then
			IS_LINK=1
			# if links target are the same, remove from temproot
			if [ "$(readlink ${COMPFILE})" = "$(readlink ${DESTDIR}${COMPFILE#.})" ]; then
				rm "${COMPFILE}"
			else
				diff_loop
			fi
			continue
		fi

		# file not present on the system
		if [ ! -e "${DESTDIR}${COMPFILE#.}" ]; then
			diff_loop
			continue
		fi

		# compare CVS $Id's first so if the file hasn't been modified,
		# it will be deleted from temproot and ignored from comparison.
		# several files are generated from scripts so CVS ID is not a
		# reliable way of detecting changes; leave for a full diff.
		if [[ -z ${DIFFMODE} && \
			-z ${PKGMODE} && \
			${COMPFILE} != ./etc/@(fbtab|ttys) && \
			-z ${IS_LINK} ]]; then
			CVSID1=$(sed -n "/[$]OpenBSD:.*Exp [$]/{p;q;}" ${DESTDIR}${COMPFILE#.} 2>/dev/null)
			CVSID2=$(sed -n "/[$]OpenBSD:.*Exp [$]/{p;q;}" ${COMPFILE} 2>/dev/null) || CVSID2=none
			[[ ${CVSID2} == ${CVSID1} ]] && rm "${COMPFILE}"
		fi

		if [ -f "${COMPFILE}" -a -z "${IS_LINK}" ]; then
			# make sure files are different; if not, delete the one in temproot
			if diff -q "${DESTDIR}${COMPFILE#.}" "${COMPFILE}" >/dev/null; then
				rm "${COMPFILE}"
			# xetcXX.tgz contains binary files; set IS_BINFILE to disable sdiff
			elif diff -q "${DESTDIR}${COMPFILE#.}" "${COMPFILE}" | grep -q Binary; then
				IS_BINFILE=1
				diff_loop
			else
				diff_loop
			fi
		fi
	done
}

sm_check_an_eg() {
	EGSUM=examplessum
	local _egmods _i _managed
	if [ -f "${DESTDIR}/${DBDIR}/${EGSUM}" ]; then
		EGMODS="$(cd ${DESTDIR:=/} && sha256 -c ${DESTDIR}/${DBDIR}/${EGSUM} 2>/dev/null | grep 'FAILED$' | awk '{ print $2 }' | sed -e "s,:,,")"
		mv ${DESTDIR}/${DBDIR}/${EGSUM} ${DESTDIR}/${DBDIR}/.${EGSUM}.bak
	fi
	for _i in ${EGMODS}; do
		_managed=$(echo ${_i} | sed -e "s,etc/examples,etc,")
		if [ -f "${DESTDIR}/${_managed}" ]; then
			_egmods="${_egmods} ${_managed##*/}"
		fi
	done
	if [ -n "${_egmods}" ]; then
		warn "example(s) changed for:${_egmods}"
	else
		# example changed but we have no corresponding file under /etc
		unset EGMODS
	fi
	cd ${DESTDIR:=/} && \
		find ./etc/examples -type f | xargs sha256 -h ${DESTDIR}/${DBDIR}/${EGSUM} || \
		error_rm_wrkdir "failed to create ${EGSUM} checksum file"
	if [ -f "${DESTDIR}/${DBDIR}/.${EGSUM}.bak" ]; then
		rm ${DESTDIR}/${DBDIR}/.${EGSUM}.bak
	fi
}

sm_post() {
	local FILES_IN_TEMPROOT FILES_IN_BKPDIR

	FILES_IN_TEMPROOT=$(find ${TEMPROOT} -type f ! -name \*.merged -size +0)
	[[ -d ${BKPDIR} ]] && FILES_IN_BKPDIR=$(find ${BKPDIR} -type f -size +0)

	if [ -n "${NEED_NEWALIASES}" ]; then
		report "===> A new ${DESTDIR}/etc/mail/aliases file was installed."
		report "However ${DESTDIR}/usr/bin/newaliases could not be run,"
		report "you will need to rebuild your aliases database manually.\n"
	fi

	if [ -n "${AUTO_INSTALLED_FILES}" ]; then
		report "===> Automatically installed file(s)"
		report "${AUTO_INSTALLED_FILES}"
	fi
	if [ -n "${MERGED_FILES}" ]; then
		report "===> Manually merged/installed file(s)"
		report "${MERGED_FILES}"
	fi
	if [ -n "${FILES_IN_BKPDIR}" ]; then
		report "===> Backup of replaced file(s) can be found under"
		report "${BKPDIR}\n"
	fi
	if [ -n "${NEWUSR}" -o -n "${NEWGRP}" ]; then
		report "===> The following user(s)/group(s) have been added"
		[[ -n ${NEWUSR} ]] && report "user(s): ${NEWUSR[@]}"
		[[ -n ${NEWGRP} ]] && report "group(s): ${NEWGRP[@]}"
		report ""
	fi
	if [ -n "${EGMODS}" ]; then
		report "===> Matching example(s) modified since last run"
		report "${EGMODS}"
		report ""
	fi
	if [ -n "${FILES_IN_TEMPROOT}" ]; then
		report "===> File(s) remaining for you to merge by hand"
		report "${FILES_IN_TEMPROOT}"
	fi

	[[ -n ${FILES_IN_TEMPROOT} ]] && \
		warn "some files are still left for comparison"

	[[ -n ${NEED_NEWALIASES} ]] && \
		warn "newaliases(8) failed to run properly"

	[[ -n ${NEED_REBOOT} ]] && \
		warn "some new/updated file(s) may require a reboot"

	echo "===> Checking directory hierarchy permissions (running mtree(8))"
	mtree -qdef ${DESTDIR}/etc/mtree/4.4BSD.dist -p ${DESTDIR:=/} -U >/dev/null
	[[ -n ${XTGZ} ]] && \
		mtree -qdef ${DESTDIR}/etc/mtree/BSD.x11.dist -p ${DESTDIR:=/} -U >/dev/null

	if [ -e "${REPORT}" ]; then
		echo "===> Output log available at ${REPORT}"
		find ${TEMPROOT} -type f -empty | xargs -r rm
		find ${TEMPROOT} -type d | sort -r | xargs -r rmdir 2>/dev/null
	else
		echo "===> Removing ${WRKDIR}"
		rm -rf "${WRKDIR}"
	fi

	unset NEED_NEWALIASES NEED_REBOOT

	rm -f ${DESTDIR}/${DBDIR}/.*.bak
}


while getopts bdpSs:x: arg; do
	case ${arg} in
	b)
		BATCHMODE=1
		;;
	d)
		DIFFMODE=1
		;;
	p)
		PKGMODE=1
		;;
	S)	
		NOSIGCHECK=1
		;;
	x)
		XTGZ="${OPTARG}"
		;;
	*)
		usage
		error_rm_wrkdir
		;;
	esac
done

shift $(( OPTIND -1 ))
if (($# != 0)); then
	usage
	error_rm_wrkdir
fi

TGZ=/usr/share/sysmerge/etc.tgz

if [ -z "${XTGZ}" -a -n "${SM_PATH}" -a -d ${DESTDIR}/etc/X11 ]; then
	XTGZ="${SM_PATH}/xetc${RELINT}.tgz"
fi

if [ -n "${PKGMODE}" ]; then
	unset TGZ
	if [ -n "${XTGZ}" ]; then
		usage
		error_rm_wrkdir "conflicting options"
	fi
fi

TEMPROOT="${WRKDIR}/temproot"
BKPDIR="${WRKDIR}/backups"

sm_fetch_and_verify
sm_populate
sm_compare
sm_check_an_eg
sm_post
