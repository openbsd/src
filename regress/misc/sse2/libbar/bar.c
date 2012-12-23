#include <emmintrin.h>

void bar(void) __attribute__((constructor));

void
bar(void)
{
	__m128i xmm_alpha;

	if ((((unsigned long)&xmm_alpha) & 15) != 0)
		exit(1);
}
