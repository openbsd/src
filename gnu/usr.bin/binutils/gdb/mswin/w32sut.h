/***
*w32sut.h -
*
*       Copyright (c) 1987-1992, Microsoft Corporation.  All rights reserved.
*
*Purpose:
*       This file declares the constants, structures, and functions
*       used for accessing and using the Universal Thunk mechanism.
*
*       This file should be compiled either with constants W32SUT_16 or
*       W32SUT_32 defined.
*
****/

/* Check that one of the 2 constants is defined  */
#ifdef W32SUT_16
#ifdef W32SUT_32
#error W32SUT_16 and W32SUT_32 cannot be defined simultaneously
#endif
#endif

#ifndef W32SUT_16
#ifndef W32SUT_32
#error  Either W32SUT_16 or W32SUT_32 should be defined
#endif
#endif


/****      Prototypes for 32 bit DLL   ***********/
#ifdef W32SUT_32

typedef DWORD  ( WINAPI * UT32PROC)( LPVOID lpBuff,
                                     DWORD  dwUserDefined,
                                     LPVOID *lpTranslationList
                                   );

BOOL    WINAPI UTRegister( HANDLE     hModule,
                           LPCSTR     lpsz16BitDLL,
                           LPCSTR     lpszInitName,
                           LPCSTR     lpszProcName,
                           UT32PROC * ppfn32Thunk,
                           FARPROC    pfnUT32Callback,
                           LPVOID     lpBuff
                         );


VOID    WINAPI UTUnRegister(HANDLE hModule);

#endif


/****      Prototypes for 16 bit DLL   ***********/
#ifdef W32SUT_16

typedef DWORD (FAR PASCAL  * UT16CBPROC)( LPVOID lpBuff,
                                          DWORD  dwUserDefined,
                                          LPVOID FAR *lpTranslationList
                                        );


LPVOID  WINAPI  UTLinearToSelectorOffset(LPBYTE lpByte);
LPVOID  WINAPI  UTSelectorOffsetToLinear(LPBYTE lpByte);

#endif




