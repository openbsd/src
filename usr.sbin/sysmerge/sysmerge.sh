#!/bin/sh -
#
# $OpenBSD: sysmerge.sh,v 1.33 2009/04/06 10:35:20 ajacoutot Exp $
#
# This script is based on the FreeBSD mergemaster script, written by
# Douglas Barton <DougB@FreeBSD.org>
#
# Some ideas came from the NetBSD etcupdate script, written by
# Martti Kuparinen <martti@NetBSD.org>
#
# Copyright (c) 1998-2003 Douglas Barton <DougB@FreeBSD.org>
# Copyright (c) 2008, 2009 Antoine Jacoutot <ajacoutot@openbsd.org>
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

WRKDIR=`mktemp -d -p /var/tmp sysmerge.XXXXX` || exit 1
SWIDTH=`stty size | awk '{w=$2} END {if (w==0) {w=80} print w}'`
MERGE_CMD="${MERGE_CMD:=sdiff -as -w ${SWIDTH} -o}"
REPORT="${REPORT:=${WRKDIR}/sysmerge.log}"

EDITOR="${EDITOR:=/usr/bin/vi}"
PAGER="${PAGER:=/usr/bin/more}"

# clean leftovers created by make in src
clean_src() {
	if [ "${SRCDIR}" ]; then
		cd ${SRCDIR}/gnu/usr.sbin/sendmail/cf/cf && make cleandir > /dev/null
	fi
}

# remove newly created work directory and exit with status 1
error_rm_wrkdir() {
	rmdir ${WRKDIR} 2> /dev/null
	exit 1
}

usage() {
	echo "usage: ${0##*/} [-ab] [-S etcXX.tgz] [-s src | etcXX.tgz] [-X xetcXX.tgz] [-x xetcXX.tgz]" >&2
}

trap "clean_src; rm -rf ${WRKDIR}; exit 1" 1 2 3 13 15

if [ "`id -u`" -ne 0 ]; then
	echo " *** Error: need root privileges to run this script"
	usage
	error_rm_wrkdir
fi

if [ -z "${FETCH_CMD}" ]; then
	if [ -z "${FTP_KEEPALIVE}" ]; then
		FTP_KEEPALIVE=0
	fi
	FETCH_CMD="/usr/bin/ftp -V -m -k ${FTP_KEEPALIVE}"
fi


