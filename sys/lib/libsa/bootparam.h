/*	$OpenBSD: bootparam.h,v 1.3 1998/05/31 23:39:18 mickey Exp $	*/

int bp_whoami __P((int sock));
int bp_getfile __P((int sock, char *key, struct in_addr *addrp, char *path));

