#	$OpenBSD: genassym.sh,v 1.3 2003/07/02 00:37:31 avsm Exp $
#	$NetBSD: genassym.sh,v 1.1 2000/08/20 14:58:45 mrg Exp $

#
# Copyright (c) 1998 Eduardo E. Horvath.
# Copyright (c) 1997 Matthias Pfaller.
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
#	This product includes software developed by Matthias Pfaller.
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

# If first argument is -c, create a temporary C file,
# compile it and execute the result.

awk=${AWK:-awk}
TMP1=`mktemp /tmp/genassym1.XXXXXXXXXX` || exit 1
TMP2=`mktemp /tmp/genassym2.XXXXXXXXXX` || {
	rm -f $TMP1
	exit 1
}

trap 'rm -f $TMP1 $TMP2' 0 1 2 3 13 15

if [ $1 = '-c' ] ; then
	shift
	ccode=1
else
	ccode=0
fi

$awk '
BEGIN {
	printf("#ifndef _KERNEL\n#define _KERNEL\n#endif\n");
	printf("#define	offsetof(type, member) ((size_t)(&((type *)0)->member))\n");
	defining = 0;
	type = "long";
	asmtype = "n";
	asmprint = "";
}

$0 ~ /^[ \t]*#.*/ || $0 ~ /^[ \t]*$/ {
	# Just ignore comments and empty lines
	next;
}

$0 ~ /^config[ \t]/ {
	type = $2;
	asmtype = $3;
	asmprint = $4;
	next;
}

/^include[ \t]/ {
	if (defining != 0) {
		defining = 0;
		printf("}\n");
	}
	printf("#%s\n", $0);
	next;
}

$0 ~ /^if[ \t]/ ||
$0 ~ /^ifdef[ \t]/ ||
$0 ~ /^ifndef[ \t]/ ||
$0 ~ /^else/ ||
$0 ~ /^elif[ \t]/ ||
$0 ~ /^endif/ {
	printf("#%s\n", $0);
	next;
}

/^struct[ \t]/ {
	structname = $2;
	$0 = "define " structname "_SIZEOF sizeof(struct " structname ")";
	# fall through
}

/^export[ \t]/ {
	$0 = "define " $2 " " $2;
	# fall through
}

/^define[ \t]/ {
	if (defining == 0) {
		defining = 1;
		printf("void f" FNR "(void);\n");
		printf("void f" FNR "() {\n");
		if (ccode)
			call[FNR] = "f" FNR;
		defining = 1;
	}
	value = $0
	gsub("^define[ \t]+[A-Za-z_][A-Za-z_0-9]*[ \t]+", "", value)
	if (ccode)
		printf("printf(\"#define " $2 " %%ld\\n\", (%s)" value ");\n", type);
	else
		printf("__asm(\"XYZZY d# %%%s0 constant %s\" : : \"%s\" (%s));\n", asmprint, $2, asmtype, value);
	next;
}

/^member[ \t]/ {
	$0 = "define " $2 " offsetof(struct " structname ", " $2 ")";
	if (defining == 0) {
		defining = 1;
		printf("void f" FNR "(void);\n");
		printf("void f" FNR "() {\n");
		if (ccode)
			call[FNR] = "f" FNR;
		defining = 1;
	}
	value = $0
	gsub("^define[ \t]+[A-Za-z_][A-Za-z_0-9]*[ \t]+", "", value)
	if (ccode)
		printf("printf(\"#define " $2 " %%ld\\n\", (%s)" value ");\n", type);
	else
		printf("__asm(\"XYZZY : %s d\# %%%s0 + ;\" : : \"%s\" (%s));\n", $2, asmprint, asmtype, value);
	next;
}

/^quote[ \t]/ {
	gsub("^quote[ \t]+", "");
	print;
	next;
}

{
	printf("syntax error in line %d\n", FNR) >"/dev/stderr";
	exit(1);
}

END {
	if (defining != 0) {
		defining = 0;
		printf("}\n");
	}
	if (ccode) {
		printf("int main(int argc, char **argv) {");
		for (i in call)
			printf(call[i] "();");
		printf("return(0); }\n");
	}
}
' ccode=$ccode > $TMP1 || exit 1

if [ $ccode = 1 ] ; then
	"$@" -x c $TMP1 -o $TMP2 && $TMP2
else
	# Kill all of the "#" and "$" modifiers; locore.s already
	# prepends the correct "constant" modifier.
	"$@" -x c -S ${TMP1} -o ${TMP2} || exit 1
	sed 's/\$//g' ${TMP2} | sed -n 's/.*XYZZY//gp'
fi
