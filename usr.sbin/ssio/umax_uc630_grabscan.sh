#!/bin/sh

# $Id: umax_uc630_grabscan.sh,v 1.2 2002/06/03 09:05:59 deraadt Exp $
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
# umax_uc630_grabscan:
# a wrapper for making PNM rawbits files from the UMAX UC630 scanner data
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

red_tempfile=`mktemp -t grabscan.red.XXXXXXXXXX` || exit 1
green_tempfile=`mktemp -t grabscan.green.XXXXXXXXXX`
if [ $? -ne 0 ]; then
	rm -f $red_tempfile
	exit 1
fi
blue_tempfile=`mktemp -t grabscan.blue.XXXXXXXXXX`
if [ $? -ne 0 ]; then
	rm -f $red_tempfile $green_tempfile
	exit 1
fi

case $image_mode in

grayscale)
  echo P5
  echo $width $height
  echo 255
  dd if=/dev/$scan_lname bs=256k
;;

binary_monochrome|dithered_monochrome)
  echo P4
  echo $width $height
  dd if=/dev/$scan_lname bs=256k
;;

red|green|blue)

  save_modes=`get_scanner -q`

  echo grabbing colors: >&2

# red pass...
  echo red... >&2
  set_scanner -i R >/dev/null
  (echo P5
   echo $width $height
   echo 255
   dd if=/dev/scan0 bs=256k) > $red_tempfile

# green pass...
  echo green... >&2
  set_scanner -i G >/dev/null
  (echo P5
   echo $width $height
   echo 255
   dd if=/dev/scan0 bs=256k) > $green_tempfile

# blue pass...
  echo blue... >&2
  set_scanner -i B >/dev/null
  (echo P5
   echo $width $height
   echo 255
   dd if=/dev/scan0 bs=256k) > $blue_tempfile

  echo mixing colors together... >&2
  rgb3toppm $red_tempfile $green_tempfile $blue_tempfile

  rm $red_tempfile $green_tempfile $blue_tempfile

# restore scanner
  set_scanner $save_modes >/dev/null

;;

esac

echo done >&2
