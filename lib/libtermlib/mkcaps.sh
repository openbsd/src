#! /bin/sh
#
#	$OpenBSD: mkcaps.sh,v 1.1.1.1 1996/05/31 05:40:02 tholo Exp $
#
# Copyright (c) 1996 SigmaSoft, Th. Lockert <tholo@sigmasoft.com>
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
#	This product includes software developed by SigmaSoft, Th.  Lockert.
# 4. The name of the author may not be used to endorse or promote products
#    derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
# INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
# AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
# THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

cat $1
awk -- '
$1 == "bool" {
		printf "#define\t%-30s\t(cur_term->bools[%d])\n", $2, boolcnt++
	}
$1 == "num" {
		printf "#define\t%-30s\t(cur_term->nums[%d])\n", $2, numcnt++
	}
$1 == "str" {
		printf "#define\t%-30s\t(cur_term->strs[%d])\n", $2, strcnt++
	}

END {
		printf "\n#define\t_tBoolCnt\t%d\n", boolcnt
		printf "#define\t_tNumCnt\t%d\n", numcnt
		printf "#define\t_tStrCnt\t%d\n", strcnt
	}
' < $2
cat $3

exit 0
