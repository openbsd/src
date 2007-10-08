/*      $OpenBSD: test-11.c,v 1.3 2007/10/08 08:18:35 gilles Exp $	*/

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
 * A function prototype with multiple attributes after,
 * one of which (volatile) is stupidly also a C keyword.
 */
__attribute__((__noreturn__)) void foo5(const char *, ...)
	__attribute__((volatile, __format__ (printf, 1, 2)));

/*
 * A function prototype with unnamed parameters having attributes.
 */
void foo6(char[], int __attribute__((unused)));

/*
 * A function prototype with named parameters having attributes.
 */
void foo7(char func[], int i __attribute__((unused)));

/*
 * A function definition with a single attribute before.
 */
__attribute__((__noreturn__)) void
foo8(void)
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
foo9(void)
{
	exit(0);
}

/*
 * A struct type having members with attributes.
 */
typedef
struct mystruct {
	unsigned char	c_data[128]	__packed;
	unsigned int	u_data[128]	__packed;
} mystruct_t;


/*
 * A struct with attributes.
 */
struct mystruct2 {
	unsigned char	c_data[128];
} __packed;

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





