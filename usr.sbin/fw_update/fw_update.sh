#!/bin/ksh
#	$OpenBSD: fw_update.sh,v 1.43 2022/08/05 18:01:40 afresh1 Exp $
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
[ "$VTYPE" = -current ] && HTTP_FWDIR=snapshots

FWURL=http://firmware.openbsd.org/firmware/${HTTP_FWDIR}
FWPUB_KEY=${DESTDIR}/etc/signify/openbsd-${VERSION}-fw.pub

DRYRUN=false
integer VERBOSE=0
DELETE=false
DOWNLOAD=true
INSTALL=true
LOCALSRC=

unset FTPPID
unset LOCKPID
unset FWPKGTMP
REMOVE_LOCALSRC=false
cleanup() {
	set +o errexit # ignore errors from killing ftp
	[ "${FTPPID:-}" ] && kill -TERM -"$FTPPID" 2>/dev/null
	[ "${LOCKPID:-}" ] && kill -TERM -"$LOCKPID" 2>/dev/null
	[ "${FWPKGTMP:-}" ] && rm -rf "$FWPKGTMP"
	"$REMOVE_LOCALSRC" && rm -rf "$LOCALSRC"
	[ -e "${CFILE}" ] && [ ! -s "$CFILE" ] && rm -f "$CFILE"
}
trap cleanup EXIT

tmpdir() {
	local _i=1 _dir

	# The installer lacks mktemp(1), do it by hand
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

	# The installer uses a limited doas(1) as a tiny su(1)
	set -o monitor # make sure ftp gets its own process group
	(
	_flags=-vm
	case "$VERBOSE" in
		0|1) _flags=-VM ;;
		  2) _flags=-Vm ;;
	esac
	if [ -x /usr/bin/su ]; then
		exec /usr/bin/su -s /bin/ksh "$_user" -c \
		    "/usr/bin/ftp -N '${0##/}' -D 'Get/Verify' $_flags -o- '$_src'" > "$_dst"
	else
		exec /usr/bin/doas -u "$_user" \
		    /usr/bin/ftp -N "${0##/}" -D 'Get/Verify' $_flags -o- "$_src" > "$_dst"
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
				kill -INT -"$FTPPID" 2>/dev/null
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

	return 0
}

# If we fail to fetch the CFILE, we don't want to try again
# but we might be doing this in a subshell so write out
# a blank file indicating failure.
check_cfile() {
	if [ -e "$CFILE" ]; then
		[ -s "$CFILE" ] || return 1
		return 0
	fi
	if ! fetch_cfile "$@"; then
		echo -n > "$CFILE"
		return 1
	fi
	return 0
}

fetch_cfile() {
	if "$DOWNLOAD"; then
		set +o noclobber # we want to get the latest CFILE
		fetch "$CFILE" || return 1
		set -o noclobber
		! signify -qVep "$FWPUB_KEY" -x "$CFILE" -m "$CFILE" &&
		    echo "Signature check of SHA256.sig failed" >&2 &&
		    rm -f "$CFILE" && return 1
	elif [ ! -e "$CFILE" ]; then
		echo "${0##*/}: $CFILE: No such file or directory" >&2
		return 1
	fi

	return 0
}

verify() {
	check_cfile || return 1
	# The installer sha256 lacks -C, do it by hand
	if ! fgrep -qx "SHA256 (${1##*/}) = $( /bin/sha256 -qb "$1" )" "$CFILE"; then
		((VERBOSE != 1)) && echo "Checksum test for ${1##*/} failed." >&2
		return 1
	fi

	return 0
}

