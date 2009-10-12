#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

int xsusage_one()       { return 1; } 
int xsusage_two()       { return 2; }
int xsusage_three()     { return 3; }
int xsusage_four()      { return 4; }
int xsusage_five(int i) { return 5; }
int xsusage_six(int i)  { return 6; }

MODULE = XSUsage         PACKAGE = XSUsage	PREFIX = xsusage_

PROTOTYPES: DISABLE

int
xsusage_one()

int
xsusage_two()
    ALIAS:
        two_x = 1
        FOO::two = 2

int
interface_v_i()
    INTERFACE:
        xsusage_three

int
xsusage_four(...)

int
xsusage_five(int i, ...)

int
xsusage_six(int i = 0)
