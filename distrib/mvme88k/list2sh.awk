#	$OpenBSD: list2sh.awk,v 1.2 2001/08/29 15:01:17 krw Exp $

BEGIN {
	printf("cd ${CURDIR}\n");
	printf("\n");
}
/^$/ || /^#/ {
	print $0;
	next;
}
$1 == "COPY" {
	printf("echo '%s'\n", $0);
	printf("cp %s ${TARGDIR}/%s\n", $2, $3);
	next;
}
$1 == "LINK" {
	printf("echo '%s'\n", $0);
	printf("(cd ${TARGDIR}; ln %s %s)\n", $2, $3);
	next;
}
$1 == "SPECIAL" {
	printf("echo '%s'\n", $0);
	sub(/^[ \t]*SPECIAL[ \t]*/, "");
	printf("(cd ${TARGDIR}; %s)\n", $0);
	next;
}
{
	printf("echo '%s'\n", $0);
	printf("echo 'Unknown keyword \"%s\" at line %d of input.'\n", $1, NR);
	printf("exit 1\n");
	exit 1;
}
END {
	printf("\n");
	printf("exit 0\n");
	exit 0;
}
