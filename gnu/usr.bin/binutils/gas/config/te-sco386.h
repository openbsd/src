/* Machine specific defines for the SCO Unix V.3.2 ODT */

/* Local labels start with a period.  */
#define LOCAL_LABEL(name) (name[0] == '.' \
                           && (name[1] == 'L' || name[1] == '.'))
#define FAKE_LABEL_NAME ".L0\001"

#include "obj-format.h"

/* end of te-sco386.h */
