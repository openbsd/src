#	$OpenBSD: dot.profile,v 1.4 2000/01/30 01:21:18 deraadt Exp $
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
alias dmesg="cat /kern/msgbuf"

# XXX
# the TERM/EDITOR stuff is really well enough parameterized to be moved
# into install.sub where it could use the routines there and be invoked
# from the various (semi) MI install and upgrade scripts

# terminals believed to be in termcap, default TERM
TERMS="sun vt* pcvt* pc3 dumb"
TERM=sun

# editors believed to be in $EDITBIN, smart and dumb defaults
EDITORS="vi ed"
EDITOR=vi
DUMB=ed
EDITBIN=/bin

if [ "X${DONEPROFILE}" = "X" ]; then
	DONEPROFILE=YES

	# mount kernfs and re-mount the boot media (perhaps r/w)
	mount_kernfs /kern /kern
	mount_ffs -o update /kern/rootdev /

	# set up some sane defaults
	echo 'erase ^H, werase ^W, kill ^U, intr ^C'
	stty newcrt werase ^W intr ^C kill ^U erase ^H 9600

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

	# get the editor preference
	if [ "X$TERM" = "Xdumb" -o "X$TERM" = "Xunknown" ]; then
		echo -n "$TERM can't handle $EDITOR"
		EDITOR="$DUMB"
		echo ", using $EDITOR as text editor!"
	elif [ "X$EDITOR" = "X$EDITORS" ]; then
		echo "Only one editor available, you get to use $EDITOR!"
	else
		_forceloop=""
		while [ "X$_forceloop" = X"" ]; do
			echo "Supported editors are: $EDITORS"
			echo -n "text editor? [$EDITOR] "
			read _choice
			if [ "X$_choice" = "X" ]; then
				_choice="$EDITOR"
				_forceloop="$_choice"
			else
				for _editor in $EDITORS; do
					if [ "X$_choice" = "X$_editor" ]; then
						_forceloop="$_choice"
						break
					fi
				done
			fi
			if [ "X$_forceloop" != "X" -a ! -x $EDITBIN/$_choice ]
			then
				_forceloop=""
			fi
			if [ "X$_forceloop" = "X" ]; then
				echo "Sorry, $_choice isn't available."
				_forceloop=""
			fi
		done
		EDITOR="$_choice"
	fi
	export EDITOR

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
