
/* 
 * Source for win-e7kpc16.dll.
 * Writes to physical memory to interface with the Hitachi
 * e7000-pc.
*/

 #include "win-e7kpc.h"

 short CALLBACK set_mem (short offset, short mem)
    {
    WORD FAR * lpVGA = MAKELP( selVGA, offset );
    *lpVGA = mem;
    return 0;
    }

 short CALLBACK get_mem (short offset)
    {
    short mem=0;
    WORD FAR * lpVGA = MAKELP( selVGA, offset );
    mem = *lpVGA;
    return mem;
    }
