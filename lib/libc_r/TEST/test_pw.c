/*	$OpenBSD: test_pw.c,v 1.4 2000/01/06 06:58:34 d Exp $	*/
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <pwd.h>
#include "test.h"

int
main()
{
	struct passwd *pw;

	CHECKn(pw = getpwuid(getuid()));
	printf("getpwuid(%d) => %p\n", getuid(), pw);
	printf(" name: %s\n  uid: %d\n  gid: %d\n"
	    "class: %s\ngecos: %s\n  dir: %s\nshell: %s\n",
	    pw->pw_name, pw->pw_uid, pw->pw_gid,
	    pw->pw_class, pw->pw_gecos, pw->pw_dir, pw->pw_shell);
	SUCCEED;
}
