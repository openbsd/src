#! /bin/sh
#
#	$OpenBSD: mkkeysym.sh,v 1.1 2000/07/01 23:52:45 mickey Exp $
#	$NetBSD: mkkeysym.sh 1.1 1998/12/28 14:01:17 hannken Exp $
#
#	Build a table of keysyms from a file describing keysyms as:
#
#	/*BEGINKEYSYMDECL*/
#	#define KS_name 0xval
#	...
#	/*ENDKEYSYMDECL*/
#

AWK=${AWK:-awk}

${AWK} '
BEGIN {
	in_decl = 0;
	printf("/* DO  NOT EDIT: AUTOMATICALLY GENERATED FROM '$1' */\n\n");
	printf("struct ksym {\n\tchar *name;\n\tint value;\n};\n\n");
	printf("struct ksym ksym_tab_by_name[] = {\n");
}

END {
	printf("};\n");
}

$1 == "/*BEGINKEYSYMDECL*/" {
	in_decl = 1;
}

$1 == "/*ENDKEYSYMDECL*/" {
	in_decl = 0;
}

$1 ~ /^#[ 	]*define/ && $2 ~ /^KS_/ && $3 ~ /^0x[0-9a-f]*/ {
	if (in_decl)
		printf("\t{ \"%s\", %s },\n", substr($2, 4), $3);
}' $1
