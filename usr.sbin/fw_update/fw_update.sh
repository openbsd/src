#!/bin/ksh
#	$OpenBSD: fw_update.sh,v 1.26 2022/01/06 20:15:54 deraadt Exp $
#
# Copyright (c) 2021 Andrew Hewus Fresh <afresh1@openbsd.org>
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

set -o errexit -o pipefail -o nounset -o noclobber -o noglob
set +o monitor
export PATH=/usr/bin:/bin:/usr/sbin:/sbin

CFILE=SHA256.sig
DESTDIR=${DESTDIR:-}
FWPATTERNS="${DESTDIR}/usr/share/misc/firmware_patterns"

VNAME=${VNAME:-$(sysctl -n kern.osrelease)}
VERSION=${VERSION:-"${VNAME%.*}${VNAME#*.}"}

HTTP_FWDIR="$VNAME"
VTYPE=$( sed -n "/^OpenBSD $VNAME\([^ ]*\).*$/s//\1/p" \
    /var/run/dmesg.boot | sed '$!d' )
[[ $VTYPE == -!(stable) ]] && HTTP_FWDIR=snapshots

FWURL=http://firmware.openbsd.org/firmware/${HTTP_FWDIR}
FWPUB_KEY=${DESTDIR}/etc/signify/openbsd-${VERSION}-fw.pub

DRYRUN=false
VERBOSE=false
DELETE=false
DOWNLOAD=true
INSTALL=true
LOCALSRC=

unset FTPPID
unset FWPKGTMP
REMOVE_LOCALSRC=false
cleanup() {
	set +o errexit # ignore errors from killing ftp
	[ "${FTPPID:-}" ] && kill -TERM -"$FTPPID" 2>/dev/null
	[ "${FWPKGTMP:-}" ] && rm -rf "$FWPKGTMP"
	"$REMOVE_LOCALSRC" && rm -rf "$LOCALSRC"
}
trap cleanup EXIT

tmpdir() {
	local _i=1 _dir

	# If we're not in the installer,
	# we have mktemp and a more hostile environment.
	if [ -x /usr/bin/mktemp ]; then
		_dir=$( mktemp -d "${1}-XXXXXXXXX" )
	else
		until _dir="${1}.$_i.$RANDOM" && mkdir -- "$_dir" 2>/dev/null; do
		    ((++_i < 10000)) || return 1
		done
	fi

	echo "$_dir"
}

fetch() {
	local _src="${FWURL}/${1##*/}" _dst=$1 _user=_file _exit _error=''

	# If we're not in the installer,
	# we have su(1) and doas(1) is unlikely to be configured.
	set -o monitor # make sure ftp gets its own process group
	(
	flags=-VM
	"$VERBOSE" && flags=-vm
	if [ -x /usr/bin/su ]; then
		exec /usr/bin/su -s /bin/ksh "$_user" -c \
		    "/usr/bin/ftp -N '${0##/}' -D 'Get/Verify' $flags -o- '$_src'" > "$_dst"
	else
		exec /usr/bin/doas -u "$_user" \
		    /usr/bin/ftp -N "${0##/}" -D 'Get/Verify' $flags -o- "$_src" > "$_dst"
	fi
	) & FTPPID=$!
	set +o monitor

	SECONDS=0
	_last=0
	while kill -0 -"$FTPPID" 2>/dev/null; do
		if [[ $SECONDS -gt 12 ]]; then
			set -- $( ls -ln "$_dst" 2>/dev/null )
			if [[ $_last -ne $5 ]]; then
				_last=$5
				SECONDS=0
				sleep 1
			else
				kill -INT -"$FTPPID"
				_error=" (timed out)"
			fi
		else
			sleep 1
		fi
	done

	set +o errexit
	wait "$FTPPID"
	_exit=$?
	set -o errexit

	unset FTPPID

	if [ "$_exit" -ne 0 ]; then
		rm -f "$_dst"
		echo "Cannot fetch $_src$_error" >&2
		return 1
	fi
}

