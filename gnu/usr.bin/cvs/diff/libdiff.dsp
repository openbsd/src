# Microsoft Developer Studio Project File - Name="libdiff" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 5.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=libdiff - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "libdiff.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "libdiff.mak" CFG="libdiff - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "libdiff - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "libdiff - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe

!IF  "$(CFG)" == "libdiff - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ".\libdiff"
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ".\libdiff"
# ADD BASE CPP /nologo /W3 /GX /O2 /I "..\windows-NT" /I "..\lib" /D "_WINDOWS" /D "HAVE_TIME_H" /D "CLOSEDIR_VOID" /D "NDEBUG" /D "WIN32" /D "WANT_WIN_COMPILER_VERSION" /YX /FD /c
# ADD CPP /nologo /W3 /GX /O2 /I "..\windows-NT" /I "..\lib" /D "_WINDOWS" /D "HAVE_TIME_H" /D "CLOSEDIR_VOID" /D "NDEBUG" /D "WIN32" /D "WANT_WIN_COMPILER_VERSION" /YX /FD /c
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "libdiff - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ".\libdiff"
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Target_Dir ".\libdiff"
# ADD BASE CPP /nologo /W3 /GX /Z7 /Od /I "..\windows-NT" /I "..\lib" /D "_DEBUG" /D "_WINDOWS" /D "WIN32" /D "HAVE_TIME_H" /D "CLOSEDIR_VOID" /YX /FD /c
# ADD CPP /nologo /W3 /GX /Z7 /Od /I "..\windows-NT" /I "..\lib" /D "_DEBUG" /D "_WINDOWS" /D "WIN32" /D "HAVE_TIME_H" /D "CLOSEDIR_VOID" /YX /FD /c
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ENDIF 

# Begin Target

# Name "libdiff - Win32 Release"
# Name "libdiff - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;hpj;bat;for;f90"
# Begin Source File

SOURCE=.\analyze.c
# End Source File
# Begin Source File

SOURCE=.\cmpbuf.c
# End Source File
# Begin Source File

SOURCE=.\context.c
# End Source File
# Begin Source File

SOURCE=.\diff.c
# End Source File
# Begin Source File

SOURCE=.\diff3.c
# End Source File
# Begin Source File

SOURCE=.\dir.c
# End Source File
# Begin Source File

SOURCE=.\ed.c
# End Source File
# Begin Source File

SOURCE=.\ifdef.c
# End Source File
# Begin Source File

SOURCE=.\io.c
# End Source File
# Begin Source File

SOURCE=.\normal.c
# End Source File
# Begin Source File

SOURCE=.\side.c
# End Source File
# Begin Source File

SOURCE=.\util.c
# End Source File
# Begin Source File

SOURCE=.\version.c
# End Source File
# End Group
# End Target
# End Project
