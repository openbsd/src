#	$OpenBSD: list2sh.awk,v 1.3 1997/05/05 16:31:37 grr Exp $

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
	printf("(cd %s && find . ! -name . | cpio -pdamu ${TARGDIR}/%s)\n", $2,
	    $3);
	next;
}
$1 == "SPECIAL" {
# escaping shell quotation is ugly whether you use " or ', use cat <<'!' ...
	work=$0;
	gsub("[\\\\]", "\\\\", work);
	gsub("[\"]", "\\\"", work);
	gsub("[$]", "\\$", work);
	gsub("[`]", "\\`", work);
	printf("echo \"%s\"\n", work);
	work=$0;
	sub("^[ 	]*" $1 "[ 	]*", "", work);
	printf("(cd ${TARGDIR}; %s)\n", work);
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