firmware_in_dmesg() {
	local IFS
	local _d _m _dmesgtail _last='' _nl='
'

	# The dmesg can contain multiple boots, only look in the last one
	_dmesgtail="$( echo ; sed -n 'H;/^OpenBSD/h;${g;p;}' /var/run/dmesg.boot )"

	grep -v '^[[:space:]]*#' "$FWPATTERNS" |
	    while read -r _d _m; do
		[ "$_d" = "$_last" ]  && continue
		[ "$_m" ]             || _m="${_nl}${_d}[0-9] at "
		[ "$_m" = "${_m#^}" ] || _m="${_nl}${_m#^}"

		IFS='*'
		set -- $_m
		unset IFS

		case $# in
		    1|2|3) [[ $_dmesgtail = *$1*([!$_nl])${2-}*([!$_nl])${3-}* ]] || continue;;
		    *) echo "${0##*/}: Bad pattern '${_m#$_nl}' in $FWPATTERNS" >&2; exit 1 ;;
		esac

		echo "$_d"
		_last="$_d"
	    done
}

firmware_filename() {
	check_cfile || return 1
	sed -n "s/.*(\($1-firmware-.*\.tgz\)).*/\1/p" "$CFILE" | sed '$!d'
}

firmware_devicename() {
	local _d="${1##*/}"
	_d="${_d%-firmware-*}"
	echo "$_d"
}

lock_db() {
	[ "${LOCKPID:-}" ] && return 0

	# The installer doesn't have perl, so we can't lock there
	[ -e /usr/bin/perl ] || return 0

	set -o monitor
	perl <<'EOL' |&
		use v5.16;
		use warnings;
		no lib ('/usr/local/libdata/perl5/site_perl');
		use OpenBSD::PackageInfo qw< lock_db >;

		$|=1;

		lock_db(0);
	
		say $$;
		sleep;
EOL
	set +o monitor

	read -rp LOCKPID

	return 0
}

installed_firmware() {
	local _pre="$1" _match="$2" _post="$3" _firmware _fw
	set -sA _firmware -- $(
	    set +o noglob
	    grep -Fxl '@option firmware' \
		"${DESTDIR}/var/db/pkg/"$_pre"$_match"$_post"/+CONTENTS" \
		2>/dev/null || true
	    set -o noglob
	)

	[ "${_firmware[*]:-}" ] || return 0
	for _fw in "${_firmware[@]}"; do
		_fw="${_fw%/+CONTENTS}"
		echo "${_fw##*/}"
	done
}

detect_firmware() {
	local _devices _last='' _d

	set -sA _devices -- $(
	    firmware_in_dmesg
	    for _d in $( installed_firmware '*' '-firmware-' '*' ); do
		firmware_devicename "$_d"
	    done
	)

	[ "${_devices[*]:-}" ] || return 0
	for _d in "${_devices[@]}"; do
		[ "$_last" = "$_d" ] && continue
		echo "$_d"
		_last="$_d"
	done
}

add_firmware () {
	local _f="${1##*/}" _m="${2:-Install}" _pkgname
	FWPKGTMP="$( tmpdir "${DESTDIR}/var/db/pkg/.firmware" )"
	local _flags=-vm
	case "$VERBOSE" in
		0|1) _flags=-VM ;;
		2|3) _flags=-Vm ;;
	esac

	ftp -N "${0##/}" -D "$_m" "$_flags" -o- "file:${1}" |
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

remove_files() {
	local _r
	# Use rm -f, not removing files/dirs is probably not worth failing over
	for _r in "$@" ; do
		if [ -d "$_r" ]; then
			# The installer lacks rmdir,
			# but we only want to remove empty directories.
			set +o noglob
			[ "$_r/*" = "$( echo "$_r"/* )" ] && rm -rf "$_r"
			set -o noglob
		else
			rm -f "$_r"
		fi
	done
}