do_pre() {
	if [ -z "${SRCDIR}" -a -z "${TGZ}" -a -z "${XTGZ}" ]; then
		if [ -f "/usr/src/etc/Makefile" ]; then
			SRCDIR=/usr/src
		else
			echo " *** Error: please specify a valid path to src or (x)etcXX.tgz"
			error_rm_wrkdir
		fi
	fi

	if [ "${OTGZ}" ]; then
		TGZV=`echo ${TGZ} | sed -e 's,^.*/,,' -e 's,etc,,' -e 's,.tgz,,'`
		OTGZV=`echo ${OTGZ} | sed -e 's,^.*/,,' -e 's,etc,,' -e 's,.tgz,,'`
		if [ -z "${TGZ}" ]; then
			echo " *** Error: please specify a valid path to the new etcXX.tgz"
			error_rm_wrkdir
		elif cmp -s ${OTGZ} ${TGZ}; then
			echo " *** Error: old and new etcXX.tgz are identical"
			error_rm_wrkdir
		elif [ "${OTGZV}" -gt "${TGZV}" ]; then
			echo " *** Error: old etc${OTGZV}.tgz version is higher than new etc${TGZV}.tgz"
			error_rm_wrkdir
		fi
	fi
	
	if [ "${OXTGZ}" ]; then
		XTGZV=`echo ${XTGZ} | sed -e 's,^.*/,,' -e 's,etc,,' -e 's,.tgz,,'`
		OXTGZV=`echo ${OXTGZ} | sed -e 's,^.*/,,' -e 's,etc,,' -e 's,.tgz,,'`
		if [ -z "${XTGZ}" ]; then
			echo " *** Error: please specify a valid path to the new xetcXX.tgz"
			error_rm_wrkdir
		elif cmp -s ${OXTGZ} ${XTGZ}; then
			echo " *** Error: old and new xetcXX.tgz are identical"
			error_rm_wrkdir
		elif [ "${OXTGZV}" -gt "${XTGZV}" ]; then
			echo " *** Error: old xetc${OXTGZV}.tgz version is higher than new xetc${XTGZV}.tgz"
			error_rm_wrkdir
		fi
	fi

	TEMPROOT="${WRKDIR}/temproot"
	BKPDIR="${WRKDIR}/backups"

	if [ -z "${BATCHMODE}" -a -z "${AUTOMODE}" ]; then
		echo "\n===> Running ${0##*/} with the following settings:\n"
		if [ "${TGZURL}" ]; then
			echo " etc source:          ${TGZURL}"
			echo "                      (fetched in ${TGZ})"
		elif [ "${TGZ}" ]; then
			echo " etc source:          ${TGZ}"
		elif [ "${SRCDIR}" ]; then
			echo " etc source:          ${SRCDIR}"
		fi
		if [ "${OTGZURL}" ]; then
			echo " old etc source:      ${OTGZURL}"
			echo "                      (fetched in ${OTGZ})"
		else
			[ "${OTGZ}" ] && echo " old etc source:      ${OTGZ}"
		fi
		if [ "${OTGZ}" -o "${OXTGZ}" ]; then echo ""; fi
		if [ "${XTGZURL}" ]; then
			echo " xetc source:         ${XTGZURL}"
			echo "                      (fetched in ${XTGZ})"
		else
			[ "${XTGZ}" ] && echo " xetc source:         ${XTGZ}"
		fi
		if [ "${OXTGZURL}" ]; then
			echo " old xetc source:     ${OXTGZURL}"
			echo "                      (fetched in ${OXTGZ})"
		else
			[ "${OXTGZ}" ] && echo " old xetc source:     ${OXTGZ}"
		fi
		echo ""
		echo " base work directory: ${WRKDIR}"
		echo " temp root directory: ${TEMPROOT}"
		echo " backup directory:    ${BKPDIR}"
		echo ""
		echo -n "Continue? (y|[n]) "
		read ANSWER
		case "${ANSWER}" in
			y|Y)
				echo ""
				;;
			*)
				error_rm_wrkdir
				;;
		esac
	fi
}


