#!/bin/sh
#	$OpenBSD: emacs-gen.sh,v 1.2 2004/12/18 20:55:52 millert Exp $

case $# in
1)	file=$1;;
*)
	echo "$0: Usage: $0 path-to-emacs.c" 1>&2
	exit 1
esac;

if [ ! -r "$file" ] ;then
	echo "$0: can't read $file" 1>&2
	exit 1
fi

cat << E_O_F || exit 1
/*
 * NOTE: THIS FILE WAS GENERATED AUTOMATICALLY FROM $file
 *
 * DO NOT BOTHER EDITING THIS FILE
 */
E_O_F

# Pass 1: print out lines before @START-FUNC-TAB@
#	  and generate defines and function declarations,
sed -e '1,/@START-FUNC-TAB@/d' -e '/@END-FUNC-TAB@/,$d' < $file |
	awk 'BEGIN { nfunc = 0; }
	    /^[	 ]*#/ {
			    print $0;
			    next;
		    }
	    {
		fname = $2;
		c = substr(fname, length(fname), 1);
		if (c == ",")
			fname = substr(fname, 1, length(fname) - 1);
		if (fname != "0") {
			printf "#define XFUNC_%s %d\n", substr(fname, 3, length(fname) - 2), nfunc;
			printf "static int %s (int c);\n", fname;
			nfunc++;
		}
	    }' || exit 1

exit 0
