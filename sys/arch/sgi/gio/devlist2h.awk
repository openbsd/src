#! /usr/bin/awk -f
#	$OpenBSD: devlist2h.awk,v 1.2 2012/04/18 17:18:10 miod Exp $
#	$NetBSD: devlist2h.awk,v 1.5 2008/05/02 18:11:05 martin Exp $
#
# Copyright (c) 1998 The NetBSD Foundation, Inc.
# All rights reserved.
#
# This code is derived from software contributed to The NetBSD Foundation
# by Christos Zoulas.
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
# THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
# ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
# TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
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
#      This product includes software developed by Christos Zoulas
# 4. The name of the author(s) may not be used to endorse or promote products
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
function collectline(_f, _line) {
	_oparen = 0
	_line = ""
	while (_f <= NF) {
		if ($_f == "#") {
			_line = _line "("
			_oparen = 1
			_f++
			continue
		}
		if (_oparen) {
			_line = _line $_f
			if (_f < NF)
				_line = _line " "
			_f++
			continue
		}
		_line = _line $_f
		if (_f < NF)
			_line = _line " "
		_f++
	}
	if (_oparen)
		_line = _line ")"
	return _line
}
BEGIN {
	nproducts = nvendors = blanklines = 0
	dfile="giodevs_data.h"
	hfile="giodevs.h"
	line=""
}
NR == 1 {
	VERSION = $0
	gsub("\\$", "", VERSION)

	printf("/*\n") > hfile
	printf(" * THIS FILE AUTOMATICALLY GENERATED.  DO NOT EDIT.\n") \
	    > hfile
	printf(" *\n") > hfile
	printf(" * generated from:\n") > hfile
	printf(" *\t%s\n", VERSION) > hfile
	printf(" */\n\n") > hfile

	printf("/*\n") > dfile
	printf(" * THIS FILE AUTOMATICALLY GENERATED.  DO NOT EDIT.\n") \
	    > dfile
	printf(" *\n") > dfile
	printf(" * generated from:\n") > dfile
	printf(" *\t%s\n", VERSION) > dfile
	printf(" */\n\n") > dfile

	next
}
NF > 0 && $1 == "product" {
	nproducts++

	products[nproducts, 1] = $2;
	products[nproducts, 2] = $3
	products[nproducts, 3] = collectline(4, line)

	next
}
{
	if ($0 == "")
		blanklines++
	if (blanklines < 2)
		print $0 > dfile
}
END {
	# print out the match tables

	printf("\n") > dfile

	printf("struct gio_knowndev {\n") > dfile
	printf("\tint productid;\n") > dfile
	printf("\tconst char *product;\n") > dfile
	printf("};\n") > dfile
	printf("\nstruct gio_knowndev gio_knowndevs[] = {\n") > dfile

	printf("\n") > hfile
	for (i = 1; i <= nproducts; i++) {
		printf("#define GIO_PRODUCT_%s\t%s\t/* %s */\n",
		    products[i, 1], products[i,2], products[i, 3]) > hfile

		printf("\t{ GIO_PRODUCT_%s, \"%s\" },\n",
		    products[i, 1], products[i, 3]) > dfile
	}
	printf("\t{ 0, NULL }\n") > dfile
	printf("};\n") > dfile
	close(dfile)
	close(hfile)
}
