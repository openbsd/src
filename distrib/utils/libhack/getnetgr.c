/*	$OpenBSD: getnetgr.c,v 1.2 2000/03/01 22:10:12 todd Exp $	*/
/*	$NetBSD: getnetgr.c,v 1.2 1995/10/13 18:10:26 gwr Exp $	*/

/*
 * Just stub these out, so it looks like
 * we are not in any any netgroups.
 */

void
endnetgrent()
{
}

void
setnetgrent(ng)
	const char	*ng;
{
}

int
getnetgrent(host, user, domain)
	const char	**host;
	const char	**user;
	const char	**domain;
{
	return 0;
}

int
innetgr(grp, host, user, domain)
	const char	*grp, *host, *user, *domain;
{
	return 0;
}
