/* Architecture independant NetBSD host support */

#include <machine/endian.h>
#include <machine/vmparam.h>
#include <machine/param.h>
#include <machine/reg.h>

#define	HOST_PAGE_SIZE			NBPG
#define	HOST_TEXT_START_ADDR		USRTEXT
#define	HOST_STACK_END_ADDR		USRSTACK

#if BYTE_ORDER == BIG_ENDIAN
#define HOST_BIG_ENDIAN_P
#endif
