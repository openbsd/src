#! /usr/bin/awk -f
#
#	$OpenBSD: devlist2h.awk,v 1.2 1997/02/03 04:47:16 downsj Exp $
#	$NetBSD: devlist2h.awk,v 1.2 1997/01/30 09:18:36 thorpej Exp $
#
# Copyright (c) 1996 Jason R. Thorpe.  All rights reserved.
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
	ndevices = 0
	fbid = 0
	dfile="diodevs_data.h"
	hfile="diodevs.h"
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

	devices[ndevices, 1] = $2		# nickname
	devices[ndevices, 2] = $3		# dio primary id
	devices[ndevices, 3] = "0"		# dio secondary id
	devices[ndevices, 4] = $4		# number of select codes
						#  used by device

	# if this is the framebuffer entry, save the primary id
	if ($2 == "FRAMEBUFFER") {
		fbid = $3;
	}

	# emit device primary id
	printf("\n#define\tDIO_DEVICE_ID_%s\t%s\n", devices[ndevices, 1], \
	    devices[ndevices, 2]) > hfile

	# emit description
	printf("#define\tDIO_DEVICE_DESC_%s\t\"", devices[ndevices, 1]) \
	    > hfile

	f = 5;

	while (f <= NF) {
		printf("%s", $f) > hfile
		if (f < NF)
			printf(" ") > hfile
		f++;
	}
	printf("\"\n") > hfile

	next
}
$1 == "framebuffer" {
	ndevices++

	devices[ndevices, 1] = $2		# nickname
	devices[ndevices, 2] = fbid		# dio primary id
	devices[ndevices, 3] = $3		# dio secondary id
	devices[ndevices, 4] = $4		# number of select codes
						#  used by device

	# emit device secondary id
	printf("\n#define\tDIO_DEVICE_SECID_%s\t%s\n", devices[ndevices, 1], \
	    devices[ndevices, 3]) > hfile

	# emit description
	printf("#define\tDIO_DEVICE_DESC_%s\t\"", devices[ndevices, 1]) \
	    > hfile

	f = 5;

	while (f <= NF) {
		printf("%s", $f) > hfile
		if (f < NF)
			printf(" ") > hfile
		f++;
	}
	printf("\"\n") > hfile

	next
}
{
	if ($0 == "")
		blanklines++
	if (blanklines != 2 && blanklines != 3)
		print $0 > hfile
	if (blanklines < 2)
		print $0 > dfile
}
END {
	# emit device count

	printf("\n") > dfile
	printf("#define DIO_NDEVICES\t%d\n", ndevices) > dfile

	# emit select code size table

	printf("\n") > dfile

	printf("struct dio_devdata dio_devdatas[] = {\n") > dfile
	for (i = 1; i <= ndevices; i++) {
		printf("\t{ %s,\t%s,\t%s },\n", devices[i, 2],
		    devices[i, 3], devices[i, 4]) > dfile
	}

	printf("};\n") > dfile

	# emit description table

	printf("\n") > dfile
	printf("#ifdef DIOVERBOSE\n") > dfile

	printf("struct dio_devdesc dio_devdescs[] = {\n") > dfile

	for (i = 1; i <= ndevices; i++) {
		printf("\t{ %s,\t%s,\tDIO_DEVICE_DESC_%s },\n",
		    devices[i, 2], devices[i, 3], devices[i, 1]) > dfile
	}

	printf("};\n") > dfile

	printf("#endif /* DIOVERBOSE */\n") > dfile
}
