#include <sys/param.h>

#ifndef ULONG_MAX
#define	ULONG_MAX	((u_long)(~0L))		/* 0xFFFFFFFF */
#endif

u_long
strtoul(s, ptr, base)
	const char *s;
	char **ptr;
	int base;
{
	u_long total = 0;
	u_int digit;
	const char *start=s;
	int did_conversion=0;
	int overflow = 0;
	int negate = 0;
	u_long maxdiv, maxrem;

	if (s == NULL) {
		if (!ptr)
			*ptr = (char *)start;
		return 0L;
	}

	while (*s == ' ')
		s++;
	if (*s == '+')
		s++;
	else if (*s == '-')
		s++, negate = 1;
	if (base==0 || base==16) { /*  the 'base==16' is for handling 0x */
		int tmp;

      /*
       * try to infer base from the string
       */
		if (*s != '0')
			tmp = 10;	/* doesn't start with 0 - assume decimal */
		else if (s[1] == 'X' || s[1] == 'x')
			tmp = 16, s += 2; /* starts with 0x or 0X - hence hex */
		else
			tmp = 8;	/* starts with 0 - hence octal */
		if (base==0)
			base = (int)tmp;
	}

	maxdiv = ULONG_MAX / base;
	maxrem = ULONG_MAX % base;

	while ((digit = *s) != '\0') {
		if (digit >= '0' && digit < ('0'+base))
			digit -= '0';
		else
			if (base > 10) {
				if (digit >= 'a' && digit < ('a'+(base-10)))
					digit = digit - 'a' + 10;
				else if (digit >= 'A' && digit < ('A'+(base-10)))
					digit = digit - 'A' + 10;
				else
					break;
			}
			else
				break;
			did_conversion = 1;
			if (total > maxdiv
			    || (total == maxdiv && digit > maxrem))
				overflow = 1;
			total = (total * base) + digit;
			s++;
	}
	if (overflow) {
		if (ptr != NULL)
			*ptr = (char *)s;
		return (ULONG_MAX);
	}
	if (ptr != NULL)
		*ptr = (char *) ((did_conversion) ? (char *)s : (char *)start);
	return negate ? -total : total;
}

/* End of strtoul.c */
