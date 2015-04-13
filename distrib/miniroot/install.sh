#!/bin/ksh
#	$OpenBSD: install.sh,v 1.265 2015/04/13 21:27:07 deraadt Exp $
#	$NetBSD: install.sh,v 1.5.2.8 1996/08/27 18:15:05 gwr Exp $
#
# Copyright (c) 1997-2015 Todd Miller, Theo de Raadt, Ken Westerback
# Copyright (c) 2015, Robert Peichaer <rpe@openbsd.org>
#
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# Copyright (c) 1996 The NetBSD Foundation, Inc.
# All rights reserved.
#
# This code is derived from software contributed to The NetBSD Foundation
# by Jason R. Thorpe.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
# ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
# TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#

#	OpenBSD installation script.

# install.sub needs to know the MODE.
MODE=install

# Include common subroutines and initialization code.
. install.sub

# Ask for/set the system hostname and add the hostname specific siteXX set. 
ask_until "System hostname? (short form, e.g. 'foo')" "$(hostname -s)"
[[ ${resp%%.*} != $(hostname -s) ]] && hostname $resp
THESETS="$THESETS site$VERSION-$(hostname -s).tgz"

echo

# Configure the network.
donetconfig

# If there's network connectivity, fetch list of mirror servers and installer
# choices from previous runs.
((NIFS != 0)) && startcgiinfo

echo
while :; do
	askpassword "Password for root account?"
	_rootpass="$_password"
	[[ -n "$_password" ]] && break
	echo "The root password must be set."
done

# Ask for the root user public ssh key during autoinstall.
rootkey=
$AUTO && ask "Public ssh key for root account?" none &&
	[[ $resp != none ]] && rootkey=$resp

# Ask user about daemon startup on boot, X Window usage and console setup.
questions

# Gather information for setting up the initial user account.
user_setup

# Set TZ variable based on zonefile and user selection.
set_timezone /var/tzlist
echo

# Get information about ROOTDISK, etc.
get_rootinfo

DISK=
DISKS_DONE=
_DKDEVS=$(get_dkdevs)
_fsent=

# Remove traces of previous install attempt.
rm -f /tmp/fstab*

ask_yn "Use DUIDs rather than device names in fstab?" yes && FSTABFLAG=-F

