#include "f2c.h"
#include "fio.h"

 static FILE *
#ifdef KR_headers
unit_chk(Unit, who) integer Unit; char *who;
#else
unit_chk(integer Unit, char *who)
#endif
{
	if (Unit >= MXUNIT || Unit < 0)
		f__fatal(101, who);
	return f__units[Unit].ufd;
	}

 integer
#ifdef KR_headers
ftell_(Unit) integer *Unit;
#else
ftell_(integer *Unit)
#endif
{
	FILE *f;
	return (f = unit_chk(*Unit, "ftell")) ? ftell(f) : -1L;
	}

 int
#ifdef KR_headers
fseek_(Unit, offset, xwhence) integer *Unit, *offset, *xwhence;
#else
fseek_(integer *Unit, integer *offset, integer *xwhence)
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

	return	!(f = unit_chk(*Unit, "fseek"))
		|| fseek(f, *offset, whence) ? 1 : 0;
	}
