#include "f2c.h"
#include "fio.h"

 static FILE *
#ifdef KR_headers
unit_chk(unit, who) integer unit; char *who;
#else
unit_chk(integer unit, char *who)
#endif
{
	if (unit >= MXUNIT || unit < 0)
		f__fatal(101, who);
	return f__units[unit].ufd;
	}

 integer
#ifdef KR_headers
ftell_(unit) integer *unit;
#else
ftell_(integer *unit)
#endif
{
	FILE *f;
	return (f = unit_chk(*unit, "ftell")) ? ftell(f) : -1L;
	}

 int
#ifdef KR_headers
fseek_(unit, offset, xwhence) integer *unit, *offset, *xwhence;
#else
fseek_(integer *unit, integer *offset, integer *xwhence)
#endif
{
	int whence;
	FILE *f;

	switch (*xwhence) {
		default:
			errno = EINVAL;
			return 1;
		case 0:
			whence = SEEK_SET;
			break;
		case 1:
			whence = SEEK_CUR;
			break;
		case 2:
			whence = SEEK_END;
			break;
		}

	return	!(f = unit_chk(*unit, "fseek"))
		|| fseek(f, *offset, whence) ? 1 : 0;
	}