verify() {
	# On the installer we don't get sha256 -C, so fake it.
	if ! fgrep -qx "SHA256 (${1##*/}) = $( /bin/sha256 -qb "$1" )" "$CFILE"; then
		echo "Checksum test for ${1##*/} failed." >&2
		return 1
	fi
}

firmware_in_dmesg() {
	local _d _m _line _dmesgtail _last='' _nl=$( echo )

	# When we're not in the installer, the dmesg.boot can
	# contain multiple boots, so only look in the last one
	_dmesgtail="$( echo ; sed -n 'H;/^OpenBSD/h;${g;p;}' /var/run/dmesg.boot )"

	grep -v '^[[:space:]]*#' "$FWPATTERNS" |
	    while read -r _d _m; do
		[ "$_d" = "$_last" ] && continue
		[ "$_m" ]             || _m="${_nl}${_d}[0-9] at "
		[ "$_m" = "${_m#^}" ] || _m="${_nl}${_m#^}"

		if [[ $_dmesgtail = *$_m* ]]; then
			echo "$_d"
			_last="$_d"
		fi
	    done
}

firmware_filename() {
	local _f
	_f="$( sed -n "s/.*(\($1-firmware-.*\.tgz\)).*/\1/p" "$CFILE" | sed '$!d' )"
	! [ "$_f" ] && echo "Unable to find firmware for $1" >&2 && return 1
	echo "$_f"
}

firmware_devicename() {
	local _d="${1##*/}"
	_d="${_d%-firmware-*}"
	echo "$_d"
}

installed_firmware() {
	local _pre="$1" _match="$2" _post="$3" _firmware
	set -sA _firmware -- $(
	    set +o noglob
	    grep -Fxl '@option firmware' \
		"${DESTDIR}/var/db/pkg/"$_pre"$_match"$_post"/+CONTENTS" \
		2>/dev/null || true
	    set -o noglob
	)

	[ "${_firmware[*]:-}" ] || return 0
	for fw in "${_firmware[@]}"; do
		fw="${fw%/+CONTENTS}"
		echo "${fw##*/}"
	done
}

detect_firmware() {
	local _devices _last='' _d

	set -sA _devices -- $(
	    firmware_in_dmesg
	    for _d in $( installed_firmware '*' '-firmware-' '*' ); do
		echo "$( firmware_devicename "$_d" )"
	    done
	)

	[ "${_devices[*]:-}" ] || return 0
	for _d in "${_devices[@]}"; do
		[[ $_last = $_d ]] && continue
		echo $_d
		_last="$_d"
	done
}

add_firmware () {
	local _f="${1##*/}" _pkgname
	FWPKGTMP="$( tmpdir "${DESTDIR}/var/db/pkg/.firmware" )"
	local flags=-VM
	"$VERBOSE" && flags=-vm
	ftp -N "${0##/}" -D "Install" "$flags" -o- "file:${1}" |
		tar -s ",^\+,${FWPKGTMP}/+," \
		    -s ",^firmware,${DESTDIR}/etc/firmware," \
		    -C / -zxphf - "+*" "firmware/*"

	_pkgname="$( sed -n '/^@name /{s///p;q;}' "${FWPKGTMP}/+CONTENTS" )"
	if [ ! "$_pkgname" ]; then
		echo "Failed to extract name from $1, partial install" 2>&1
		rm -rf "$FWPKGTMP"
		unset FWPKGTMP
		return 1
	fi

	# TODO: Should we mark these so real fw_update can -Drepair?
	ed -s "${FWPKGTMP}/+CONTENTS" <<EOL
/^@comment pkgpath/ -1a
@option manual-installation
@option firmware
@comment install-script
.
w
EOL

	chmod 755 "$FWPKGTMP"
	mv "$FWPKGTMP" "${DESTDIR}/var/db/pkg/${_pkgname}"
	unset FWPKGTMP
}

