# Make file for serdll32.c -> serdll32.dll
CPU=i386

!include <ntwin32.mak>

all: serdll32.dll 

# Update the object file if necessary

serdll32.obj: $*.c serdll32.h
    $(cc) $(cflags) $(cvarsdll) $(cdebug) $*.c

serdll32.lib serdll32.exp: $*.obj $*.def
     $(implib) -machine:$(CPU) -def:$*.def $*.obj -out:$*.lib

serdll32.dll: serdll32.obj serdll32.def serdll32.exp
    $(link) $(conflags) $(ldebug) -dll -entry:DllInit$(DLLENTRY) -base:0x20000000 -out:$@ $*.exp $*.obj $(guilibsdll) w32sut32.lib mpr.lib
        copy serdll32.dll c:\gs
