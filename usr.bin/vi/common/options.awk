#	$OpenBSD: options.awk,v 1.4 2017/12/14 10:02:53 martijn Exp $

#	@(#)options.awk	10.1 (Berkeley) 6/8/95

BEGIN {
	printf("enum {\n");
	first = 1;
}
/^\/\* O_[0-9A-Z_]*/ {
	printf("\t%s%s,\n", $2, first ? " = 0" : "");
	first = 0;
	next;
}
END {
	printf("\tO_OPTIONCOUNT\n};\n");
}
