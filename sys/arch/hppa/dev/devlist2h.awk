#	$OpenBSD: devlist2h.awk,v 1.1 1998/09/29 07:00:46 mickey Exp $

#
# Copyright (c) 1998 Michael Shalayeff
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
#	This product includes software developed by Michael Shalayeff.
# 4. The name of the author may not be used to endorse or promote products
#    derived from this software without specific prior written permission.
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

BEGIN	{
	ncpu = nboard = 0;
	cpuh="cpudevs.h";
	cpud="cpudevs_data.h";
	brdh="boards.h";
	brdd="boards_data.h";
	SUBSEP = "_";
}

NR == 1	{
	VERSION = $0;
	gsub("\\$", "", VERSION);

	printf("/*\n * THIS FILE AUTOMATICALLY GENERATED. DO NOT EDIT.\n" \
	       " * generated from:\n *\t%s\n */\n\n", VERSION) > cpud;
	printf("/*\n * THIS FILE AUTOMATICALLY GENERATED. DO NOT EDIT.\n" \
	       " * generated from:\n *\t%s\n */\n\n", VERSION) > cpuh;
	printf("/*\n * THIS FILE AUTOMATICALLY GENERATED. DO NOT EDIT.\n" \
	       " * generated from:\n *\t%s\n */\n\n", VERSION) > brdd;
	printf("/*\n * THIS FILE AUTOMATICALLY GENERATED. DO NOT EDIT.\n" \
	       " * generated from:\n *\t%s\n */\n\n", VERSION) > brdh;

	printf("static const struct hppa_mod_info hppa_knownmods[] = {\n")\
		> cpud;
	printf("static const struct hppa_board_info hppa_knownboards[] = {\n")\
		> brdd;
}

$1=="board"	{
	printf("#define\tHPPA_BOARD_%s\t%s\n", $2, $3) > brdh;
	printf("\t{ HPPA_BOARD_%s,\t\"%s\",\t\"", $2, $2) > brdd;
	f = 4;
	while (f <= NF) {
		printf ("%s", $f) > brdd;
		if (f < NF)
			printf (" ") > brdd;
		f++;
	}
	printf("\" },\n") > brdd;
}

$1=="type"	{
	printf("#define\tHPPA_TYPE_%s\t%s\n", toupper($2), $3) > cpuh;
	types[tolower($2)] = 1;
}

{
	if ($1 in types) {
		printf("#define\tHPPA_%s_%s\t%s\n", toupper($1),
		       toupper($2), $3) > cpuh;
		printf("\t{HPPA_TYPE_%s,\tHPPA_%s_%s,\t\"", toupper($1),
		       toupper($1), toupper($2), $3) > cpud;
		f = 4;
		while (f <= NF) {
			printf ("%s", $f) > cpud;
			if (f < NF)
				printf (" ") > cpud;
			f++;
		}
		printf("\" },\n") > cpud;
	}
}

END	{
	printf("\t{ -1 }\n};\n") > brdd;
	for (m in modules) {
		printf("#define\tHPPA_%s\t%s\n", m, modules[m]) > cpuh;
	}
	printf("\t{ -1 }\n};\n") > cpud;
}