delete_firmware() {
	local _cwd _pkg="$1" _pkgdir="${DESTDIR}/var/db/pkg"

	# TODO: Check hash for files before deleting
	"$VERBOSE" && echo "Uninstalling $_pkg"
	_cwd="${_pkgdir}/$_pkg"

	if [ ! -e "$_cwd/+CONTENTS" ] ||
	    ! grep -Fxq '@option firmware' "$_cwd/+CONTENTS"; then
		echo "${0##*/}: $_pkg does not appear to be firmware" >&2
		return 2
	fi

	set -A _remove -- "${_cwd}/+CONTENTS" "${_cwd}"

	while read -r c g; do
		case $c in
		@cwd) _cwd="${DESTDIR}$g"
		  ;;
		@*) continue
		  ;;
		*) set -A _remove -- "$_cwd/$c" "${_remove[@]}"
		  ;;
		esac
	done < "${_pkgdir}/${_pkg}/+CONTENTS"

	# We specifically rm -f here because not removing files/dirs
	# is probably not worth failing over.
	for _r in "${_remove[@]}" ; do
		if [ -d "$_r" ]; then
			# Try hard not to actually remove recursively
			# without rmdir on the install media.
			set +o noglob
			[ "$_r/*" = "$( echo "$_r"/* )" ] && rm -rf "$_r"
			set -o noglob
		else
			rm -f "$_r"
		fi
	done
}

usage() {
	echo "usage: ${0##*/} [-d | -F] [-av] [-p path] [driver | file ...]"
	exit 2
}

ALL=false
OPT_F=
while getopts :adFnp:v name
do
       case "$name" in
       a) ALL=true ;;
       d) DELETE=true ;;
       F) OPT_F=true ;;
       n) DRYRUN=true ;;
       p) LOCALSRC="$OPTARG" ;;
       v) VERBOSE=true ;;
       :)
	   echo "${0##*/}: option requires an argument -- -$OPTARG" >&2
	   usage 2
	   ;;
       ?)
	   echo "${0##*/}: unknown option -- -$OPTARG" >&2
	   usage 2
	   ;;
       esac
done
shift $((OPTIND - 1))

