
/* This code is 32-bit code which calls 16-bit functions defined in 
 * win-e7kpc16.c in order to interface with the memory mapped 
 * physical memory for the Hitachi e7000-pc.
 */

#include <stdio.h>
#include "win-e7kpc.h"
typedef int (CALLBACK *PROC16_PFN)(int);

// Prototype undocumented KERNEL32 functions
HINSTANCE WINAPI LoadLibrary16( PSTR );
void WINAPI FreeLibrary16( HINSTANCE );
FARPROC WINAPI GetProcAddress16( HINSTANCE, PSTR );
void __cdecl QT_Thunk(void);

PROC16_PFN pfn_set_mem = 0;   // We don't want these as locals in
PROC16_PFN pfn_get_mem = 0;   
HINSTANCE hInstUser16;                  // main(), since QT_THUNK could
static long save_sp;
static WORD set_mem_rc;                 // trash them...
static WORD mem;                 
static long mem_offset;
static long mem_parms;

    static int i;
//============= stand-alone test code ==================
#ifdef STAND_ALONE
    static int len=20;
    static int offset=10;
    static char buf[512];
    static FILE *fout;
#ifdef _WIN32
main()

#else
 int PASCAL WinMain (HINSTANCE   hinst,
			 HINSTANCE   hinstPrev,
			 LPSTR       lpszCmdLine,
			 int         cmdShow
			 )
#endif
{
    //============= trash space ==================
    char buffer[0x40];
    buffer[0] = 0;  // Make sure to use the local variable so that the
                    // compiler sets up an EBP frame

    fout=fopen("errs","w");
    fprintf(fout,"Testing... i=%d, offset=%d\n", i,offset);
    if (win_load_e7kpc() != 0)
    {
        fprintf(fout,"ERROR! unable to load e7kpc dll\n"); 
        return 1;
    }
    
    //============= do test ============================
    for (i=0; i<len; i++) 
	buf[i] = 0x50+offset+i;

    // Should put garbage pixels on top left of screen
    win_mem_put(buf,len,offset);
    win_mem_get(buf,len,offset);

    for (i=0; i<len; i++) 
    {
	if (0x50+offset+i != buf[i])
	    fprintf(fout,"ERROR! i=%d, memexp=%d, memis=%d\n", 
		    i,0x50+offset+i,buf[i]);
    }
    fclose(fout);

    win_unload_e7kpc();

    return 0;
}
#endif /* STAND_ALONE */

//============= load dll and get proc addresses ==================
int win_load_e7kpc(void)
{
    hInstUser16 = LoadLibrary16("wine7kpc.dll");
    if ( hInstUser16 < (HINSTANCE)32 )
    {
        printf( "LoadLibrary16() failed!\n" );
        return 1;
    }

    pfn_set_mem =
        (PROC16_PFN) GetProcAddress16(hInstUser16, "set_mem");
    if ( !pfn_set_mem )
    {
        printf( "GetProcAddress16() failed!\n" );
        return 1;
    }
    pfn_get_mem =
        (PROC16_PFN) GetProcAddress16(hInstUser16, "get_mem");
    if ( !pfn_get_mem )
    {
        printf( "GetProcAddress16() failed!\n" );
        return 1;
    }
    return 0;
}

//============= cleanup ============================
void win_unload_e7kpc(void)
{
    FreeLibrary16( hInstUser16 );   
}
    
void win_mem_put (char* buf, int len, int offset)
{
    //============= trash space ==================
    char buffer[0x40];
    buffer[0] = 0;  // Make sure to use the local variable so that the
                    // compiler sets up an EBP frame
    //============= write mem bytes from buf ==================
    for (i=0,mem_offset=offset; i<len; i++, mem_offset++) 
    {
	mem=buf[i];
	// push both 16-bit parms with one 32-bit push
	mem_parms=(mem&0xffff)|(mem_offset<<16)&0xffff0000;
	// thunk from 32 to 16-bit code in dll
	__asm 
	{
	    mov	save_sp, esp
	    mov     edx, [mem_parms]
	    push    edx
	    mov     edx, [pfn_set_mem]
	    call    QT_Thunk
	    mov	esp, save_sp
	    mov     [set_mem_rc], ax
        }
    }
}

void win_mem_get (char* buf, int len, int offset)
{
    //============= trash space ==================
    char buffer[0x40];
    buffer[0] = 0;  // Make sure to use the local variable so that the
                    // compiler sets up an EBP frame
    //============= collect mem bytes into buf ==================
    for (i=0,mem_offset=offset; i<len; i++, mem_offset++) 
    {
	// thunk from 32 to 16-bit code in dll
	__asm 
	{
	    mov	save_sp, esp
	    mov     edx, [mem_offset]
	    push    edx
	    mov     edx, [pfn_get_mem]
	    call    QT_Thunk
	    mov	esp, save_sp
	    mov     [mem], ax
        }
	buf[i]=mem;
    }
}
