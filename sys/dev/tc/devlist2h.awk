#! /usr/bin/awk -f
#	$OpenBSD: devlist2h.awk,v 1.2 1996/06/10 07:34:58 deraadt Exp $
#	$NetBSD: devlist2h.awk,v 1.2.4.1 1996/06/05 18:34:36 cgd Exp $
#
# Copyright (c) 1995, 1996 Christopher G. Demetriou
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
#      This product includes software developed by Christopher G. Demetriou.
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
BEGIN {
	nproducts = 0
	dfile="tcdevs_data.h"
	hfile="tcdevs.h"
}
NR == 1 {
	VERSION = $0
	gsub("\\$", "", VERSION)

	printf("/*\n") > dfile
	printf(" * THIS FILE AUTOMATICALLY GENERATED.  DO NOT EDIT.\n") \
	    > dfile
	printf(" *\n") > dfile
	printf(" * generated from:\n") > dfile
	printf(" *\t%s\n", VERSION) > dfile
	printf(" */\n") > dfile

	printf("/*\n") > hfile
	printf(" * THIS FILE AUTOMATICALLY GENERATED.  DO NOT EDIT.\n") \
	    > hfile
	printf(" *\n") > hfile
	printf(" * generated from:\n") > hfile
	printf(" *\t%s\n", VERSION) > hfile
	printf(" */\n") > hfile

	next
}
$1 == "device" {
	ndevices++

	devices[ndevices, 0] = $2;		# devices id
	devices[ndevices, 1] = $2;		# C identifier for device
	gsub("-", "_", devices[ndevices, 1]);

	devices[ndevices, 2] = $3;		# driver name

	printf("\n") > hfile
	printf("#define\tTC_DEVICE_%s\t\"%s\"\n", devices[ndevices, 1],
	    devices[ndevices, 2]) > hfile

	printf("#define\tTC_DESCRIPTION_%s\t\"", devices[ndevices, 1]) > hfile

	f = 4;
	i = 3;

	# comments
	ocomment = oparen = 0
	if (f <= NF) {
		ocomment = 1;
	}
	while (f <= NF) {
		if ($f == "#") {
			printf("(") > hfile
			oparen = 1
			f++
			continue
		}
		if (oparen) {
			printf("%s", $f) > hfile
			if (f < NF)
				printf(" ") > hfile
			f++
			continue
		}
		devices[ndevices, i] = $f
		printf("%s", devices[ndevices, i]) > hfile
		if (f < NF)
			printf(" ") > hfile
		i++; f++;
	}
	if (oparen)
		printf(")") > hfile
	if (ocomment)
		printf("\"") > hfile
	printf("\n") > hfile

	next
}
{
	if ($0 == "")
		blanklines++
	if (blanklines < 2)
		print $0 > hfile
	if (blanklines < 2)
		print $0 > dfile
}
END {
	# print out the match tables

	printf("\n") > dfile

	printf("struct tc_knowndev tc_knowndevs[] = {\n") > dfile
	for (i = 1; i <= ndevices; i++) {
		printf("\t{\n") > dfile
		printf("\t    \"%-8s\",\n", devices[i, 0]) \
		    > dfile
		printf("\t    TC_DEVICE_%s,\n", devices[i, 1]) \
		    > dfile
		printf("\t    TC_DESCRIPTION_%s,\n", devices[i, 1]) \
		    > dfile

		printf("\t},\n") > dfile
	}
	printf("\t{ NULL, NULL, NULL, }\n") > dfile
	printf("};\n") > dfile
}
