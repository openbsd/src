/*      $OpenBSD: test-11.c,v 1.1 2005/12/02 22:11:46 cloder Exp $	*/

/*
 * Placed in the public domain by Chad Loder <cloder@openbsd.org>.
 *
 * Test lint parsing of gcc's __attribute__ syntax.
 */

/* Define this here so we don't need to pull in a header */
void exit(int);

/*
 * A function prototype with a single attribute before.
 */
__attribute__((__noreturn__)) void foo1(void);

/*
 * A function prototype with a multiple attributes before.
 */
__attribute__((__noreturn__))
__attribute__((__pure__))
__attribute__((__section__("text")))
void foo2(void);

/*
 * A function prototype with a single attribute after.
 */
void foo3(void) __attribute__((__noreturn__));

/*
 * A function prototype with multiple attributes after.
 */
void foo4(void)
	__attribute__((__noreturn__))
	__attribute__((__pure__))
	__attribute__((__section__("text")));


/*
 * A function definition with a single attribute before.
 */
__attribute__((__noreturn__)) void
foo5(void)
{
	exit(0);
}

/*
 * A function definition with multiple attributes before.
 */
__attribute__((__noreturn__))
__attribute__((__pure__))
__attribute__((__section__("text")))
void
foo6(void)
{
	exit(0);
}

/*
 * A struct type having members with attributes.
 */
typedef
struct mystruct {
	unsigned char	c_data[128]	__attribute__((__packed__));
	unsigned int	u_data[128]	__attribute__((__packed__));
} mystruct_t;


/*
 * A struct with attributes.
 */
struct mystruct2 {
	unsigned char	c_data[128];
} __attribute__((__packed__));

/*
 * A typedef with an attribute after the typename.
 */
typedef int more_aligned_int __attribute__ ((aligned (8)));

/*
 * A typedef with attributes before the typename.
 */
typedef short __attribute__((__may_alias__)) short_a;


/*
 * A variable declaration with attributes.
 */
int sh __attribute__((__section__ ("shared")));

/*
 * A variable declaration with attributes and initializer.
 */
int sh2 __attribute__((__section__ ("shared"))) = 0;

/*
 * A simple indirection: "pointer to 8-bit aligned pointer to char"
 */
char * __attribute__((__aligned__(8))) *pac;

/*
 * A really tough one with multiple indirections that even older
 * gcc has problems with.
 */
void (****f)(void) __attribute__((__noreturn__));

int
main(int argc, char* argv[])
{
	return 0;
}





