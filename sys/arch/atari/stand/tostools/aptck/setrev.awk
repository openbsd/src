#!/usr/bin/awk -f
#
# $NetBSD: setrev.awk,v 1.1 1996/01/16 15:15:55 leo Exp $
#
function revcmp(r1, r2,    n1, n2, a1, a2, n, i) {
	n1 = split(r1, a1, "\.")
	n2 = split(r2, a2, "\.")
	n = (n1 < n2) ? n1 : n2

	for (i = 1; i <= n; ++i) {
		if (a1[i] != a2[i])
			return(a1[i] - a2[i])
	}
	if (n1 != n2)
		return(n1 - n2)
	return(0)
}

BEGIN {
	destfile = ARGV[1]
	rev = "0.0"
}

{
	if (revcmp($4, rev) > 0)
		rev = $4
	next file
}

END {
	while ((e = getline <destfile) > 0) {
		if (/"\$Revision.*\$"/)
			sub("\\\$Revision.*\\\$", "Revision " rev)
		print
	}
	if (e)
		exit(1)
	exit(0)
}
