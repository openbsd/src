/* extra braces, should not cause internal compiler error */
struct a {
	int i;
} *p = { { 0 } };
