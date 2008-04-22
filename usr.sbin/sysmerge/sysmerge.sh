#!/bin/sh -
#
# $OpenBSD: sysmerge.sh,v 1.1 2008/04/22 20:53:16 ajacoutot Exp $
#
# This script is based on the FreeBSD mergemaster script which is
# Copyright (c) 1998-2003 Douglas Barton <DougB@FreeBSD.org>
#
# Some ideas came from the NetBSD etcupdate script, written by
# Martti Kuparinen <martti@NetBSD.org>
#
# Copyright (c) 2008 Antoine Jacoutot <ajacoutot@openbsd.org>
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
PATH="/bin:/usr/bin:/sbin:/usr/sbin"

PAGER="${PAGER:=/usr/bin/more}"
SWIDTH=`stty size | awk '{w=$2}END{if(w==0){w=80}print w}'`


usage() {
cat << EOF
usage: ${0##*/} [-a] [-s src | etcXX.tgz]
EOF
}


yesno() {
echo -n "${*}? (y|[n]) "
read ANSWER
case "${ANSWER}" in
	y|Y)
		echo ""
		return 0
		;;
	*)
		return 1
		;;
esac
}


do_pre() {
if [ `id -u` -ne 0 ]; then
	echo " *** ERROR: Need root privilege to run this script"
	exit 1
fi

WRKDIR=`mktemp -d -p /var/tmp sysmerge.XXXXX` || exit 1
TEMPROOT="${WRKDIR}/temproot"
BKPDIR="${WRKDIR}/backups"

trap "rm -rf ${WRKDIR}; exit 1" 1 2 3 13 15

if [ -z "${SRCMODE}" -a -z "${TGZMODE}" ]; then
	SRCMODE=1
	SRCDIR=/usr/src
fi

echo "\n===> Running ${0##*/} with the following settings:\n"
if [ "${AUTOMODE}" ]; then
	echo " auto-mode:            yes"
fi
echo " source:               ${SRCDIR}${TGZ}"
echo " base work directory:  ${WRKDIR}"
echo " temp root directory:  ${TEMPROOT}"
echo " backup directory:     ${BKPDIR}"
echo ""
if yesno "Continue"; then
	echo -n ""
else
	rmdir ${WRKDIR} 2> /dev/null
	exit 1
fi
}


do_populate() {
if [ "${SRCMODE}" -o "${TGZMODE}" ]; then
	echo "===> Creating and populating temporary root under ${TEMPROOT}"
	mkdir -p ${TEMPROOT}
	if [ "${SRCMODE}" ]; then
		cd ${SRCDIR}/etc
		make DESTDIR=${TEMPROOT} distribution-etc-root-var 2>&1 1> /dev/null \
		  | tee | grep -v "WARNING\: World writable directory"
	fi

	if [ "${TGZMODE}" ]; then
		for i in ${TGZ}; do tar -xzphf ${i} -C ${TEMPROOT}; done
	fi

	# files we don't want/need to deal with
	IGNORE_FILES="/etc/*.db /etc/mail/*.db /etc/passwd /etc/motd /etc/myname /var/mail/root"
	CF_FILES="/etc/mail/localhost.cf /etc/mail/sendmail.cf /etc/mail/submit.cf"
	for cf in ${CF_FILES}; do
		CF_DIFF=`diff -u -I "##### built by .* on" ${TEMPROOT}/${cf} ${DESTDIR}/${cf}`
		if [ -z "${CF_DIFF}" ]; then
			IGNORE_FILES="${IGNORE_FILES} ${cf}"
		fi
	done
	for i in ${IGNORE_FILES}; do rm -f ${TEMPROOT}/${i}; done
fi
}


