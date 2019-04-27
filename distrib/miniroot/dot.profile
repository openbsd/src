#	$OpenBSD: dot.profile,v 1.44 2019/04/27 22:08:58 kn Exp $
#	$NetBSD: dot.profile,v 1.1 1995/12/18 22:54:43 pk Exp $
#
# Copyright (c) 2009 Kenneth R. Westerback
# Copyright (c) 1995 Jason R. Thorpe
# Copyright (c) 1994 Christopher G. Demetriou
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
# 3. All advertising materials mentioning features or use of this software
#    must display the following acknowledgement:
#	This product includes software developed by Christopher G. Demetriou.
# 4. The name of the author may not be used to endorse or promote products
#    derived from this software without specific prior written permission
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

export VNAME=$(sysctl -n kern.osrelease)
export VERSION="${VNAME%.*}${VNAME#*.}"
export ARCH=$(sysctl -n hw.machine)
export OBSD="OpenBSD/$ARCH $VNAME"
export PATH=/sbin:/bin:/usr/bin:/usr/sbin:/

umask 022

# emacs-style command line editing.
set -o emacs


if [[ -z $DONEPROFILE ]]; then
	DONEPROFILE=YES

	# Extract rootdisk from last 'root on ...' dmesg line.
	rootdisk=$(dmesg | sed -E '/^root on ([^ ]+) .*$/h;$!d;g;s//\1/')
	mount -u /dev/${rootdisk:-rd0a} /

	# Create a fake rc that just returns 1 and throws us back.
	echo ! : >/etc/rc

	# Start IPv6 stateless address autoconfiguration daemon.
	[[ -x /sbin/slaacd ]] && /sbin/slaacd

	# Set up some sane tty defaults.
	echo 'erase ^?, werase ^W, kill ^U, intr ^C, status ^T'
	stty newcrt werase ^W intr ^C kill ^U erase ^? status ^T

	cat <<__EOT

Welcome to the $OBSD installation program.
__EOT

	# Create working directories with proper permissions in /tmp.
	mkdir -m u=rwx,go=rx -p /tmp/{ai,i}

	# try unattended install
	/autoinstall -x

	# Set timer to automatically start unattended installation or upgrade
	# if netbooted or if a response file is found in / after a timeout,
	# but only the very first time around.
	timeout=false
	timer_pid=
	if [[ ! -f /tmp/ai/noai ]] && { ifconfig netboot >/dev/null 2>&1 ||
		[[ -f /auto_install.conf ]] ||
		[[ -f /auto_upgrade.conf ]]; }; then

		echo "Starting non-interactive mode in 5 seconds..."
		>/tmp/ai/noai

		# Set trap handlers to remove timer if the shell is interrupted,
		# killed or about to exit.
		trap 'kill $timer_pid 2>/dev/null' EXIT
		trap 'exit 1' INT
		trap 'timeout=true' TERM

		# Stop monitoring background processes to avoid printing job
		# completion notices in interactive shell mode. This doesn't
		# stop the "[1] <pid>" on starting a job though; that's why
		# stdout and stderr is redirected temporarily.
		set +m
		exec 3<&1 4<&2 >/dev/null 2>&1
		(sleep 5; kill $$) &
		timer_pid=$!
		exec 1<&3 2<&4 3<&- 4<&-
		set +m
	fi

	while :; do
		read REPLY?'(I)nstall, (U)pgrade, (A)utoinstall or (S)hell? '

		# Begin the automatic installation if the timeout has expired.
		if $timeout; then
			timeout=false
			echo
			REPLY=a
		else
			# User has made a choice; stop the read timeout.
			[[ -n $timer_pid ]] && kill $timer_pid 2>/dev/null
			timer_pid=
		fi

		case $REPLY in
		[aA]*)	/autoinstall && break;;
		[iI]*)	/install && break;;
		[uU]*)	/upgrade && break;;
		[sS]*)	break;;
		esac
	done
fi
