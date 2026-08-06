/* Public domain. */

#ifndef _LINUX_MINMAX_H
#define _LINUX_MINMAX_H

#include <linux/types.h>

#define swap(a, b) \
	do { __typeof(a) __tmp = (a); (a) = (b); (b) = __tmp; } while(0)

#define min_t(t, a, b) ({ \
	t __min_a = (a); \
	t __min_b = (b); \
	__min_a < __min_b ? __min_a : __min_b; })

#define max_t(t, a, b) ({ \
	t __max_a = (a); \
	t __max_b = (b); \
	__max_a > __max_b ? __max_a : __max_b; })

#define MIN_T(t, a, b) min_t(t, a, b)
#define MAX_T(t, a, b) max_t(t, a, b)

#define clamp_t(t, x, a, b) min_t(t, max_t(t, x, a), b)
#define clamp(x, a, b) clamp_t(__typeof(x), x, a, b)
#define clamp_val(x, a, b) clamp_t(__typeof(x), x, a, b)

#define min(a, b) MIN(a, b)
#define max(a, b) MAX(a, b)
#define min3(x, y, z) MIN(x, MIN(y, z))
#define max3(x, y, z) MAX(x, MAX(y, z))

#define min_not_zero(a, b) (a == 0) ? b : ((b == 0) ? a : min(a, b))

#define min_array(_array, _nitems)		\
({						\
	typeof(_array[0]) _r = _array[0];	\
	for (int i = 1; i < _nitems; i++) {	\
		if (_r > _array[i])		\
			_r = _array[i];		\
	}					\
	_r;					\
})

#define max_array(_array, _nitems)		\
({						\
	typeof(_array[0]) _r = _array[0];	\
	for (int i = 1; i < _nitems; i++) {	\
		if (_r < _array[i])		\
			_r = _array[i];		\
	}					\
	_r;					\
})

#endif
