/*?$OpenBSD: openlibs_ixemul.c,v 1.1 1998/03/29 22:24:53 espie Exp $?*/

#include <graphics/gfxbase.h>
#include <libraries/expansion.h>
#include <libraries/expansionbase.h>
#include <proto/exec.h>

struct ExpansionBase *ExpansionBase;
struct GfxBase *GfxBase;
extern void err(int, const char *, ...);

void
open_libraries()
{
	if ((GfxBase = (void *)OpenLibrary(GRAPHICSNAME, 0)) == NULL)
		err(20, "can't open graphics library");
	if ((ExpansionBase=(void *)OpenLibrary(EXPANSIONNAME, 0)) == NULL)
		err(20, "can't open expansion library");
}		
