/*
 * int krb_equiv(u_int32_t ipaddr_a, u_int32_t ipaddr_b);
 *
 * Given two IP adresses return true if they match
 * or are considered to belong to the same host.
 *
 * For example if /etc/krb.equiv looks like
 *
 *    130.237.223.3   192.16.126.3    # alv alv1
 *    130.237.223.4   192.16.126.4    # byse byse1
 *    130.237.228.152 192.16.126.9    # topsy topsy1
 *
 * krb_equiv(alv, alv1) would return true but
 * krb_equiv(alv, byse1) would not.
 *
 * A comment starts with an '#' and ends with '\n'.
 *
 */
#include "krb_locl.h"

int
krb_equiv(a, b)
	u_int32_t a;
	u_int32_t b;
{
#ifdef NO_IPADDR_CHECK
  return 1;
#else

  FILE *fil;
  int result = 0;
  char line[256];
  
  if (a == b)			/* trivial match */
    return 1;
  
  fil = fopen(KRB_EQUIV, "r");
  if (fil == NULL)		/* open failed */
    return 0;
  
  while (fgets(line, sizeof(line)-1, fil) != NULL) /* for each line */
    {
      int hit_a = 0, hit_b = 0;
      char *t = line;
      
      /* for each item on this line */
      while (*t != 0)		/* more addresses on this line? */
	if (*t == '#')		/* rest is comment */
	  *t = 0;
	else if (isspace(*t))	/* skip space */
	  t++;
	else if (isdigit(*t))	/* an address? */
	  {
	    u_int32_t tmp = inet_addr(t);
	    if (tmp == -1)
	      ;			/* not an address (or broadcast) */
	    else if (tmp == a)
	      hit_a = 1;
	    else if (tmp == b)
	      hit_b = 1;
	    
	    while (*t == '.' || isdigit(*t)) /* done with this address */
	      t++;
	  }
	else
	  *t = 0;		/* garbage on this line, skip it */

      /* line is now parsed, if we found 2 matches were done */
      if (hit_a && hit_b)
	{
	  result = 1;
	  goto done;
	}
    }

 done:
  fclose(fil);
  return result;
#endif /* !NO_IPADDR_CHECK */
}
