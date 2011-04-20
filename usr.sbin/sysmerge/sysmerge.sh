#!/bin/ksh -
#
# $OpenBSD: sysmerge.sh,v 1.68 2011/04/20 09:37:35 ajacoutot Exp $
#
# Copyright (c) 1998-2003 Douglas Barton <DougB@FreeBSD.org>
# Copyright (c) 2008, 2009, 2010 Antoine Jacoutot <ajacoutot@openbsd.org>
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

unset AUTO_INSTALLED_FILES BATCHMODE DIFFMODE ETCSUM NEED_NEWALIASES
unset NEWGRP NEWUSR NEED_REBOOT OBSOLETE_FILES SRCDIR SRCSUM TGZ TGZURL
unset XETCSUM XTGZ XTGZURL

WRKDIR=`mktemp -d -p ${TMPDIR:=/var/tmp} sysmerge.XXXXX` || exit 1
SWIDTH=`stty size | awk '{w=$2} END {if (w==0) {w=80} print w}'`
MERGE_CMD="${MERGE_CMD:=sdiff -as -w ${SWIDTH} -o}"
REPORT="${REPORT:=${WRKDIR}/sysmerge.log}"
DBDIR="${DBDIR:=/var/db/sysmerge}"

PAGER="${PAGER:=/usr/bin/more}"

# clean leftovers created by make in src
clean_src() {
	if [ "${SRCDIR}" ]; then
		cd ${SRCDIR}/gnu/usr.sbin/sendmail/cf/cf && make cleandir > /dev/null
	fi
}

