#!/bin/sh

# $Id: grabscan.sh,v 1.1 1997/03/11 03:23:14 kstailey Exp $
#
# Copyright (c) 1996 Kenneth Stailey
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
#	This product includes software developed for the NetBSD Project
#	by Kenneth Stailey
# 4. The name of the author may not be used to endorse or promote products
#    derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
# BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
# AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#

# top-level script to kick off scanning.

LANG=En_US
export LANG

if [ $# -ne 0 -a $# -ne 2 ]
then
	echo usage: grabscan '[ -l \<logical name of scanner\> ]'	>&2
	exit 1
fi

if [ $# -eq 2 ]
then
	scan_lname=$2
else
	scan_lname=scan0
fi

scan_type=`get_scanner -t`

if [ -z "$scan_type" ]
then
	echo grabscan: no scanner found.			>&2
	echo grabscan: terminating execution.			>&2
	exit 1
fi

case $scan_type in
  ricoh_is410 | ibm_2456)
	exec ricoh_is410_grabscan $scan_lname
  ;;

  fujitsu_m3069g)
	exec fujitsu_m3096g_grabscan $scan_lname
  ;;

  ricoh_fs1)
	exec ricoh_fs1_grabscan $scan_lname
  ;;

  sharp_jx600)
	exec sharp_jx600_grabscan $scan_lname
  ;;

  ricoh_is50)
	exec ricoh_is50_grabscan $scan_lname
  ;;

  umax_uc630 | umax_ug630)
	exec umax_uc630_grabscan $scan_lname
  ;;

  hp_scanjet_IIc)
	exec hp_scanjet_IIc_grabscan $scan_lname
  ;;

  *)
	echo grabscan: an unsupported scanner type was specified.	>&2
	echo grabscan: terminating execution.				>&2
	exit 1
  ;;
esac
