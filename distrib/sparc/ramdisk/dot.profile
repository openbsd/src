#	$OpenBSD: dot.profile,v 1.7 1998/10/15 07:33:32 deraadt Exp $
#	$NetBSD: dot.profile,v 1.1 1995/12/18 22:54:43 pk Exp $
#
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

export PATH=/sbin:/bin:/usr/bin:/usr/sbin:/
export HISTFILE=/.sh_history

umask 022

set -o emacs # emacs-style command line editing

# XXX
# the TERM/EDITOR stuff is really well enough parameterized to be moved
# into install.sub where it could use the routines there and be invoked
# from the various (semi) MI install and upgrade scripts

# terminals believed to be in termcap, default TERM
TERMS="sun vt* pcvt* pc3 dumb"
TERM=sun

if [ "X${DONEPROFILE}" = "X" ]; then
	DONEPROFILE=YES

	# mount kernfs and re-mount the boot media (perhaps r/w)
	mount_kernfs /kern /kern
	mount_ffs -o update /dev/rd0a /

	# set up some sane defaults
	echo 'erase ^?, werase ^W, kill ^U, intr ^C'
	stty newcrt werase ^W intr ^C kill ^U erase ^? 9600

	# get the terminal type
	_forceloop=""
	while [ "X$_forceloop" = X"" ]; do
		echo "Supported terminals are: $TERMS"
		eval `tset -s -m ":?$TERM"`
		if [ "X$TERM" != X"unknown" ]; then
			_forceloop="done"
		fi
	done
	export TERM

	# Installing or upgrading?
	_forceloop=""
	while [ "X$_forceloop" = X"" ]; do
		echo -n '(I)nstall, (U)pgrade, or (S)hell? '
		read _forceloop
		case "$_forceloop" in
			i*|I*)
				/install
				;;

			u*|U*)
				/upgrade
				;;

			s*|S*)
				;;

			*)
				_forceloop=""
				;;
		esac
	done
fi
