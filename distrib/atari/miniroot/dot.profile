#	$NetBSD: dot.profile,v 1.1.1.1.4.2 1996/07/15 06:33:30 leo Exp $
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
export TERM=vt220

umask 022

set -o emacs # emacs-style command line editing

if [ "X${DONEPROFILE}" = "X" ]; then
	DONEPROFILE=YES

	# set up some sane defaults
	echo 'erase ^H, werase ^W, kill ^U, intr ^C'
	stty newcrt werase ^W intr ^C kill ^U erase ^H 9600

	# run update, so that installed software is written as it goes.
	update

	# Select a keyboard map
	_maps=`ls /usr/share/keymaps/atari | sed 's/\.map//g'`
	while [ ! -z "$_maps" ]; do
		echo "The available keyboard maps are:"
		_num=0
		for i in $_maps; do
			echo "	$_num  $i"
			_num=`expr $_num + 1`
		done
		echo
		echo -n "Select the number of the map you want to activate: "
		read _ans

		# Delete all non-nummeric characters from the users answer
		if [ ! -z "$_ans" ]; then
			_ans=`echo $_ans | sed 's/[^0-9]//g`
		fi

		# Check if the answer is valid (in range). Note that an answer
		# < 0 cannot happen because the sed(1) above also removes the
		# sign.
		if [ -z "$_ans" -o "$_ans" -ge $_num ]; then
		    echo "You entered an invalid response, please try again."
		    continue
		fi

		# Got a valid answer, activate the map...
		set -- $_maps
		shift $_ans
		/usr/sbin/loadkmap -f /usr/share/keymaps/atari/$1.map
		break
	done

	# Installing or upgrading?
	_forceloop=""
	while [ "X${_forceloop}" = X"" ]; do
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