delete_firmware() {
	local _cwd _pkg="$1" _pkgdir="${DESTDIR}/var/db/pkg"

	# TODO: Check hash for files before deleting
	((VERBOSE > 2)) && echo -n "Uninstall $_pkg ..."
	_cwd="${_pkgdir}/$_pkg"

	if [ ! -e "$_cwd/+CONTENTS" ] ||
	    ! grep -Fxq '@option firmware' "$_cwd/+CONTENTS"; then
		echo "${0##*/}: $_pkg does not appear to be firmware" >&2
		return 2
	fi

	set -A _remove -- "${_cwd}/+CONTENTS" "${_cwd}"

	while read -r _c _g; do
		case $_c in
		@cwd) _cwd="${DESTDIR}$_g"
		  ;;
		@*) continue
		  ;;
		*) set -A _remove -- "$_cwd/$_c" "${_remove[@]}"
		  ;;
		esac
	done < "${_pkgdir}/${_pkg}/+CONTENTS"

	remove_files "${_remove[@]}"

	((VERBOSE > 2)) && echo " done."

	return 0
}

unregister_firmware() {
	local _d="$1" _pkgdir="${DESTDIR}/var/db/pkg" _fw

	set -A installed -- $( installed_firmware '' "$d-firmware-" '*' )
	if [ "${installed:-}" ]; then
		for _fw in "${installed[@]}"; do
			((VERBOSE)) && echo "Unregister $_fw"
			"$DRYRUN" && continue
			remove_files \
			    "$_pkgdir/$_fw/+CONTENTS" \
			    "$_pkgdir/$_fw/+DESC" \
			    "$_pkgdir/$_fw/"
		done
		return 0
	fi

	return 1
}

usage() {
	echo "usage: ${0##*/} [-adFnv] [-p path] [driver | file ...]"
	exit 1
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
	v) ((++VERBOSE)) ;;
	:)
	    echo "${0##*/}: option requires an argument -- -$OPTARG" >&2
	    usage
	    ;;
	?)
	    echo "${0##*/}: unknown option -- -$OPTARG" >&2
	    usage
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
		    exit 1
	fi
fi

# "Download only" means local dir and don't install
if [ "$OPT_F" ]; then
	INSTALL=false
	LOCALSRC="${LOCALSRC:-.}"

	# Always check for latest CFILE and so latest firmware
	if [ -e "$LOCALSRC/$CFILE" ]; then
		mv "$LOCALSRC/$CFILE" "$LOCALSRC/$CFILE-OLD"
		if check_cfile; then
			rm -f "$LOCALSRC/$CFILE-OLD"
		else
			mv "$LOCALSRC/$CFILE-OLD" "$LOCALSRC/$CFILE"
			echo "Using existing $CFILE" >&2
		fi
	fi
elif [ "$LOCALSRC" ]; then
	DOWNLOAD=false
fi

if [ -x /usr/bin/id ] && [ "$(/usr/bin/id -u)" != 0 ]; then
	echo "need root privileges" >&2
	exit 1
fi

set -sA devices -- "$@"

if "$DELETE"; then
	[ "$OPT_F" ] && echo "Cannot use -F and -d" >&2 && usage
	lock_db

	# Show the "Uninstall" message when just deleting not upgrading
	((VERBOSE)) && VERBOSE=3

	set -A installed
	if [ "${devices[*]:-}" ]; then
		"$ALL" && echo "Cannot use -a and devices/files" >&2 && usage

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
				((VERBOSE)) && echo "Delete $fw"
			else
				delete_firmware "$fw" || continue
			fi
			deleted="$deleted,$( firmware_devicename "$fw" )"
		done
	fi

	deleted="${deleted#,}"
	echo "${0:##*/}: deleted ${deleted:-none}";

	exit
fi

