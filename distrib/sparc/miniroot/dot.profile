#	$OpenBSD: dot.profile,v 1.4 1997/04/30 23:56:07 grr Exp $
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

PATH=/sbin:/bin:/usr/bin:/usr/sbin:/
export PATH

TERM=sun
export TERM

umask 022

if [ "X${DONEPROFILE}" = "X" ]; then
	DONEPROFILE=YES

	# set up some sane defaults
	echo 'erase ^H, werase ^W, kill ^U, intr ^C'
	stty newcrt werase ^W intr ^C kill ^U erase ^H 9600

	# get the terminal type
	_forceloop=""
	while [ "X${_forceloop}" = X"" ]; do
		eval `tset -s -m ":?$TERM"`
		if [ "X${TERM}" != X"unknown" ]; then
			_forceloop="done"
		fi
	done
	export TERM

	# get the editor preference
	EDITOR=vi
	_forceloop=""
	while [ "X${_forceloop}" = X"" ]; do
		echo -n "text editor - vi or ed? [$EDITOR] "
		read _forceloop
		case "$_forceloop" in
			vi|"")
				EDITOR=vi
				_forceloop=$EDITOR
				;;

			ed)
				EDITOR=ed
				;;

			*)
				echo "sorry, no $_forceloop available"
				_forceloop=""
				;;
		esac
	done
	export EDITOR

	# Installing or upgrading?
	_forceloop=""
	while [ "X${_forceloop}" = X"" ]; do
		echo -n '(I)nstall or (U)pgrade? '
		read _forceloop
		case "$_forceloop" in
			i*|I*)
				/install
				;;

			u*|U*)
				/upgrade
				;;

			*)
				_forceloop=""
				;;
		esac
	done
fi
