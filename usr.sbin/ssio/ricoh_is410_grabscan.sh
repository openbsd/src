#!/bin/sh

# $Id: ricoh_is410_grabscan.sh,v 1.1 1997/03/11 03:23:16 kstailey Exp $
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

#
# ricoh_is410_grabscan:
# a wrapper for making PNM rawbits files from the Ricoh IS410 scanner data
#

if [ $# -gt 1 ]
then
	echo usage: $0 [\<logical name of scanner\>]		1>&2
	exit -1
fi

if [ $# = 0 ]
then
	scan_lname=scan0
else
	scan_lname=$1
fi

image_mode=`get_scanner -l $scan_lname | awk '/image_mode/ { print $3 }'`

set `get_scanner -p -l $scan_lname`
width=$1
height=$2

if [ $image_mode = grayscale ]
then
  echo P5
  echo $width $height
  echo 255
  dd if=/dev/$scan_lname bs=256k
else # monochrome
  echo P4
  echo $width $height
  dd if=/dev/$scan_lname bs=256k
fi
