/*	$OpenBSD: rnd.c,v 1.2 2001/01/28 23:41:46 niklas Exp $	*/

#ifndef lint
static char rcsid[] = "$OpenBSD: rnd.c,v 1.2 2001/01/28 23:41:46 niklas Exp $";
#endif /* not lint */

#define RND(x)	((random()>>3) % x)

rn1(x,y)
register x,y;
{
	return(RND(x)+y);
}

rn2(x)
register x;
{
	return(RND(x));
}

rnd(x)
register x;
{
	return(RND(x)+1);
}

d(n,x)
register n,x;
{
	register tmp = n;

	while(n--) tmp += RND(x);
	return(tmp);
}