do_populate() {
	echo "===> Creating and populating temporary root under"
	echo "     ${TEMPROOT}"
	mkdir -p ${TEMPROOT}
	if [ "${SRCDIR}" ]; then
		cd ${SRCDIR}/etc
		make DESTDIR=${TEMPROOT} distribution-etc-root-var > /dev/null 2>&1
	fi

	if [ "${TGZ}" -o "${XTGZ}" ]; then
		for i in ${TGZ} ${XTGZ}; do
			tar -xzphf ${i} -C ${TEMPROOT};
		done
	fi

	if [ "${OTGZ}" -o "${OXTGZ}" ]; then
		OTEMPROOT="${WRKDIR}/.otemproot"
		mkdir -p ${OTEMPROOT}
		for i in ${OTGZ} ${OXTGZ}; do
			tar -xzphf ${i} -C ${OTEMPROOT};
			tar -tzf ${i} >> ${WRKDIR}/olist;
		done
		for i in ${TGZ} ${XTGZ}; do
			tar -tzf ${i} >> ${WRKDIR}/nlist;
		done
		OBSOLETE_FILES=`diff -C 0 ${WRKDIR}/olist ${WRKDIR}/nlist | grep -E '^- .' | sed -e 's,^- .,,g'`
		rm -f ${WRKDIR}/olist ${WRKDIR}/nlist
	fi

	# files we don't want/need to deal with
	IGNORE_FILES="/etc/*.db /etc/mail/*.db /etc/passwd /etc/motd /etc/myname /var/mail/root"
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
		echo -n "===> A new ${DESTDIR}/dev/MAKEDEV script was installed, "
		echo "MAKEDEV will be run"
		cd ${DESTDIR}/dev && /bin/sh MAKEDEV all
		;;
	/etc/login.conf)
		if [ -f ${DESTDIR}/etc/login.conf.db ]; then
			echo -n "===> A new ${DESTDIR}/etc/login.conf file was installed, "
			echo "cap_mkdb will be run"
			cap_mkdb ${DESTDIR}/etc/login.conf
		fi
		;;
	/etc/mail/aliases)
		echo -n "===> A new ${DESTDIR}/etc/mail/aliases file was installed, "
		echo "newaliases will be run"
		if [ "${DESTDIR}" ]; then
			chroot ${DESTDIR} newaliases || NEED_NEWALIASES=1
		else
			newaliases
		fi
		;;
	/etc/master.passwd)
		echo -n "===> A new ${DESTDIR}/etc/master.passwd file was installed, "
		echo "pwd_mkdb will be run"
		pwd_mkdb -d ${DESTDIR}/etc -p ${DESTDIR}/etc/master.passwd
		;;
	esac
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
			echo "  Default is to leave the temporary file to deal with by hand"
			echo ""
			echo -n "===> How should I deal with the merged file? [Leave it for later] "
			read INSTALL_MERGED
			case "${INSTALL_MERGED}" in
			[eE])
				echo "editing merged file...\n"
				if [ -z "${VISUAL}" ]; then
					EDIT="${EDITOR}"
				else
					EDIT="${VISUAL}"
				fi
				if which ${EDIT} > /dev/null 2>&1; then
					${EDIT} ${COMPFILE}.merged
				else
					echo " *** Error: ${EDIT} can not be found or is not executable"
				fi
				INSTALL_MERGED=v
				;;
			[iI])
				mv "${COMPFILE}.merged" "${COMPFILE}"
				echo ""
					if mm_install "${COMPFILE}"; then
						echo "===> Merged version of ${COMPFILE} installed successfully"
					else
						echo " *** Warning: problem installing ${COMPFILE}, it will remain to merge by hand"
					fi
				unset MERGE_AGAIN
				;;
			[nN])
				echo "comparison between merged and new files:\n"
				diff -u ${COMPFILE}.merged ${COMPFILE}
				INSTALL_MERGED=v
				;;
			[oO])
				echo "comparison between old and merged files:\n"
				diff -u ${DESTDIR}${COMPFILE#.} ${COMPFILE}.merged
				INSTALL_MERGED=v
				;;
			[rR])
				rm "${COMPFILE}.merged"
				;;
			[vV])
				${PAGER} "${COMPFILE}.merged"
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

	while [ "${HANDLE_COMPFILE}" = "v" -o "${HANDLE_COMPFILE}" = "todo" ]; do
		if [ "${HANDLE_COMPFILE}" = "v" ]; then
			echo "\n========================================================================\n"
		fi
		if [ -f "${DESTDIR}${COMPFILE#.}" -a -f "${COMPFILE}" ]; then
			# if current != new and current = old, auto-install new
			if [ "${OTGZ}" -o "${OXTGZ}" ]; then
				if diff -q "${DESTDIR}${COMPFILE#.}" "${OTEMPROOT}${COMPFILE#.}" > /dev/null 2>&1; then
					echo "===> ${COMPFILE} has not been modified since previous release/snapshot,"
					echo "     automatically installing new version"
					if mm_install "${COMPFILE}"; then
						echo "===> ${COMPFILE} installed successfully"
						AUTO_INSTALLED_FILES="${AUTO_INSTALLED_FILES}${DESTDIR}${COMPFILE#.}\n"
					else
						echo " *** Warning: problem installing ${COMPFILE}, it will remain to merge by hand"
					fi
					return
				fi
			fi
			if [ "${HANDLE_COMPFILE}" = "v" ]; then
				(
					echo "===> Displaying differences between ${COMPFILE} and installed version:"
					echo ""
					diff -u "${DESTDIR}${COMPFILE#.}" "${COMPFILE}"
				) | ${PAGER}
				echo ""
			fi
		else
			echo "===> ${COMPFILE} was not found on the target system"
			if [ -z "${AUTOMODE}" ]; then
				echo ""
				NO_INSTALLED=1
			else
				if mm_install "${COMPFILE}"; then
					echo "===> ${COMPFILE} installed successfully"
					AUTO_INSTALLED_FILES="${AUTO_INSTALLED_FILES}${DESTDIR}${COMPFILE#.}\n"
				else
					echo " *** Warning: problem installing ${COMPFILE}, it will remain to merge by hand"
				fi
				return
			fi
		fi

		if [ -z "${BATCHMODE}" ]; then
			echo "  Use 'd' to delete the temporary ${COMPFILE}"
			if [ "${COMPFILE}" != "./etc/master.passwd" -a "${COMPFILE}" != "./etc/group" ]; then
				echo "  Use 'i' to install the temporary ${COMPFILE}"
			fi
			if [ -z "${NO_INSTALLED}" -a -z "${IS_BINFILE}" ]; then
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
			if [ "${COMPFILE}" != "./etc/master.passwd" -a "${COMPFILE}" != "./etc/group" ]; then
				echo ""
				if mm_install "${COMPFILE}"; then
					echo "===> ${COMPFILE} installed successfully"
				else
					echo " *** Warning: problem installing ${COMPFILE}, it will remain to merge by hand"
				fi
			else
				echo "invalid choice: ${HANDLE_COMPFILE}\n"
				HANDLE_COMPFILE="todo"
			fi
				
			;;
		[mM])
			if [ -z "${NO_INSTALLED}" -a -z "${IS_BINFILE}" ]; then
				merge_loop
			else
				echo "invalid choice: ${HANDLE_COMPFILE}\n"
				HANDLE_COMPFILE="todo"
			fi
			;;
		[vV])
			if [ -z "${NO_INSTALLED}" -a -z "${IS_BINFILE}" ]; then
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

	unset NO_INSTALLED
}


