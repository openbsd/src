# Target: Renesas Super-H running on Windows CE
TDEPFILES= sh-tdep.o sh64-tdep.o wince.o
TM_FILE= tm-wince.h
MT_CFLAGS=-DSHx -U_X86_ -U_M_IX86 -U__i386__ -U__i486__ -U__i586__ -U__i686__ -DUNICODE -D_WIN32_WCE -DWINCE_STUB='"${target_alias}-stub.exe"'
TM_CLIBS=-lrapi