if [ ! "$LOCALSRC" ]; then
	LOCALSRC="$( tmpdir "${DESTDIR}/tmp/${0##*/}" )"
	REMOVE_LOCALSRC=true
fi

CFILE="$LOCALSRC/$CFILE"

if [ "${devices[*]:-}" ]; then
	"$ALL" && echo "Cannot use -a and devices/files" >&2 && usage
else
	((VERBOSE > 1)) && echo -n "Detect firmware ..."
	set -sA devices -- $( detect_firmware )
	((VERBOSE > 1)) &&
	    { [ "${devices[*]:-}" ] && echo " found." || echo " done." ; }
fi


added=''
updated=''
kept=''
unregister=''
if [ "${devices[*]:-}" ]; then
	lock_db
	for f in "${devices[@]}"; do
		d="$( firmware_devicename "$f" )"

		verify_existing=true
		if [ "$f" = "$d" ]; then
			f=$( firmware_filename "$d" ) || continue
			if [ ! "$f" ]; then
				if "$INSTALL" && unregister_firmware "$d"; then
					unregister="$unregister,$d"
				else
					echo "Unable to find firmware for $d" >&2
				fi
				continue
			fi
			f="$LOCALSRC/$f"
		elif ! "$INSTALL" && ! grep -Fq "($f)" "$CFILE" ; then
			echo "Cannot download local file $f" >&2
			exit 1
		else
			# Don't verify files specified on the command-line
			verify_existing=false
		fi

		set -A installed -- $( installed_firmware '' "$d-firmware-" '*' )

		if "$INSTALL" && [ "${installed[*]:-}" ]; then
			for i in "${installed[@]}"; do
				if [ "${f##*/}" = "$i.tgz" ]; then
					((VERBOSE > 2)) && echo "Keep $i"
					kept="$kept,$d"
					continue 2
				fi
			done
		fi

		pending_status=false
		if "$verify_existing" && [ -e "$f" ]; then
			if ((VERBOSE == 1)); then
				echo -n "Verify ${f##*/} ..."
				pending_status=true
			elif ((VERBOSE > 1)) && ! "$INSTALL"; then
			    echo "Keep/Verify ${f##*/}"
			fi

			if "$DRYRUN" || verify "$f"; then
				"$INSTALL" || kept="$kept,$d"
			elif "$DOWNLOAD"; then
				((VERBOSE == 1)) && echo " failed."
				((VERBOSE > 1)) && echo "Refetching $f"
				rm -f "$f"
			else
				"$pending_status" && echo " failed."
				continue
			fi
		fi

		if [ -e "$f" ]; then
			"$pending_status" && ! "$INSTALL" && echo " done."
		elif "$DOWNLOAD"; then
			if "$DRYRUN"; then
				((VERBOSE)) && echo "Get/Verify ${f##*/}"
			else
				if ((VERBOSE == 1)); then
					echo -n "Get/Verify ${f##*/} ..."
					pending_status=true
				fi
				fetch  "$f" &&
				verify "$f" || {
					"$pending_status" && echo " failed."
					continue
				}
				"$pending_status" && ! "$INSTALL" && echo " done."
			fi
			"$INSTALL" || added="$added,$d"
		elif "$INSTALL"; then
			echo "Cannot install ${f##*/}, not found" >&2
			continue
		fi

		"$INSTALL" || continue

		update="Install"
		if [ "${installed[*]:-}" ]; then
			update="Update"
			for i in "${installed[@]}"; do
				"$DRYRUN" || delete_firmware "$i"
			done
		fi

		if "$DRYRUN"; then
			((VERBOSE)) && echo "$update $f"
		else
			if ((VERBOSE == 1)) && ! "$pending_status"; then
				echo -n "Install ${f##*/} ..."
				pending_status=true
			fi
			add_firmware "$f" "$update"
		fi

		f="${f##*/}"
		f="${f%.tgz}"
		if [ "$update" = Install ]; then
			"$pending_status" && echo " installed."
			added="$added,$d"
		else
			"$pending_status" && echo " updated."
			updated="$updated,$d"
		fi
	done
fi

added="${added:#,}"
updated="${updated:#,}"
kept="${kept:#,}"
[ "${unregister:-}" ] && unregister="; unregistered ${unregister:#,}"
if "$INSTALL"; then
	echo  "${0##*/}: added ${added:-none}; updated ${updated:-none}; kept ${kept:-none}${unregister}"
else
	echo  "${0##*/}: downloaded ${added:-none}; kept ${kept:-none}${unregister}"
fi