# restore files from backups or remove the newly generated sum files if
# they did not exist
restore_bak() {
	for i in ${DESTDIR}/${DBDIR}/.{${SRCSUM},${ETCSUM},${XETCSUM}}.bak; do
		_i=`basename ${i} .bak`
		if [ -f "${i}" ]; then
			mv ${i} ${DESTDIR}/${DBDIR}/${_i#.}
		elif [ -f "${DESTDIR}/${DBDIR}/${_i#.}" ]; then
			rm ${DESTDIR}/${DBDIR}/${_i#.}
		fi
	done
}

# remove newly created work directory and exit with status 1
error_rm_wrkdir() {
	rmdir ${WRKDIR} 2> /dev/null
	exit 1
}

usage() {
	echo "usage: ${0##*/} [-bd] [-s [src | etcXX.tgz]] [-x xetcXX.tgz]" >&2
}

trap "restore_bak; clean_src; rm -rf ${WRKDIR}; exit 1" 1 2 3 13 15

if [ "`id -u`" -ne 0 ]; then
	echo "\t*** ERROR: need root privileges to run this script"
	usage
	error_rm_wrkdir
fi

if [ -z "${FETCH_CMD}" ]; then
	if [ -z "${FTP_KEEPALIVE}" ]; then
		FTP_KEEPALIVE=0
	fi
	FETCH_CMD="/usr/bin/ftp -V -m -k ${FTP_KEEPALIVE}"
fi

do_populate() {
	mkdir -p ${DESTDIR}/${DBDIR} || error_rm_wrkdir
	echo "===> Populating temporary root under ${TEMPROOT}"
	mkdir -p ${TEMPROOT}
	if [ "${SRCDIR}" ]; then
		SRCSUM=srcsum
		cd ${SRCDIR}/etc
		make DESTDIR=${TEMPROOT} distribution-etc-root-var > /dev/null 2>&1
		(cd ${TEMPROOT} && find . -type f | xargs cksum > ${WRKDIR}/${SRCSUM})
	fi

	if [ "${TGZ}" -o "${XTGZ}" ]; then
		for i in ${TGZ} ${XTGZ}; do
			tar -xzphf ${i} -C ${TEMPROOT};
		done
		if [ "${TGZ}" ]; then
			ETCSUM=etcsum
			_E=$(cd `dirname ${TGZ}` && pwd)/`basename ${TGZ}`
			(cd ${TEMPROOT} && tar -tzf ${_E} | xargs cksum > ${WRKDIR}/${ETCSUM})
		fi
		if [ "${XTGZ}" ]; then
			XETCSUM=xetcsum
			_X=$(cd `dirname ${XTGZ}` && pwd)/`basename ${XTGZ}`
			(cd ${TEMPROOT} && tar -tzf ${_X} | xargs cksum > ${WRKDIR}/${XETCSUM})
		fi
	fi

	for i in ${SRCSUM} ${ETCSUM} ${XETCSUM}; do
		if [ -f ${DESTDIR}/${DBDIR}/${i} ]; then
			# delete file in temproot if it has not changed since last release
			# and is present in current installation
			if [ -z "${DIFFMODE}" ]; then
				_R=$(cd ${TEMPROOT} && cksum -c ${DESTDIR}/${DBDIR}/${i} 2> /dev/null | grep OK | awk '{ print $2 }' | sed 's/[:]//')
				for _r in ${_R}; do
					if [ -f ${DESTDIR}/${_r} -a -f ${TEMPROOT}/${_r} ]; then
						rm -f ${TEMPROOT}/${_r}
					fi
				done
			fi

			# set auto-upgradable files
			_D=`diff -u ${WRKDIR}/${i} ${DESTDIR}/${DBDIR}/${i} | grep -E '^\+' | sed '1d' | awk '{print $3}'`
			for _d in ${_D}; do
				CURSUM=$(cd ${DESTDIR:=/} && cksum ${_d} 2> /dev/null)
				if [ -n "`grep "${CURSUM}" ${DESTDIR}/${DBDIR}/${i}`" -a -z "`grep "${CURSUM}" ${WRKDIR}/${i}`" ]; then
					local _array="${_array} ${_d}"
				fi
			done
			if [ -n "${_array}" ]; then
				set -A AUTO_UPG -- ${_array}
			fi

			# check for obsolete files
			awk '{ print $3 }' ${DESTDIR}/${DBDIR}/${i} > ${WRKDIR}/new
			awk '{ print $3 }' ${WRKDIR}/${i} > ${WRKDIR}/old
			if [ -n "`diff -q ${WRKDIR}/old ${WRKDIR}/new`" ]; then
				local _obs="${_obs} `diff -C 0 ${WRKDIR}/new ${WRKDIR}/old | sed -n -e 's,^- .,,gp'`"
				set -A OBSOLETE_FILES -- ${_obs}
			fi
			rm ${WRKDIR}/new ${WRKDIR}/old
			
			mv ${DESTDIR}/${DBDIR}/${i} ${DESTDIR}/${DBDIR}/.${i}.bak
		fi
		mv ${WRKDIR}/${i} ${DESTDIR}/${DBDIR}/${i}
	done

	# files we don't want/need to deal with
	IGNORE_FILES="/etc/*.db
		      /etc/mail/*.db
		      /etc/passwd
		      /etc/motd
		      /etc/myname
		      /var/db/locate.database
		      /var/db/sysmerge/{etc,xetc}sum
		      /var/games/tetris.scores
		      /var/mail/root"
	CF_FILES="/etc/mail/localhost.cf /etc/mail/sendmail.cf /etc/mail/submit.cf"
	for cf in ${CF_FILES}; do
		CF_DIFF=`diff -q -I "##### " ${TEMPROOT}/${cf} ${DESTDIR}/${cf} 2> /dev/null`
		if [ -z "${CF_DIFF}" ]; then
			IGNORE_FILES="${IGNORE_FILES} ${cf}"
		fi
	done
	if [ -r /etc/sysmerge.ignore ]; then
		while read i; do \
			IGNORE_FILES="${IGNORE_FILES} $(echo ${i} | sed -e 's,\.\.,,g' -e 's,#.*,,g')"
		done < /etc/sysmerge.ignore
	fi
	for i in ${IGNORE_FILES}; do
		rm -rf ${TEMPROOT}/${i};
	done
}

do_install_and_rm() {
	if [ -f "${5}/${4##*/}" ]; then
		mkdir -p ${BKPDIR}/${4%/*}
		cp ${5}/${4##*/} ${BKPDIR}/${4%/*}
	fi

	if ! install -m "${1}" -o "${2}" -g "${3}" "${4}" "${5}" 2> /dev/null; then
		rm -f ${BKPDIR}/${4%/*}/${4##*/}
		return 1
	fi
	rm -f "${4}"
}

mm_install() {
	local INSTDIR
	unset RUNNING
	INSTDIR=${1#.}
	INSTDIR=${INSTDIR%/*}

	if [ -z "${INSTDIR}" ]; then INSTDIR=/; fi

	DIR_MODE=`stat -f "%OMp%OLp" "${TEMPROOT}/${INSTDIR}"`
	eval `stat -f "FILE_MODE=%OMp%OLp FILE_OWN=%Su FILE_GRP=%Sg" ${1}`

	if [ "${DESTDIR}${INSTDIR}" -a ! -d "${DESTDIR}${INSTDIR}" ]; then
		install -d -o root -g wheel -m "${DIR_MODE}" "${DESTDIR}${INSTDIR}"
	fi

	do_install_and_rm "${FILE_MODE}" "${FILE_OWN}" "${FILE_GRP}" "${1}" "${DESTDIR}${INSTDIR}" || return

	case "${1#.}" in
	/dev/MAKEDEV)
		RUNNING=" (running MAKEDEV(8))"
		(cd ${DESTDIR}/dev && /bin/sh MAKEDEV all)
		export NEED_REBOOT=1
		;;
	/etc/login.conf)
		if [ -f ${DESTDIR}/etc/login.conf.db ]; then
			RUNNING=" (running cap_mkdb(1))"
			cap_mkdb ${DESTDIR}/etc/login.conf
		fi
		export NEED_REBOOT=1
		;;
	/etc/mail/access|/etc/mail/genericstable|/etc/mail/mailertable|/etc/mail/virtusertable)
		RUNNING=" (running makemap(8))"
		DBFILE=`echo ${1} | sed -e 's,.*/,,'`
		/usr/libexec/sendmail/makemap hash ${DESTDIR}/${1#.} < ${DESTDIR}/${1#.}
		;;
	/etc/mail/aliases)
		RUNNING=" (running newaliases(8))"
		if [ "${DESTDIR}" ]; then
			chroot ${DESTDIR} newaliases >/dev/null || export NEED_NEWALIASES=1
		else
			newaliases >/dev/null
		fi
		;;
	/etc/master.passwd)
		RUNNING=" (running pwd_mkdb(8))"
		pwd_mkdb -d ${DESTDIR}/etc -p ${DESTDIR}/etc/master.passwd
		;;
	esac
}

mm_install_link() {
	_LINKT=`readlink ${COMPFILE}`
	_LINKF=`dirname ${DESTDIR}${COMPFILE#.}`
	rm -f ${COMPFILE}
	(cd ${_LINKF} && ln -sf ${_LINKT} .)
	return
}

merge_loop() {
	if [ "`expr "${MERGE_CMD}" : ^sdiff.*`" -gt 0 ]; then
		echo "===> Type h at the sdiff prompt (%) to get usage help\n"
	fi
	MERGE_AGAIN=1
	while [ "${MERGE_AGAIN}" ]; do
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
				if [ -z "${VISUAL}" ]; then
					EDIT="${EDITOR:=/usr/bin/vi}"
				else
					EDIT="${VISUAL}"
				fi
				if which ${EDIT} > /dev/null 2>&1; then
					${EDIT} ${COMPFILE}.merged
				else
					echo "\t*** ERROR: ${EDIT} can not be found or is not executable"
				fi
				INSTALL_MERGED=v
				;;
			[iI])
				mv "${COMPFILE}.merged" "${COMPFILE}"
				echo "\n===> Installing merged version of ${COMPFILE#.}${RUNNING}"
				if ! mm_install "${COMPFILE}"; then
					echo "\t*** WARNING: problem installing ${COMPFILE#.}, it will remain to merge by hand"
				fi
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
	if [ "${BATCHMODE}" ]; then
		HANDLE_COMPFILE=todo
	else
		HANDLE_COMPFILE=v
	fi

	unset NO_INSTALLED
	unset CAN_INSTALL
	unset FORCE_UPG

	while [ "${HANDLE_COMPFILE}" = "v" -o "${HANDLE_COMPFILE}" = "todo" ]; do
		if [ -f "${DESTDIR}${COMPFILE#.}" -a -f "${COMPFILE}" -a -z "${IS_LINK}" ]; then
			if [ -z "${DIFFMODE}" ]; then
				# automatically install files if current != new and current = old
				for i in "${AUTO_UPG[@]}"; do
					if [ "${i}" = "${COMPFILE}" ]; then
						FORCE_UPG=1
					fi
				done
				# automatically install files which differ only by CVS Id or that are binaries
				if [ -z "`diff -q -I'[$]OpenBSD:.*$' "${DESTDIR}${COMPFILE#.}" "${COMPFILE}"`" -o -n "${FORCE_UPG}" -o -n "${IS_BINFILE}" ]; then
					echo "===> Installing ${COMPFILE#.}${RUNNING}"
					if mm_install "${COMPFILE}"; then
						AUTO_INSTALLED_FILES="${AUTO_INSTALLED_FILES}${DESTDIR}${COMPFILE#.}\n"
					else
						echo "\t*** WARNING: problem installing ${COMPFILE#.}, it will remain to merge by hand"
					fi
					return
				fi
				# automatically install missing users
				if [ "${COMPFILE}" = "./etc/master.passwd" ]; then
					local _merge_pwd
					while read l; do
						_u=`echo ${l} | awk -F ':' '{ print $1 }'`
						if [ "${_u}" != "root" ]; then
							if [ -z "`grep -E "^${_u}:" ${DESTDIR}${COMPFILE#.}`" ]; then
								echo "===> Adding the ${_u} user"
								if [ "${DESTDIR}" ]; then
									chroot ${DESTDIR} chpass -la "${l}"
								else
									chpass -la "${l}"
								fi
								if [ $? -eq 0 ]; then
									set -A NEWUSR -- ${NEWUSR[@]} ${_u}
								else
									_merge_pwd=1
								fi
							fi
						fi
					done < ${COMPFILE}
					if [ -z ${_merge_pwd} ]; then
						rm "${TEMPROOT}${COMPFILE#.}"
						return
					fi
				fi
				# automatically install missing groups
				if [ "${COMPFILE}" = "./etc/group" ]; then
					local _merge_grp
					while read l; do
						_g=`echo ${l} | awk -F ':' '{ print $1 }'`
						_gid=`echo ${l} | awk -F ':' '{ print $3 }'`
						if [ -z "`grep -E "^${_g}:" ${DESTDIR}${COMPFILE#.}`" ]; then
							echo "===> Adding the ${_g} group"
							if [ "${DESTDIR}" ]; then
								chroot ${DESTDIR} groupadd -g "${_gid}" "${_g}"
							else
								groupadd -g "${_gid}" "${_g}"
							fi
							if [ $? -eq 0 ]; then
								set -A NEWGRP -- ${NEWGRP[@]} ${_g}
							else
								_merge_grp=1
							fi
						fi
					done < ${COMPFILE}
					if [ -z ${_merge_grp} ]; then
						rm "${TEMPROOT}${COMPFILE#.}"
						return
					fi
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
			if [ "${IS_LINK}" ]; then
				if [ -n "${DIFFMODE}" ]; then
					echo ""
					NO_INSTALLED=1
				else
					if mm_install_link; then
						echo "===> ${COMPFILE#.} link created successfully"
						AUTO_INSTALLED_FILES="${AUTO_INSTALLED_FILES}${DESTDIR}${COMPFILE#.}\n"
					else
						echo "\t*** WARNING: problem creating ${COMPFILE#.} link, manual intervention will be needed"
					fi
					return
				fi
			fi
			if [ -n "${DIFFMODE}" ]; then
				echo ""
				NO_INSTALLED=1
			else
				echo "===> Installing ${COMPFILE#.}${RUNNING}"
				if mm_install "${COMPFILE}"; then
					AUTO_INSTALLED_FILES="${AUTO_INSTALLED_FILES}${DESTDIR}${COMPFILE#.}\n"
				else
					echo "\t*** WARNING: problem installing ${COMPFILE#.}, it will remain to merge by hand"
				fi
				return
			fi
		fi

		if [ -z "${BATCHMODE}" ]; then
			echo "  Use 'd' to delete the temporary ${COMPFILE}"
			if [ "${COMPFILE}" != "./etc/master.passwd" -a "${COMPFILE}" != "./etc/group" -a "${COMPFILE}" != "./etc/hosts" ]; then
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
					if mm_install_link; then
						echo "===> ${COMPFILE#.} link created successfully"
						AUTO_INSTALLED_FILES="${AUTO_INSTALLED_FILES}${DESTDIR}${COMPFILE#.}\n"
					else
						echo "\t*** WARNING: problem creating ${COMPFILE#.} link, manual intervention will be needed"
					fi
				else
					echo "===> Installing ${COMPFILE#.}${RUNNING}"
					if ! mm_install "${COMPFILE}"; then
						echo "\t*** WARNING: problem installing ${COMPFILE#.}, it will remain to merge by hand"
					fi
				fi
			else
				echo "invalid choice: ${HANDLE_COMPFILE}\n"
				HANDLE_COMPFILE="todo"
			fi
				
			;;
		[mM])
			if [ -z "${NO_INSTALLED}" -a -z "${IS_BINFILE}" -a -z "${IS_LINK}" ]; then
				merge_loop || HANDLE_COMPFILE="todo"
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
			echo "\n===> ${COMPFILE} will remain for your consideration"
			;;
		*)
			echo "invalid choice: ${HANDLE_COMPFILE}\n"
			HANDLE_COMPFILE="todo"
			continue
			;;
		esac
	done
}

do_compare() {
	echo "===> Starting comparison"

	cd ${TEMPROOT} || error_rm_wrkdir

	# use -size +0 to avoid comparing empty log files and device nodes;
	# however, we want to keep the symlinks
	for COMPFILE in `find . -type f -size +0 -or -type l`; do
		unset IS_BINFILE
		unset IS_LINK
		# links need to be treated in a different way
		if [ -h "${COMPFILE}" ]; then
			IS_LINK=1
		fi
		if [ ! -e "${DESTDIR}${COMPFILE#.}" ]; then
			diff_loop
			continue
		fi

		# compare CVS $Id's first so if the file hasn't been modified,
		# it will be deleted from temproot and ignored from comparison.
		# several files are generated from scripts so CVS ID is not a
		# reliable way of detecting changes; leave for a full diff.
		if [ -z "${DIFFMODE}" -a "${COMPFILE}" != "./etc/fbtab" \
		    -a "${COMPFILE}" != "./etc/login.conf" \
		    -a "${COMPFILE}" != "./etc/sysctl.conf" \
		    -a "${COMPFILE}" != "./etc/ttys" -a -z "${IS_LINK}" ]; then
			CVSID1=`grep "[$]OpenBSD:" ${DESTDIR}${COMPFILE#.} 2> /dev/null`
			CVSID2=`grep "[$]OpenBSD:" ${COMPFILE} 2> /dev/null` || CVSID2=none
			if [ "${CVSID2}" = "${CVSID1}" ]; then rm "${COMPFILE}"; fi
		fi

		if [ -f "${COMPFILE}" -a -z "${IS_LINK}" ]; then
			# make sure files are different; if not, delete the one in temproot
			if diff -q "${DESTDIR}${COMPFILE#.}" "${COMPFILE}" > /dev/null 2>&1; then
				rm "${COMPFILE}"
			# xetcXX.tgz contains binary files; set IS_BINFILE to disable sdiff
			elif diff -q "${DESTDIR}${COMPFILE#.}" "${COMPFILE}" | grep "Binary" > /dev/null 2>&1; then
				IS_BINFILE=1
				diff_loop
			else
				diff_loop
			fi
		fi
	done

	echo "===> Comparison complete"
}

do_post() {
	echo "===> Checking directory hierarchy permissions (running mtree(8))"
	mtree -qdef ${DESTDIR}/etc/mtree/4.4BSD.dist -p ${DESTDIR:=/} -U > /dev/null
	if [ -n "${XTGZ}" ]; then
		mtree -qdef ${DESTDIR}/etc/mtree/BSD.x11.dist -p ${DESTDIR:=/} -U > /dev/null
	fi

	if [ "${NEED_NEWALIASES}" ]; then
		echo "===> A new ${DESTDIR}/etc/mail/aliases file was installed." >> ${REPORT}
		echo "However ${DESTDIR}/usr/bin/newaliases could not be run," >> ${REPORT}
		echo "you will need to rebuild your aliases database manually.\n" >> ${REPORT}
		unset NEED_NEWALIASES
	fi

	FILES_IN_TEMPROOT=`find ${TEMPROOT} -type f ! -name \*.merged -size +0 2> /dev/null`
	FILES_IN_BKPDIR=`find ${BKPDIR} -type f -size +0 2> /dev/null`
	if [ "${AUTO_INSTALLED_FILES}" ]; then
		echo "===> Automatically installed file(s)" >> ${REPORT}
		echo "${AUTO_INSTALLED_FILES}" >> ${REPORT}
	fi
	if [ "${FILES_IN_BKPDIR}" ]; then
		echo "===> Backup of replaced file(s) can be found under" >> ${REPORT}
		echo "${BKPDIR}\n" >> ${REPORT}
	fi
	if [ "${OBSOLETE_FILES}" ]; then
		echo "===> File(s) removed from previous source (maybe obsolete)" >> ${REPORT}
		echo "${OBSOLETE_FILES[@]}" | tr "[:space:]" "\n" >> ${REPORT}
		echo "" >> ${REPORT}
	fi
	if [ "${NEWUSR}" -o "${NEWGRP}" ]; then
		echo "===> The following user(s)/group(s) have been added" >> ${REPORT}
		if [ "${NEWUSR}" ]; then
			echo -n "user(s): ${NEWUSR[@]}\n" >> ${REPORT}
		fi
		if [ "${NEWGRP}" ]; then
			echo -n "group(s): ${NEWGRP[@]}\n" >> ${REPORT}
		fi
		echo "" >> ${REPORT}
	fi
	if [ "${FILES_IN_TEMPROOT}" ]; then
		echo "===> File(s) remaining for you to merge by hand" >> ${REPORT}
		echo "${FILES_IN_TEMPROOT}" >> ${REPORT}
	fi

	if [ -e "${REPORT}" ]; then
		echo "===> Output log available at ${REPORT}"
	else
		echo "===> Removing ${WRKDIR}"
		rm -rf "${WRKDIR}"
	fi

	if [ "${FILES_IN_TEMPROOT}" ]; then
		echo "\t*** WARNING: some files are still left for comparison"
	fi

	if [ "${OBSOLETE_FILES}" ]; then
		echo "\t*** WARNING: file(s) detected as obsolete: ${OBSOLETE_FILES[@]}"
	fi

	if [ "${NEED_NEWALIASES}" ]; then
		echo "\t*** WARNING: newaliases(8) failed to run properly"
	fi

	if [ "${NEED_REBOOT}" ]; then
		echo "\t*** WARNING: some new/updated file(s) may require a reboot"
	fi

	unset FILES_IN_TEMPROOT OBSOLETE_FILES NEED_NEWALIASES NEED_REBOOT

	clean_src
	rm -f ${DESTDIR}/${DBDIR}/.*.bak
}

while getopts bds:x: arg; do
	case ${arg} in
	b)
		BATCHMODE=1
		;;
	d)
		DIFFMODE=1
		;;
	s)
		if [ -f "${OPTARG}/etc/Makefile" ]; then
			SRCDIR=${OPTARG}
		elif [ -f "${OPTARG}" ] && \
			tar tzf ${OPTARG} ./var/db/sysmerge/etcsum > /dev/null 2>&1 ; then
			TGZ=${OPTARG}
		elif echo ${OPTARG} | \
		    grep -qE '^(http|ftp)://.*/etc[0-9][0-9]\.tgz$'; then
			TGZ=${WRKDIR}/etc.tgz
			TGZURL=${OPTARG}
			if ! ${FETCH_CMD} -o ${TGZ} ${TGZURL}; then
				echo "\t*** ERROR: could not retrieve ${TGZURL}"
				error_rm_wrkdir
			fi
		else
			echo "\t*** ERROR: ${OPTARG} is not a path to src nor etcXX.tgz"
			error_rm_wrkdir
		fi
		;;
	x)
		if [ -f "${OPTARG}" ] && \
			tar tzf ${OPTARG} ./var/db/sysmerge/xetcsum > /dev/null 2>&1 ; then \
			XTGZ=${OPTARG}
		elif echo ${OPTARG} | \
		    grep -qE '^(http|ftp)://.*/xetc[0-9][0-9]\.tgz$'; then
			XTGZ=${WRKDIR}/xetc.tgz
			XTGZURL=${OPTARG}
			if ! ${FETCH_CMD} -o ${XTGZ} ${XTGZURL}; then
				echo "\t*** ERROR: could not retrieve ${XTGZURL}"
				error_rm_wrkdir
			fi
		else
			echo "\t*** ERROR: ${OPTARG} is not a path to xetcXX.tgz"
			error_rm_wrkdir
		fi
		;;
	*)
		usage
		error_rm_wrkdir
		;;
	esac
done

shift $(( OPTIND -1 ))
if [ $# -ne 0 ]; then
	usage
	error_rm_wrkdir
fi


if [ -z "${SRCDIR}" -a -z "${TGZ}" -a -z "${XTGZ}" ]; then
	if [ -f "/usr/src/etc/Makefile" ]; then
		SRCDIR=/usr/src
	else
		echo "\t*** ERROR: please specify a valid path to src or (x)etcXX.tgz"
		usage
		error_rm_wrkdir
	fi
fi

TEMPROOT="${WRKDIR}/temproot"
BKPDIR="${WRKDIR}/backups"

do_populate
do_compare
do_post
