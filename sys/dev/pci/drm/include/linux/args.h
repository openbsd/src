/* Public domain. */

#ifndef _LINUX_ARGS_H
#define _LINUX_ARGS_H

#define CONCATENATE(x, y)	__CONCAT(x, y)

#define _COUNT_ARGS(_15, _14, _13, _12, _11, _10, _9, _8, _7, _6, _5, _4, _3, _2, _1, _0, N, ...) N
#define COUNT_ARGS(...) _COUNT_ARGS(, ##__VA_ARGS__, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)

#endif
