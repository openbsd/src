/*	$OpenBSD: test_pw.c,v 1.5 2000/03/23 05:18:33 d Exp $	*/
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <pwd.h>
#include "test.h"

int
main()
{
	struct passwd *pw;
	struct passwd pwbuf;
	char buf[8192];
	char name[16];

	CHECKn(pw = getpwuid(getuid()));
	printf("getpwuid(%d) => %p\n", getuid(), pw);
	printf(" name: %s\n  uid: %d\n  gid: %d\n"
	    "class: %s\ngecos: %s\n  dir: %s\nshell: %s\n",
	    pw->pw_name, pw->pw_uid, pw->pw_gid,
	    pw->pw_class, pw->pw_gecos, pw->pw_dir, pw->pw_shell);

	strlcpy(name, pw->pw_name, sizeof name);
	CHECKe(getpwnam_r(name, &pwbuf, buf, sizeof buf, &pw));
	ASSERT(pwbuf.pw_uid == getuid());

	SUCCEED;
}
