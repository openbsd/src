/* should not compile, but should not crash pcc either */
struct a {
	struct x {
		int b;
	} c[2];
} p[2] = { { { 1 }, { 2 } } };
