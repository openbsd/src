
#ifndef	_LIBM_FENV_H_
#define	_LIBM_FENV_H_

#include_next <fenv.h>

PROTO_NORMAL(feclearexcept);
PROTO_STD_DEPRECATED(fedisableexcept);
PROTO_STD_DEPRECATED(feenableexcept);
PROTO_NORMAL(fegetenv);
PROTO_STD_DEPRECATED(fegetexcept);
PROTO_STD_DEPRECATED(fegetexceptflag);
PROTO_NORMAL(fegetround);
PROTO_NORMAL(feholdexcept);
PROTO_NORMAL(feraiseexcept);
PROTO_NORMAL(fesetenv);
PROTO_NORMAL(fesetexceptflag);
PROTO_NORMAL(fesetround);
PROTO_NORMAL(fetestexcept);
PROTO_NORMAL(feupdateenv);

#endif	/* ! _LIBM_FENV_H_ */