if [ "$LOCALSRC" ]; then
	if [[ $LOCALSRC = @(ftp|http?(s))://* ]]; then
		FWURL="${LOCALSRC}"
		LOCALSRC=
	else
		LOCALSRC="${LOCALSRC:#file:}"
		! [ -d "$LOCALSRC" ] &&
		    echo "The path must be a URL or an existing directory" >&2 &&
		    exit 2
	fi
fi

# "Download only" means local dir and don't install
if [ "$OPT_F" ]; then
	INSTALL=false
	LOCALSRC="${LOCALSRC:-.}"
elif [ "$LOCALSRC" ]; then
	DOWNLOAD=false
fi

if [ -x /usr/bin/id ] && [ "$(/usr/bin/id -u)" != 0 ]; then
	echo "need root privileges" >&2
	exit 1
fi

set -sA devices -- "$@"

if "$DELETE"; then
	[ "$OPT_F" ] && usage 22

	set -A installed
	if [ "${devices[*]:-}" ]; then
		"$ALL" && usage 22

		set -A installed -- $(
		    for d in "${devices[@]}"; do
			f="${d##*/}"  # only care about the name
			f="${f%.tgz}" # allow specifying the package name
			[ "$( firmware_devicename "$f" )" = "$f" ] && f="$f-firmware"

			set -A i -- $( installed_firmware '' "$f-" '*' )

			if [ "${i[*]:-}" ]; then
				echo "${i[@]}"
			else
				echo "No firmware found for '$d'" >&2
			fi
		    done
		)
	elif "$ALL"; then
		set -A installed -- $( installed_firmware '*' '-firmware-' '*' )
	fi

	deleted=''
	if [ "${installed:-}" ]; then
		for fw in "${installed[@]}"; do
			if "$DRYRUN"; then
				echo "Delete $fw"
			else
				delete_firmware "$fw" || continue
			fi
			deleted="$deleted,$( firmware_devicename "$fw" )"
		done
	fi

	deleted="${deleted:+${deleted#,}}"
	echo "${0:##*/}: deleted ${deleted:-none}";

	exit
fi

if [ ! "$LOCALSRC" ]; then
    LOCALSRC="$( tmpdir "${DESTDIR}/tmp/${0##*/}" )"
    REMOVE_LOCALSRC=true
fi

CFILE="$LOCALSRC/$CFILE"

if [ "${devices[*]:-}" ]; then
	"$ALL" && usage 22
else
	"$VERBOSE" && echo -n "Detecting firmware ..."
	set -sA devices -- $( detect_firmware )
	"$VERBOSE" &&
	    { [ "${devices[*]:-}" ] && echo " found." || echo " done." ; }
fi

[ "${devices[*]:-}" ] || exit

if "$DOWNLOAD"; then
	set +o noclobber # we want to get the latest CFILE
	fetch "$CFILE"
	set -o noclobber
	! signify -qVep "$FWPUB_KEY" -x "$CFILE" -m "$CFILE" &&
	    echo "Signature check of SHA256.sig failed" >&2 && exit 1
elif [ ! -e "$CFILE" ]; then
	# TODO: We shouldn't need a CFILE if all arguments are files.
	echo "${0##*/}: $CFILE: No such file or directory" >&2
	exit 2
fi

added=''
updated=''
kept=''
for f in "${devices[@]}"; do
	d="$( firmware_devicename "$f" )"

	if [ "$f" = "$d" ]; then
		f=$( firmware_filename "$d" || true )
		[ "$f" ] || continue
		f="$LOCALSRC/$f"
	elif ! "$INSTALL" && ! grep -Fq "($f)" "$CFILE" ; then
		echo "Cannot download local file $f" >&2
		exit 2
	fi

	set -A installed -- $( installed_firmware '' "$d-firmware-" '*' )

	if "$INSTALL" && [ "${installed[*]:-}" ]; then
		for i in "${installed[@]}"; do
			if [ "${f##*/}" = "$i.tgz" ]; then
				"$VERBOSE" && echo "Keep $i"
				kept="$kept,$d"
				continue 2
			fi
		done
	fi

	if [ -e "$f" ]; then
		if "$DOWNLOAD"; then
			"$VERBOSE" && ! "$INSTALL" &&
			    echo "Keep/Verify ${f##*/}"
			"$DRYRUN"  || verify "$f" || continue
			"$INSTALL" || kept="$kept,$d"
		# else assume it was verified when downloaded
		fi
	elif "$DOWNLOAD"; then
		if "$DRYRUN"; then
			"$VERBOSE" && echo "Get/Verify ${f##*/}"
		else
			fetch  "$f" || continue
			verify "$f" || continue
		fi
		"$INSTALL"  || added="$added,$d"
	elif "$INSTALL"; then
		echo "Cannot install ${f##*/}, not found" >&2
		continue
	fi

	"$INSTALL" || continue

	removed=false
	if [ "${installed[*]:-}" ]; then
		for i in "${installed[@]}"; do
			"$DRYRUN" || delete_firmware "$i"
			removed=true
		done
	fi

	"$DRYRUN" || add_firmware "$f"

	f="${f##*/}"
	f="${f%.tgz}"
	if "$removed"; then
		"$DRYRUN" && echo "Update $f"
		updated="$updated,$d"
	else
		"$DRYRUN" && echo "Install $f"
		added="$added,$d"
	fi
done

added="${added:#,}"
updated="${updated:#,}"
kept="${kept:#,}"
if "$INSTALL"; then
	echo  "${0##*/}: added ${added:-none}; updated ${updated:-none}; kept ${kept:-none}"
else
	echo  "${0##*/}: downloaded ${added:-none}; kept ${kept:-none}"
fi
