#! /usr/bin/awk -f
#	$OpenBSD: makemap.awk,v 1.1 2003/02/16 01:42:49 miod Exp $
#
# Copyright (c) 2003, Miodrag Vallat.
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

#
# This script attempts to convert, with minimal hacks and losses, the
# regular PS/2 keyboard (pckbd) layout tables into GSC keyboard (gsckbd)
# layout tables, as almost all scancodes are completely different in the
# GSC world.
#

BEGIN {
	mapnum = 0

	# PS/2 id -> GSCKBD conversion table, or "sanity lossage 101"
	for (i = 0; i < 256; i++)
		conv[i] = -1

	conv[1] = 118
	conv[2] = 22
	conv[3] = 30
	conv[4] = 38
	conv[5] = 37
	conv[6] = 46
	conv[7] = 54
	conv[8] = 61
	conv[9] = 62
	conv[10] = 70
	conv[11] = 69
	conv[12] = 78
	conv[13] = 85
	conv[14] = 102
	conv[15] = 13
	conv[16] = 21
	conv[17] = 29
	conv[18] = 36
	conv[19] = 45
	conv[20] = 44
	conv[21] = 53
	conv[22] = 60
	conv[23] = 67
	conv[24] = 68
	conv[25] = 77
	conv[26] = 84
	conv[27] = 91
	conv[28] = 90
	conv[29] = 20
	conv[30] = 28
	conv[31] = 27
	conv[32] = 35
	conv[33] = 43
	conv[34] = 52
	conv[35] = 51
	conv[36] = 59
	conv[37] = 66
	conv[38] = 75
	conv[39] = 76
	conv[40] = 82
	conv[41] = 14
	conv[42] = 18
	conv[43] = 93
	conv[44] = 26
	conv[45] = 34
	conv[46] = 33
	conv[47] = 42
	conv[48] = 50
	conv[49] = 49
	conv[50] = 58
	conv[51] = 65
	conv[52] = 73
	conv[53] = 74
	conv[54] = 89
	conv[55] = 124
	conv[56] = 17
	conv[57] = 41
	conv[58] = 88
	conv[59] = 5
	conv[60] = 6
	conv[61] = 4
	conv[62] = 12
	conv[63] = 3
	conv[64] = 11
	conv[65] = 131
	conv[66] = 10
	conv[67] = 1
	conv[68] = 9
	conv[69] = 119
	conv[70] = 126
	conv[71] = 108
	conv[72] = 117
	conv[73] = 125
	conv[74] = 123
	conv[75] = 107
	conv[76] = 115
	conv[77] = 116
	conv[78] = 121
	conv[79] = 105
	conv[80] = 114
	conv[81] = 122
	conv[82] = 112
	conv[83] = 113
	conv[86] = 97
	conv[87] = 120
	conv[88] = 7
	# 112 used by jp
	# 115 used by jp and br
	# 121 used by jp
	# 123 used by jp
	# 125 used by jp
	conv[127] = 127
	conv[156] = 218
	conv[157] = 148
	# Print Screen produces E0 12 E0 7C when pressed, then E0 7C E0 12
	# when released.  Ignore the E0 12 code and match only on E0 7C
	conv[170] = 252
	conv[181] = 202
	conv[184] = 145
	conv[198] = 254
	conv[199] = 236
	conv[200] = 245
	conv[201] = 253
	conv[203] = 235
	conv[205] = 244
	conv[207] = 233
	conv[208] = 242
	conv[209] = 250
	conv[210] = 240
	conv[211] = 113
}
NR == 1 {
	VERSION = $0
	gsub("\\$", "", VERSION)

	printf("/*\t\$OpenBSD\$\t*/\n\n")
	printf("/*\n")
	printf(" * THIS FILE AUTOMATICALLY GENERATED.  DO NOT EDIT.\n")
	printf(" *\n")
	printf(" * generated from:\n")
	printf(" */\n")
	print VERSION

	next
}
$1 == "#include" {
	if ($2 == "<dev/pckbc/wskbdmap_mfii.h>")
		print "#include <hppa/gsc/gsckbdmap.h>"
	else
		printf("#include %s\n", $2)

	next
}
$1 == "#define" || $1 == "#undef" {
	print $0
	next
}
/pckbd/ {
	gsub("pckbd", "gsckbd", $0)
	print $0
	next
}
/KC/ {
	sidx = substr($1, 4, length($1) - 5)
	orig = int(sidx)
	id = conv[orig]

	# 183 is another Print Screen...
	if (orig == 183)
		next

	if (id == -1) {
		printf("/* initially KC(%d),", orig)
		for (f = 2; f <= NF; f++) {
			if ($f != "/*" && $f != "*/")
				printf("\t%s", $f)
		}
		printf("\t*/\n")
	} else {
		printf("    KC(%d),", id)
		for (f = 2; f <= NF; f++) {
			printf("\t%s", $f)
		}
		printf("\n")
	}

	next
}
/};/ {
	if (mapnum == 0) {
		# Add 241 to the US map...
		printf("    KC(241),\tKS_Delete,\n")
	}
	mapnum++
}
{
	print $0
}
