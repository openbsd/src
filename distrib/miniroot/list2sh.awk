#	$NetBSD: list2sh.awk,v 1.1 1995/12/18 22:47:30 pk Exp $

BEGIN {
	printf("cd ${OBJDIR}\n");
	printf("\n");
}
/^$/ || /^#/ {
	print $0;
	next;
}
$1 == "COPY" {
	printf("echo '%s'\n", $0);
	printf("rm -f ${TARGDIR}/%s\n", $3);
	printf("cp %s ${TARGDIR}/%s\n", $2, $3);
	next;
}
$1 == "LINK" {
	printf("echo '%s'\n", $0);
	printf("rm -f ${TARGDIR}/%s\n", $3);
	printf("(cd ${TARGDIR}; ln %s %s)\n", $2, $3);
	next;
}
$1 == "SYMLINK" {
	printf("echo '%s'\n", $0);
	printf("rm -f ${TARGDIR}/%s\n", $3);
	printf("(cd ${TARGDIR}; ln -s %s %s)\n", $2, $3);
	next;
}
$1 == "COPYDIR" {
	printf("echo '%s'\n", $0);
	printf("(cd ${TARGDIR}/%s && find . ! -name . | xargs /bin/rm -rf)\n",
	    $3);
	printf("(cd %s && pax -pe -rw . ${TARGDIR}/%s)\n", $2, $3);
	next;
}
$1 == "SPECIAL" {
	printf("echo '%s'\n", $0);
	printf("(cd ${TARGDIR};");
	for (i = 2; i <= NF; i++)
		printf(" %s", $i);
	printf(")\n");
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