do_compare() {
	echo "===> Starting comparison"

	cd ${TEMPROOT} || error_rm_wrkdir

	# use -size +0 to avoid comparing empty log files and device nodes
	for COMPFILE in `find . -type f -size +0`; do
		if [ ! -e "${DESTDIR}${COMPFILE#.}" ]; then
			diff_loop
			continue
		fi

		# compare CVS $Id's first so if the file hasn't been modified,
		# it will be deleted from temproot and ignored from comparison.
		# several files are generated from scripts so CVS ID is not a
		# reliable way of detecting changes; leave for a full diff.
		if [ "${AUTOMODE}" -a "${COMPFILE}" != "./etc/fbtab" \
		    -a "${COMPFILE}" != "./etc/login.conf" \
		    -a "${COMPFILE}" != "./etc/sysctl.conf" \
		    -a "${COMPFILE}" != "./etc/ttys" ]; then
			CVSID1=`grep "[$]OpenBSD:" ${DESTDIR}${COMPFILE#.} 2> /dev/null`
			CVSID2=`grep "[$]OpenBSD:" ${COMPFILE} 2> /dev/null` || CVSID2=none
			if [ "${CVSID2}" = "${CVSID1}" ]; then rm "${COMPFILE}"; fi
		fi

		if [ -f "${COMPFILE}" ]; then
			# make sure files are different; if not, delete the one in temproot
			if diff -q "${DESTDIR}${COMPFILE#.}" "${COMPFILE}" > /dev/null 2>&1; then
				rm "${COMPFILE}"
			# delete file in temproot if it has not changed since last release
			elif [ "${OTGZ}" -o "${OXTGZ}" ] && diff -q "${COMPFILE}" "${OTEMPROOT}${COMPFILE#.}" > /dev/null 2>&1; then
				rm "${COMPFILE}"
			# xetcXX.tgz contains binary files; set IS_BINFILE to disable sdiff
			elif diff -q "${DESTDIR}${COMPFILE#.}" "${COMPFILE}" | grep "Binary" > /dev/null 2>&1; then
				IS_BINFILE=1
				diff_loop
			else
				unset IS_BINFILE
				diff_loop
			fi
		fi
	done

	echo "\n===> Comparison complete"
}


do_post() {
	if [ "${NEED_NEWALIASES}" ]; then
		echo "===> A new ${DESTDIR}/etc/mail/aliases file was installed."
		echo "     However ${DESTDIR}/usr/bin/newaliases could not be run,"
		echo "     you will need to rebuild your aliases database manually."
        fi

	clean_src
	rm -rf ${OTEMPROOT}

	echo "===> Making sure your directory hierarchy has correct perms, running mtree"
	mtree -qdef ${DESTDIR}/etc/mtree/4.4BSD.dist -p ${DESTDIR:=/} -U > /dev/null

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
		echo "===> File(s) removed from previous set (maybe obsolete)" >> ${REPORT}
		echo "${OBSOLETE_FILES}" >> ${REPORT}
	fi
	if [ "${FILES_IN_TEMPROOT}" ]; then
		echo "===> File(s) remaining for you to merge by hand" >> ${REPORT}
		echo "${FILES_IN_TEMPROOT}" >> ${REPORT}
	fi

	if [ -e "${REPORT}" ]; then
		echo "===> Manual intervention may be needed, see ${REPORT}"
		echo "===> When done, ${WRKDIR} and its subdirectories should be removed"
	else
		echo "===> Removing ${WRKDIR}"
		rm -rf "${WRKDIR}"
	fi
}