# Configure the disk(s).
while :; do
	DISKS_DONE=$(addel "$DISK" $DISKS_DONE)
	_DKDEVS=$(rmel "$DISK" $_DKDEVS)

	# Always do ROOTDISK first, and repeat until it is configured.
	if isin $ROOTDISK $_DKDEVS; then
		resp=$ROOTDISK
		rm -f /tmp/fstab
	else
		# Force the user to think and type in a disk name by
		# making 'done' the default choice.
		ask_which "disk" "do you wish to initialize" \
			'$(l=$(get_dkdevs); for a in $DISKS_DONE; do
				l=$(rmel $a $l); done; bsort $l)' \
			done
		[[ $resp == done ]] && break
	fi

	DISK=$resp
	makedev $DISK || continue

	# Deal with disklabels, including editing the root disklabel
	# and labeling additional disks. This is machine-dependent since
	# some platforms may not be able to provide this functionality.
	# /tmp/fstab.$DISK is created here with 'disklabel -f' or
	# 'disklabel -F' depending on the value of $FSTABFLAG.
	rm -f /tmp/*.$DISK
	md_prep_disklabel $DISK || { DISK=; continue; }

	# Make sure there is a '/' mount point.
	grep -qs " / ffs " /tmp/fstab.$ROOTDISK ||
		{ DISK=; echo "'/' must be configured!"; continue; }

	if [[ -f /tmp/fstab.$DISK ]]; then
		# Avoid duplicate mount points on different disks.
		while read _pp _mp _rest; do
			if [[ $_mp == none ]]; then
				# Multiple swap partitions are ok.
				echo "$_pp $_mp $_rest" >>/tmp/fstab
				continue
			fi
			# Non-swap mountpoints must be in only one file.
			[[ /tmp/fstab.$DISK == $(grep -l " $_mp " /tmp/fstab.*) ]] ||
				{ _rest=$DISK; DISK=; break; }
		done </tmp/fstab.$DISK
		if [[ -z $DISK ]]; then
			# Duplicate mountpoint.
			# Allow disklabel(8) to read back mountpoint info
			# if it is immediately run against the same disk.
			cat /tmp/fstab.$_rest >/etc/fstab
			rm /tmp/fstab.$_rest
			set -- $(grep -h " $_mp " /tmp/fstab.*[0-9])
			echo "$_pp and $1 can't both be mounted at $_mp."
			continue
		fi

		# Add ffs filesystems to list after newfs'ing them. Ignore
		# other filesystems.
		while read _pp _mp _fstype _rest; do
			[[ $_fstype == ffs ]] || continue
			_OPT=
			[[ $_mp == / ]] && _OPT=$MDROOTFSOPT
			newfs -q $_OPT ${_pp##/dev/}
			# N.B.: '!' is lexically < '/'. That is
			#	required for correct sorting of
			#	mount points.
			_fsent="$_fsent $_mp!$_pp"
		done </tmp/fstab.$DISK
	fi
done

# Write fstab entries to fstab in mount point alphabetic order
# to enforce a rational mount order.
for _mp in $(bsort $_fsent); do
	_pp=${_mp##*!}
	_mp=${_mp%!*}
	echo -n "$_pp $_mp ffs rw"

	# Only '/' is neither nodev nor nosuid. i.e. it can obviously
	# *always* contain devices or setuid programs.
	[[ $_mp == / ]] && { echo " 1 1"; continue; }

	# Every other mounted filesystem is nodev. If the user chooses
	# to mount /dev as a separate filesystem, then on the user's
	# head be it.
	echo -n ",nodev"

	# The only directories that the install puts suid binaries into
	# (as of 3.2) are:
	#
	# /sbin
	# /usr/bin
	# /usr/sbin
	# /usr/libexec
	# /usr/libexec/auth
	# /usr/X11R6/bin
	#
	# and ports and users can do who knows what to /usr/local and
	# sub directories thereof.
	#
	# So try to ensure that only filesystems that are mounted at
	# or above these directories can contain suid programs. In the
	# case of /usr/libexec, give blanket permission for
	# subdirectories.
	case $_mp in
	/sbin|/usr)			;;
	/usr/bin|/usr/sbin)		;;
	/usr/libexec|/usr/libexec/*)	;;
	/usr/local|/usr/local/*)	;;
	/usr/X11R6|/usr/X11R6/bin)	;;
	*)	echo -n ",nosuid"	;;
	esac
	echo " 1 2"
done >>/tmp/fstab

# Create a skeletal /etc/fstab which is usable for the installation process.
munge_fstab

# Use async options for faster mounts of the filesystems.
mount_fs "-o async"

# Feed the random pool some entropy before we read from it.
feed_random

# Ask the user for locations, and install whatever sets the user selected.
install_sets

# If we did not succeed at setting TZ yet, we try again
# using the timezone names extracted from the base set.
if [[ -z $TZ ]]; then
	(cd /mnt/usr/share/zoneinfo
		ls -1dF $(tar cvf /dev/null [A-Za-y]*) >/mnt/tmp/tzlist )
	echo
	set_timezone /mnt/tmp/tzlist
	rm -f /mnt/tmp/tzlist
fi

# If we got a timestamp from the cgi server, and that time diffs by more
# than 120 seconds, ask if the user wants to adjust the time.
if _time=$(http_time) && _now=$(date +%s) &&
	(( _now - _time > 120 || _time - _now > 120 )); then
	_tz=/mnt/usr/share/zoneinfo/$TZ
	if ask_yn "Time appears wrong.  Set to '$(TZ=$_tz date -r "$(http_time)")'?" yes; then
		# We do not need to specify TZ below since both date
		# invocations use the same one.
		date $(date -r "$(http_time)" "+%Y%m%d%H%M.%S") >/dev/null
		# N.B. This will screw up SECONDS.
	fi
fi

# If we managed to talk to the cgi server before, tell it what
# location we used... so it can perform magic next time.
if [[ -s $HTTP_LIST ]]; then
	_i=
	[[ -n $INSTALL ]] && _i="install=$INSTALL"
	[[ -n $TZ ]] && _i="$_i&TZ=$TZ"
	[[ -n $METHOD ]] && _i="$_i&method=$METHOD"

	[[ -n $_i ]] && ftp -Vao - \
		"http://129.128.5.191/cgi-bin/ftpinstall.cgi?$_i" >/dev/null 2>&1 &
fi

# Ensure an enabled console has the correct speed in /etc/ttys.
sed "/^console.*on.*secure.*$/s/std\.[0-9]*/std.$(stty speed </dev/console)/" \
	/mnt/etc/ttys >/tmp/ttys
