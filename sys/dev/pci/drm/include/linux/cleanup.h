/* Public domain. */

#ifndef _LINUX_CLEANUP_H
#define _LINUX_CLEANUP_H

#define __free(fn)	__cleanup(__free_##fn)

/* function arg is effectively type *, deref is required */
#define DEFINE_FREE(fn, type, body)	\
static inline void			\
__free_##fn(void *p)			\
{					\
	type _T = *(type *)p;		\
	body;				\
}

#define no_free_ptr(p) \
	({void *_p = (p); (p) = NULL; _p; })

#define DEFINE_CLASS(_name, _type, _exit, _enter, _args...)	\
typedef _type class_##_name##_t;				\
static inline _type						\
class_##_name##_constructor(_args)				\
{								\
	_type t = _enter;					\
	return t;						\
}								\
static inline void						\
class_##_name##_destructor(_type *p)				\
{								\
	_type _T = *p;						\
	_exit;							\
}

#define _guard(_type) \
	class_##_type##_t _guard_p __cleanup(class_##_type##_destructor) = \
	    class_##_type##_constructor
#define guard(_type) _guard(_type)

#define _scoped_guard(_type, _varname, _args...)			\
        int _varname = 1;						\
        for (class_##_type##_t _guard_p __cleanup(class_##_type##_destructor) = \
	    class_##_type##_constructor(_args); _varname;_varname--)

#ifndef __COUNTER__
#define __COUNTER__ __LINE
#endif

#define __guardname(num)	_scoped_guard_loop##num
#define _guardname(num)		__guardname(num)
#define guardname()		_guardname(__COUNTER__)
#define scoped_guard(_type, _args...)	_scoped_guard(_type, guardname(), _args)

#endif
