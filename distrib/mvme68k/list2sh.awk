#	$OpenBSD: list2sh.awk,v 1.2 2000/01/24 04:50:25 smurph Exp $

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
	printf("rm -fr ${TARGDIR}/%s\n", $3);
	printf("cp %s ${TARGDIR}/%s\n", $2, $3);
	next;
}
$1 == "LINK" {
	printf("echo '%s'\n", $0);
	for (i = 3; i <= NF; i++) {
		printf("rm -f ${TARGDIR}/%s\n", $i);
		printf("(cd ${TARGDIR}; ln %s %s)\n", $2, $i);
	}
	next;
}
$1 == "SYMLINK" {
	printf("echo '%s'\n", $0);
	for (i = 3; i <= NF; i++) {
		printf("rm -f ${TARGDIR}/%s\n", $i);
		printf("(cd ${TARGDIR}; ln -s %s %s)\n", $2, $i);
	}
	next;
}
$1 == "ARGVLINK" {
	# crunchgen directive; ignored here
	next;
}
$1 == "SRCDIRS" {
	# crunchgen directive; ignored here
	next;
}
$1 == "CRUNCHSPECIAL" {
	# crunchgen directive; ignored here
	next;
}
$1 == "COPYDIR" {
	printf("echo '%s'\n", $0);
	printf("(cd ${TARGDIR}/%s && find . ! -name . | xargs /bin/rm -rf)\n",
	    $3);
	printf("(cd %s && find . ! -name . | cpio -pdamu ${TARGDIR}/%s)\n", $2,
	    $3);
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