mv /tmp/ttys /mnt/etc/ttys

echo -n "Saving configuration files..."

# Save any leases obtained during install.
(cd /var/db; for _f in dhclient.leases.*; do
	[[ -f $_f ]] && mv $_f /mnt/var/db/.
done)

# Move configuration files from /tmp to /mnt/etc.
hostname >/tmp/myname

# Append entries to installed hosts file, changing '1.2.3.4 hostname'
# to '1.2.3.4 hostname.$FQDN hostname'. Leave untouched lines containing
# domain information or aliases. These are lines the user added/changed
# manually.
# Add common entries.
echo "127.0.0.1\tlocalhost" >/mnt/etc/hosts
echo "::1\t\tlocalhost" >>/mnt/etc/hosts
# Note we may have no hosts file if no interfaces were configured.
if [[ -f /tmp/hosts ]]; then
	_dn=$(get_fqdn)
	while read _addr _hn _aliases; do
		if [[ -n $_aliases || $_hn != ${_hn%%.*} || -z $_dn ]]; then
			echo "$_addr\t$_hn $_aliases"
		else
			echo "$_addr\t$_hn.$_dn $_hn"
		fi
	done </tmp/hosts >>/mnt/etc/hosts
	rm /tmp/hosts
fi

# Append dhclient.conf to installed dhclient.conf.
_f=dhclient.conf
[[ -f /tmp/$_f ]] && { cat /tmp/$_f >>/mnt/etc/$_f; rm /tmp/$_f; }

# Possible files to copy from /tmp: fstab hostname.* kbdtype mygate
#     myname ttys boot.conf resolv.conf sysctl.conf resolv.conf.tail
# Save only non-empty (-s) regular (-f) files.
(cd /tmp; for _f in fstab hostname* kbdtype my* ttys *.conf *.tail; do
	[[ -f $_f && -s $_f ]] && mv $_f /mnt/etc/.
done)

echo "done."

# Apply configuration settings based on information from questions().
apply

# Create user account based on information from user_setup().
if [[ -n $user ]]; then
	_encr=$(encr_pwd "$userpass")
	_home=/home/$user
	uline="${user}:${_encr}:1000:1000:staff:0:0:${username}:$_home:/bin/ksh"
	echo "$uline" >> /mnt/etc/master.passwd
	echo "${user}:*:1000:" >> /mnt/etc/group
	echo ${user} > /mnt/root/.forward

	_home=/mnt$_home
	mkdir -p $_home
	(cd /mnt/etc/skel; cp -pR . $_home)
	(umask 077 &&
		sed "s,^To: root\$,To: ${username} <${user}>," \
		/mnt/var/mail/root > /mnt/var/mail/$user )
	chown -R 1000:1000 $_home /mnt/var/mail/$user
	echo "1,s@wheel:.:0:root\$@wheel:\*:0:root,${user}@
w
q" | ed /mnt/etc/group 2>/dev/null

	# During autoinstall, add public ssh key to authorized_keys.
	[[ -n "$userkey" ]] &&
		print -r -- "$userkey" >> $_home/.ssh/authorized_keys
fi

# Store root password and rebuild password database.
if [[ -n "$_rootpass" ]]; then
	_encr=$(encr_pwd "$_rootpass")
	echo "1,s@^root::@root:${_encr}:@
w
q" | ed /mnt/etc/master.passwd 2>/dev/null
fi
pwd_mkdb -p -d /mnt/etc /etc/master.passwd

# During autoinstall, add root user's public ssh key to authorized_keys.
[[ -n "$rootkey" ]] && (
	umask 077
	mkdir /mnt/root/.ssh
	print -r -- "$rootkey" >> /mnt/root/.ssh/authorized_keys
)

# Perform final steps common to both an install and an upgrade.
finish_up
