/* A wrapper around strtol() and strtoul() to correct some
 * "out of bounds" cases that don't work well on at least UTS.
 * If a value is Larger than the max, strto[u]l should return
 * the max value, and set errno to ERANGE
 *  The same if a value is smaller than the min value (only
 * relevant for strtol(); not strtoul()), except the minimum
 * value is returned (and errno == ERANGE).
 */

#include	<ctype.h>
#include	<string.h>
#include	<sys/errno.h>
#include	<stdlib.h>

extern int	errno;

#undef	I32
#undef	U32

#define	I32	int
#define	U32	unsigned int

struct	base_info {
	char	*ValidChars;

	char	*Ulong_max_str;
	char	*Long_max_str;
	char	*Long_min_str;	/* Absolute value */

	int	Ulong_max_str_len;
	int	Long_max_str_len;
	int	Long_min_str_len;	/* Absolute value */

	U32	Ulong_max;
	I32	Long_max;
	I32	Long_min;	/* NOT Absolute value */
};
static struct	base_info Base_info[37];

static struct base_info Base_info_16 = {
	"0123456789abcdefABCDEF",
	"4294967295", "2147483648" /* <== ABS VAL */ , "2147483647",
	10, 10, 10,
	4294967295, 2147483647, - 2147483648,
};

static struct base_info Base_info_10 = {
	"0123456789",
	"4294967295", "2147483648" /* <== ABS VAL */ , "2147483647",
	10, 10, 10,
	4294967295, 2147483647, - 2147483648,
};

 /* Used eventually (if this is fully developed) to hold info
  * for processing bases 2-36.  So that we can just plug the
  * base in as a selector for its info, we sacrifice
  * Base_info[0] and Base_info[1] (unless they are used
  * at some point for special information).
  */

/* This may be replaced later by something more universal */
static void
init_Base_info()
{
	if(Base_info[10].ValidChars) return;
	Base_info[10] = Base_info_10;
	Base_info[16] = Base_info_16;
}

unsigned int
strtoul_wrap32(char *s, char **pEnd, int base)
{
	int	Len;
	int	isNegated = 0;
	char	*sOrig = s;

	init_Base_info();

	while(*s && isspace(*s)) ++s;

	if(*s == '-') {
		++isNegated;
		++s;
		while(*s && isspace(*s)) ++s;
	}
	if(base == 0) {
		if(*s == '0') {
			if(s[1] == 'x' || s[1] == 'X') {
				s += 2;
				base = 16;
			} else {
				++s;
				base = 8;
			}
		} else if(isdigit(*s)) {
			base = 10;
		}
	}
	if(base != 10) {
		return strtoul(sOrig, pEnd, base);
	}
	
	Len = strspn(s, Base_info[base].ValidChars);

	if(Len > Base_info[base].Ulong_max_str_len
		||
	   (Len == Base_info[base].Ulong_max_str_len
	   		&&
	    strncmp(Base_info[base].Ulong_max_str, s, Len) < 0)
	  ) {
		/* In case isNegated is set - what to do?? */
		/* Mightn't we say a negative number is ERANGE for strtoul? */
		errno = ERANGE;
		return Base_info[base].Ulong_max;
	}

	return strtoul(sOrig, pEnd, base);
}

int
strtol_wrap32(char *s, char **pEnd, int base)
{
	int	Len;
	int	isNegated = 0;
	char	*sOrig = s;

	init_Base_info();

	while(*s && isspace(*s)) ++s;

	if(*s == '-') {
		++isNegated;
		++s;
		while(*s && isspace(*s)) ++s;
	}
	if(base == 0) {
		if(*s == '0') {
			if(s[1] == 'x' || s[1] == 'X') {
				s += 2;
				base = 16;
			} else {
				++s;
				base = 8;
			}
		} else if(isdigit(*s)) {
			base = 10;
		}
	}
	if(base != 10) {
		return strtol(sOrig, pEnd, base);
	}
	
	Len = strspn(s, Base_info[base].ValidChars);

	if(Len > Base_info[base].Long_max_str_len
				||
	   (!isNegated && Len == Base_info[base].Long_max_str_len
	   	&&
	    strncmp(Base_info[base].Long_max_str, s, Len) < 0)
	    			||
	   (isNegated && Len == Base_info[base].Long_min_str_len
	   	&&
	    strncmp(Base_info[base].Long_min_str, s, Len) < 0)
	  ) {
		/* In case isNegated is set - what to do?? */
		/* Mightn't we say a negative number is ERANGE for strtol? */
		errno = ERANGE;
		return(isNegated ? Base_info[base].Long_min
					:
				   Base_info[base].Long_min);
	}

	return strtol(sOrig, pEnd, base);
}
