#	$OpenBSD: ex.awk,v 1.3 2017/12/14 10:02:53 martijn Exp $

#	@(#)ex.awk	10.1 (Berkeley) 6/8/95

BEGIN {
	printf("enum {");
	first = 1;
}
/^\/\* C_[0-9A-Z_]* \*\/$/ {
	printf("%s\n\t%s%s", first ? "" : ",", $2, first ? " = 0" : "");
	first = 0;
	next;
}
END {
	printf("\n};\n");
}