do_install_and_rm() {
if [ -f "${5}/${4##*/}" ]; then
	mkdir -p ${BKPDIR}/${4%/*}
	cp ${5}/${4##*/} ${BKPDIR}/${4%/*}
fi

install -m "${1}" -o "${2}" -g "${3}" "${4}" "${5}"
rm -f "${4}"
}


mm_install() {
local INSTDIR
INSTDIR=${1#.}
INSTDIR=${INSTDIR%/*}

if [ -z "${INSTDIR}" ]; then INSTDIR=/; fi

DIR_OWN=`stat -f "%OMp%OLp" "${TEMPROOT}/${INSTDIR}"`
eval `stat -f "FILE_MODE=%OMp%OLp FILE_OWN=%Su FILE_GRP=%Sg" ${1}`

if [ -n "${DESTDIR}${INSTDIR}" -a ! -d "${DESTDIR}${INSTDIR}" ]; then
	install -d -o root -g wheel -m "${DIR_MODE}" "${DESTDIR}${INSTDIR}"
fi

do_install_and_rm "${FILE_MODE}" "${FILE_OWN}" "${FILE_GRP}" "${1}" "${DESTDIR}${INSTDIR}"

case "${1#.}" in
	/dev/MAKEDEV)
		NEED_MAKEDEV=1
		;;
	/etc/login.conf)
		if [ -f ${DESTDIR}/etc/login.conf.db ]; then NEED_CAP_MKDB=1; fi
		;;
	/etc/mail/aliases)
		NEED_NEWALIASES=1
		;;
	/etc/master.passwd)
		NEED_PWD_MKDB=1
		;;
esac
}


merge_loop() {
echo "===> Type h at the sdiff prompt (%) to get usage help\n"
MERGE_AGAIN=1
while [ "${MERGE_AGAIN}" ]; do
	cp -p "${COMPFILE}" "${COMPFILE}.merged"
	sdiff -as -o "${COMPFILE}.merged" -w ${SWIDTH} \
		"${DESTDIR}${COMPFILE#.}" "${COMPFILE}"
	INSTALL_MERGED=v
	while [ "${INSTALL_MERGED}" = "v" ]; do
		echo ""
		echo "  Use 'i' to install merged file"
		echo "  Use 'r' to re-do the merge"
		echo "  Use 'v' to view the merged file"
		echo "  Default is to leave the temporary file to deal with by hand"
		echo ""
		echo -n "===> How should I deal with the merged file? [Leave it for later] "
		read INSTALL_MERGED
		case "${INSTALL_MERGED}" in
			[iI])
				mv "${COMPFILE}.merged" "${COMPFILE}"
				echo ""
					if mm_install "${COMPFILE}"; then
						echo "===> Merged version of ${COMPFILE} installed successfully"
					else
						echo " *** WARNING: Problem installing ${COMPFILE}, it will remain to merge by hand"
					fi
				unset MERGE_AGAIN
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
HANDLE_COMPFILE=v

while [ "${HANDLE_COMPFILE}" = "v" -o "${HANDLE_COMPFILE}" = "todo" ]; do
	if [ "${HANDLE_COMPFILE}" = "v" ]; then
		echo "\n========================================================================\n"
	fi
	if [ -f "${DESTDIR}${COMPFILE#.}" -a -f "${COMPFILE}" ]; then
		if [ "${HANDLE_COMPFILE}" = "v" ]; then
			(
				echo "===> Displaying differences between ${COMPFILE} and installed version:"
				echo ""
				diff -u "${DESTDIR}${COMPFILE#.}" "${COMPFILE}"
			) | ${PAGER}
		echo ""
		fi
	else
		echo "===> ${COMPFILE} was not found on the target system\n"
		if [ -z "${AUTOMODE}" ]; then
			NO_INSTALLED=1
		else
			if mm_install "${COMPFILE}"; then
				AUTO_INSTALLED_FILES="${AUTO_INSTALLED_FILES}${DESTDIR}${COMPFILE#.}\n"
			else
				echo " *** WARNING: Problem installing ${COMPFILE}, it will remain to merge by hand"
			fi
			return
		fi
	fi

	echo "  Use 'd' to delete the temporary ${COMPFILE}"
	echo "  Use 'i' to install the temporary ${COMPFILE}"
	if [ -z "${NO_INSTALLED}" ]; then
		echo "  Use 'm' to merge the temporary and installed versions"
		echo "  Use 'v' to view the diff results again"
	fi
	echo ""
	echo "  Default is to leave the temporary file to deal with by hand"
	echo ""
	echo -n "How should I deal with this? [Leave it for later] "
	read HANDLE_COMPFILE

	case "${HANDLE_COMPFILE}" in
		[dD])
			rm "${COMPFILE}"
			echo "\n===> Deleting ${COMPFILE}"
			;;
		[iI])
			echo ""
			if mm_install "${COMPFILE}"; then
				echo "===> ${COMPFILE} installed successfully"
			else
				echo " *** WARNING: Problem installing ${COMPFILE}, it will remain to merge by hand"
			fi
			;;
		[mM])
			if [ -z "${NO_INSTALLED}" ]; then
				merge_loop
			else
				HANDLE_COMPFILE="todo"
			fi
			;;
		[vV])
			HANDLE_COMPFILE="v"
			continue
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
if [ ! -d "${TEMPROOT}" ]; then
	echo " *** ERROR: ${TEMPROOT} does not exist!"
	exit 1
fi

cd ${TEMPROOT}

# use -size +0 to avoid comparing empty log files and device nodes
for COMPFILE in `find . -type f -size +0`; do
	if [ ! -e "${DESTDIR}${COMPFILE#.}" ]; then
		diff_loop
		continue
	fi

	# compare CVS $Id's first so if the file hasn't been modified,
	# it will be deleted from temproot and ignored from comparison
	if [ "${AUTOMODE}" ]; then
		CVSID1=`grep "[$]OpenBSD:" ${DESTDIR}${COMPFILE#.} 2>/dev/null`
		CVSID2=`grep "[$]OpenBSD:" ${COMPFILE} 2>/dev/null` || CVSID2=none
		if [ "${CVSID2}" = "${CVSID1}" ]; then rm "${COMPFILE}"; fi
	fi

	if [ -f "${COMPFILE}" ]; then
		# make sure files are different; if not, delete the one in temproot
		if diff -q "${DESTDIR}${COMPFILE#.}" "${COMPFILE}" > /dev/null 2>&1; then
			rm "${COMPFILE}"
		else
			diff_loop
		fi
	fi
done

echo "\n===> Comparison complete"
}


do_post() {
if [ "${AUTO_INSTALLED_FILES}" ]; then
	echo "${AUTO_INSTALLED_FILES}" > ${WRKDIR}/auto_installed_files
fi

if [ "${NEED_CAP_MKDB}" ]; then
	echo -n "===> You installed a new ${DESTDIR}/etc/login.conf file, "
	if [ "${AUTOMODE}" ]; then
		echo "running cap_mkdb"
		/usr/bin/cap_mkdb ${DESTDIR}/etc/login.conf
	else
		echo "\n    rebuild your login.conf database by running the following command as root:"
		echo "    '/usr/bin/cap_mkdb ${DESTDIR}/etc/login.conf'"
	fi
fi

if [ "${NEED_PWD_MKDB}" ]; then
	echo -n "===> A new ${DESTDIR}/etc/master.passwd file was installed, "
	if [ "${AUTOMODE}" ]; then
		echo "running pwd_mkdb"
		/usr/sbin/pwd_mkdb -d ${DESTDIR}/etc -p ${DESTDIR}/etc/master.passwd
	else
		echo "\n    rebuild your password files by running the following command as root:"
		echo "    '/usr/sbin/pwd_mkdb -d ${DESTDIR}/etc -p ${DESTDIR}/etc/master.passwd'"
	fi
fi

if [ "${NEED_MAKEDEV}" ]; then
	echo -n "===> A new ${DESTDIR}/dev/MAKEDEV script was installed, "
	if [ "${AUTOMODE}" ]; then
		echo "running MAKEDEV"
		cd ${DESTDIR}/dev && /bin/sh MAKEDEV all
	else
		echo "\n    rebuild your device nodes by running the following command as root:"
		echo "    'cd ${DESTDIR}/dev && /bin/sh MAKEDEV all'"
	fi
fi

if [ "${NEED_NEWALIASES}" ]; then
	echo -n "===> A new ${DESTDIR}/etc/mail/aliases file was installed, "
	if [ "${DESTDIR}" ]; then
		echo "\n    but the newaliases command is limited to the directories configured"
		echo "    in sendmail.cf.  Make sure to create your aliases database by"
		echo "    hand when your sendmail configuration is done."
	elif [ "${AUTOMODE}" ]; then
		echo "running newaliases"
		/usr/bin/newaliases
	else
		echo "\n    rebuild your aliases database by running the following command as root:"
		echo "    '/usr/bin/newaliases'"
	fi
fi

echo "===> Making sure your directory hierarchy has correct perms, running mtree"
/usr/sbin/mtree -qdef ${DESTDIR}/etc/mtree/4.4BSD.dist -p ${DESTDIR:=/} -U 1> /dev/null

FILES_IN_WRKDIR=`find ${WRKDIR} -type f -size +0 2>/dev/null`
if [ "${FILES_IN_WRKDIR}" ]; then
	FILES_IN_TEMPROOT=`find ${TEMPROOT} -type f -size +0 2>/dev/null`
	FILES_IN_BKPDIR=`find ${BKPDIR} -type f -size +0 2>/dev/null`
	if [ "${AUTO_INSTALLED_FILES}" ]; then
		echo "===> Automatically installed file(s) listed in"
		echo "     ${WRKDIR}/auto_installed_files"
	fi
	if [ "${FILES_IN_TEMPROOT}" ]; then
		echo "===> File(s) remaining for you to merge by hand:"
		find "${TEMPROOT}" -type f -size +0 -exec echo "     {}" \;
	fi
	if [ "${FILES_IN_BKPDIR}" ]; then
		echo "===> Backup of replaced file(s) can be found under ${BKPDIR}"
	fi
	echo "===> When done, ${WRKDIR} and its sub-directories should be removed"
else
	echo "===> Removing ${WRKDIR}"
	rm -rf "${WRKDIR}"
fi
}


ARGS=`getopt as: $*`
if [ $? -ne 0 ]; then
	usage
	exit 1
fi
set -- ${ARGS}
while [ $# -ne 0 ]
do
	case "$1" in
		-a)
			AUTOMODE=1
			shift;;
		-s)
			WHERE="${2}"
			shift 2
			if [ -d "${WHERE}" ]; then
				SRCMODE=1
				SRCDIR=${WHERE}
			elif [ -f "${WHERE}" ]; then
				TGZMODE=1
				TGZ=${WHERE}
			else
				echo " *** ERROR: ${WHERE} is not a path to src nor etcXX.tgz"
				exit 1
			fi
			;;
		--)
			shift; break;;
	esac
done


do_pre
do_populate
do_compare
do_post
