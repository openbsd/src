/* Public domain. */

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#pragma redefine_extname __atomic_is_lock_free_c __atomic_is_lock_free

bool
__atomic_is_lock_free_c(size_t size, void *ptr)
{
	switch (size) {
	case 1:
		return true;
	case 2:
		return (((uintptr_t)ptr & 1) == 0);
	case 4:
		return (((uintptr_t)ptr & 3) == 0);
	}

	return false;
}
