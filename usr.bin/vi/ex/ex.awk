#	$OpenBSD: ex.awk,v 1.2 2001/01/29 01:58:40 niklas Exp $

#	@(#)ex.awk	10.1 (Berkeley) 6/8/95
 
/^\/\* C_[0-9A-Z_]* \*\/$/ {
	printf("#define %s %d\n", $2, cnt++);
	next;
}