while getopts abs:x:S:X: arg; do
	case ${arg} in
	a)
		AUTOMODE=1
		;;
	b)
		BATCHMODE=1
		;;
	s)
		if [ -f "${OPTARG}/etc/Makefile" ]; then
			SRCDIR=${OPTARG}
		elif [ -f "${OPTARG}" ] && echo -n ${OPTARG} | \
		    awk -F/ '{print $NF}' | \
		    grep '^etc[0-9][0-9]\.tgz$' > /dev/null 2>&1 ; then
			TGZ=${OPTARG}
		elif echo ${OPTARG} | \
		    grep -qE '^(http|ftp)://.*/etc[0-9][0-9]\.tgz$'; then
			TGZ=${WRKDIR}/etc.tgz
			TGZURL=${OPTARG}
			if ! ${FETCH_CMD} -o ${TGZ} ${TGZURL}; then
				echo " *** Error: could not retrieve ${TGZURL}"
				error_rm_wrkdir
			fi
		else
			echo " *** Error: ${OPTARG} is not a path to src nor etcXX.tgz"
			error_rm_wrkdir
		fi
		;;
	x)
		if [ -f "${OPTARG}" ] && echo -n ${OPTARG} | \
		    awk -F/ '{print $NF}' | \
		    grep '^xetc[0-9][0-9]\.tgz$' > /dev/null 2>&1 ; then
			XTGZ=${OPTARG}
		elif echo ${OPTARG} | \
		    grep -qE '^(http|ftp)://.*/xetc[0-9][0-9]\.tgz$'; then
			XTGZ=${WRKDIR}/xetc.tgz
			XTGZURL=${OPTARG}
			if ! ${FETCH_CMD} -o ${XTGZ} ${XTGZURL}; then
				echo " *** Error: could not retrieve ${XTGZURL}"
				error_rm_wrkdir
			fi
		else
			echo " *** Error: ${OPTARG} is not a path to xetcXX.tgz"
			error_rm_wrkdir
		fi
		;;
	S)
		if [ -f "${OPTARG}" ] && echo -n ${OPTARG} | \
		    awk -F/ '{print $NF}' | \
		    grep '^etc[0-9][0-9]\.tgz$' > /dev/null 2>&1 ; then
			OTGZ=${OPTARG}
		elif echo ${OPTARG} | \
		    grep -qE '^(http|ftp)://.*/etc[0-9][0-9]\.tgz$'; then
			OTGZ=${WRKDIR}/etc.tgz
			OTGZURL=${OPTARG}
			if ! ${FETCH_CMD} -o ${OTGZ} ${OTGZURL}; then
				echo " *** Error: could not retrieve ${OTGZURL}"
				error_rm_wrkdir
			fi
		else
			echo " *** Error: ${OPTARG} is not a path to etcXX.tgz"
			error_rm_wrkdir
		fi
		;;
	X)
		if [ -f "${OPTARG}" ] && echo -n ${OPTARG} | \
		    awk -F/ '{print $NF}' | \
		    grep '^xetc[0-9][0-9]\.tgz$' > /dev/null 2>&1 ; then
			OXTGZ=${OPTARG}
		elif echo ${OPTARG} | \
		    grep -qE '^(http|ftp)://.*/xetc[0-9][0-9]\.tgz$'; then
			OXTGZ=${WRKDIR}/xetc.tgz
			OXTGZURL=${OPTARG}
			if ! ${FETCH_CMD} -o ${OXTGZ} ${OXTGZURL}; then
				echo " *** Error: could not retrieve ${OXTGZURL}"
				error_rm_wrkdir
			fi
		else
			echo " *** Error: ${OPTARG} is not a path to xetcXX.tgz"
			error_rm_wrkdir
		fi
		;;
	*)
		usage
		error_rm_wrkdir
		;;
	esac
done


do_pre
do_populate
do_compare
do_post
