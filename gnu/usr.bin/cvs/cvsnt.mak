# Microsoft Visual C++ Generated NMAKE File, Format Version 2.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

!IF "$(CFG)" == ""
CFG=Win32 Debug
!MESSAGE No configuration specified.  Defaulting to Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "Win32 Release" && "$(CFG)" != "Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE on this makefile
!MESSAGE by defining the macro CFG on the command line.  For example:
!MESSAGE 
!MESSAGE NMAKE /f "cvsnt.mak" CFG="Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

################################################################################
# Begin Project
# PROP Target_Last_Scanned "Win32 Debug"
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "WinRel"
# PROP BASE Intermediate_Dir "WinRel"
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "WinRel"
# PROP Intermediate_Dir "WinRel"
OUTDIR=.\WinRel
INTDIR=.\WinRel

ALL : .\WinRel\cvs.exe .\WinRel\cvsnt.bsc

$(OUTDIR) : 
    if not exist $(OUTDIR)/nul mkdir $(OUTDIR)

# ADD BASE CPP /nologo /W3 /GX /YX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /FR /c
# ADD CPP /nologo /W3 /GX /YX /Ob1 /I "windows-NT" /I "lib" /I "src" /D "NDEBUG" /D "WIN32" /D "_CONSOLE" /D "HAVE_CONFIG_H" /FR /c
# SUBTRACT CPP /WX
CPP_PROJ=/nologo /W3 /GX /YX /Ob1 /I "windows-NT" /I "lib" /I "src" /D "NDEBUG"\
 /D "WIN32" /D "_CONSOLE" /D "HAVE_CONFIG_H" /FR$(INTDIR)/\
 /Fp$(OUTDIR)/"cvsnt.pch" /Fo$(INTDIR)/ /c 
CPP_OBJS=.\WinRel/
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o$(OUTDIR)/"cvsnt.bsc" 
BSC32_SBRS= \
	.\WinRel\subr.sbr \
	.\WinRel\admin.sbr \
	.\WinRel\server.sbr \
	.\WinRel\diff.sbr \
	.\WinRel\client.sbr \
	.\WinRel\checkout.sbr \
	.\WinRel\no_diff.sbr \
	.\WinRel\entries.sbr \
	.\WinRel\tag.sbr \
	.\WinRel\rtag.sbr \
	.\WinRel\status.sbr \
	.\WinRel\root.sbr \
	.\WinRel\myndbm.sbr \
	.\WinRel\hash.sbr \
	.\WinRel\repos.sbr \
	.\WinRel\parseinfo.sbr \
	.\WinRel\vers_ts.sbr \
	.\WinRel\checkin.sbr \
	.\WinRel\commit.sbr \
	.\WinRel\version.sbr \
	.\WinRel\cvsrc.sbr \
	.\WinRel\remove.sbr \
	.\WinRel\update.sbr \
	.\WinRel\logmsg.sbr \
	.\WinRel\classify.sbr \
	.\WinRel\history.sbr \
	.\WinRel\add.sbr \
	.\WinRel\lock.sbr \
	.\WinRel\recurse.sbr \
	.\WinRel\modules.sbr \
	.\WinRel\find_names.sbr \
	.\WinRel\rcs.sbr \
	.\WinRel\create_adm.sbr \
	.\WinRel\main.sbr \
	.\WinRel\patch.sbr \
	.\WinRel\release.sbr \
	.\WinRel\rcscmds.sbr \
	.\WinRel\import.sbr \
	.\WinRel\ignore.sbr \
	.\WinRel\log.sbr \
	.\WinRel\wrapper.sbr \
	.\WinRel\getwd.sbr \
	.\WinRel\error.sbr \
	.\WinRel\sighandle.sbr \
	.\WinRel\getopt.sbr \
	.\WinRel\argmatch.sbr \
	.\WinRel\md5.sbr \
	.\WinRel\yesno.sbr \
	.\WinRel\getopt1.sbr \
	.\WinRel\valloc.sbr \
	.\WinRel\xgetwd.sbr \
	.\WinRel\regex.sbr \
	.\WinRel\fnmatch.sbr \
	.\WinRel\getdate.sbr \
	".\WinRel\save-cwd.sbr" \
	.\WinRel\mkdir.sbr \
	.\WinRel\run.sbr \
	.\WinRel\pwd.sbr \
	.\WinRel\filesubr.sbr \
	.\WinRel\win32.sbr \
	.\WinRel\waitpid.sbr \
	.\WinRel\ndir.sbr \
	.\WinRel\strippath.sbr \
	.\WinRel\stripslash.sbr \
	.\WinRel\rcmd.sbr \
	.\WinRel\startserver.sbr

.\WinRel\cvsnt.bsc : $(OUTDIR)  $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /NOLOGO /SUBSYSTEM:console /MACHINE:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib wsock32.lib /NOLOGO /SUBSYSTEM:console /MACHINE:I386 /OUT:"WinRel/cvs.exe"
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib\
 advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib\
 odbccp32.lib wsock32.lib /NOLOGO /SUBSYSTEM:console /INCREMENTAL:no\
 /PDB:$(OUTDIR)/"cvsnt.pdb" /MACHINE:I386 /OUT:"WinRel/cvs.exe" 
DEF_FILE=
LINK32_OBJS= \
	.\WinRel\subr.obj \
	.\WinRel\admin.obj \
	.\WinRel\server.obj \
	.\WinRel\diff.obj \
	.\WinRel\client.obj \
	.\WinRel\checkout.obj \
	.\WinRel\no_diff.obj \
	.\WinRel\entries.obj \
	.\WinRel\tag.obj \
	.\WinRel\rtag.obj \
	.\WinRel\status.obj \
	.\WinRel\root.obj \
	.\WinRel\myndbm.obj \
	.\WinRel\hash.obj \
	.\WinRel\repos.obj \
	.\WinRel\parseinfo.obj \
	.\WinRel\vers_ts.obj \
	.\WinRel\checkin.obj \
	.\WinRel\commit.obj \
	.\WinRel\version.obj \
	.\WinRel\cvsrc.obj \
	.\WinRel\remove.obj \
	.\WinRel\update.obj \
	.\WinRel\logmsg.obj \
	.\WinRel\classify.obj \
	.\WinRel\history.obj \
	.\WinRel\add.obj \
	.\WinRel\lock.obj \
	.\WinRel\recurse.obj \
	.\WinRel\modules.obj \
	.\WinRel\find_names.obj \
	.\WinRel\rcs.obj \
	.\WinRel\create_adm.obj \
	.\WinRel\main.obj \
	.\WinRel\patch.obj \
	.\WinRel\release.obj \
	.\WinRel\rcscmds.obj \
	.\WinRel\import.obj \
	.\WinRel\ignore.obj \
	.\WinRel\log.obj \
	.\WinRel\wrapper.obj \
	.\WinRel\getwd.obj \
	.\WinRel\error.obj \
	.\WinRel\sighandle.obj \
	.\WinRel\getopt.obj \
	.\WinRel\argmatch.obj \
	.\WinRel\md5.obj \
	.\WinRel\yesno.obj \
	.\WinRel\getopt1.obj \
	.\WinRel\valloc.obj \
	.\WinRel\xgetwd.obj \
	.\WinRel\regex.obj \
	.\WinRel\fnmatch.obj \
	.\WinRel\getdate.obj \
	".\WinRel\save-cwd.obj" \
	.\WinRel\mkdir.obj \
	.\WinRel\run.obj \
	.\WinRel\pwd.obj \
	.\WinRel\filesubr.obj \
	.\WinRel\win32.obj \
	.\WinRel\waitpid.obj \
	.\WinRel\ndir.obj \
	.\WinRel\strippath.obj \
	.\WinRel\stripslash.obj \
	.\WinRel\rcmd.obj \
	.\WinRel\startserver.obj

.\WinRel\cvs.exe : $(OUTDIR)  $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "WinDebug"
# PROP BASE Intermediate_Dir "WinDebug"
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "WinDebug"
# PROP Intermediate_Dir "WinDebug"
OUTDIR=.\WinDebug
INTDIR=.\WinDebug

ALL : .\WinDebug\cvs.exe .\WinDebug\cvsnt.bsc

$(OUTDIR) : 
    if not exist $(OUTDIR)/nul mkdir $(OUTDIR)

# ADD BASE CPP /nologo /W3 /GX /Zi /YX /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /FR /c
# ADD CPP /nologo /W3 /GX /Zi /YX /Ob1 /I "windows-NT" /I "lib" /I "src" /D "_DEBUG" /D "WIN32" /D "_CONSOLE" /D "HAVE_CONFIG_H" /FR /c
# SUBTRACT CPP /WX
CPP_PROJ=/nologo /W3 /GX /Zi /YX /Ob1 /I "windows-NT" /I "lib" /I "src" /D\
 "_DEBUG" /D "WIN32" /D "_CONSOLE" /D "HAVE_CONFIG_H" /FR$(INTDIR)/\
 /Fp$(OUTDIR)/"cvsnt.pch" /Fo$(INTDIR)/ /Fd$(OUTDIR)/"cvsnt.pdb" /c 
CPP_OBJS=.\WinDebug/
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o$(OUTDIR)/"cvsnt.bsc" 
BSC32_SBRS= \
	.\WinDebug\subr.sbr \
	.\WinDebug\admin.sbr \
	.\WinDebug\server.sbr \
	.\WinDebug\diff.sbr \
	.\WinDebug\client.sbr \
	.\WinDebug\checkout.sbr \
	.\WinDebug\no_diff.sbr \
	.\WinDebug\entries.sbr \
	.\WinDebug\tag.sbr \
	.\WinDebug\rtag.sbr \
	.\WinDebug\status.sbr \
	.\WinDebug\root.sbr \
	.\WinDebug\myndbm.sbr \
	.\WinDebug\hash.sbr \
	.\WinDebug\repos.sbr \
	.\WinDebug\parseinfo.sbr \
	.\WinDebug\vers_ts.sbr \
	.\WinDebug\checkin.sbr \
	.\WinDebug\commit.sbr \
	.\WinDebug\version.sbr \
	.\WinDebug\cvsrc.sbr \
	.\WinDebug\remove.sbr \
	.\WinDebug\update.sbr \
	.\WinDebug\logmsg.sbr \
	.\WinDebug\classify.sbr \
	.\WinDebug\history.sbr \
	.\WinDebug\add.sbr \
	.\WinDebug\lock.sbr \
	.\WinDebug\recurse.sbr \
	.\WinDebug\modules.sbr \
	.\WinDebug\find_names.sbr \
	.\WinDebug\rcs.sbr \
	.\WinDebug\create_adm.sbr \
	.\WinDebug\main.sbr \
	.\WinDebug\patch.sbr \
	.\WinDebug\release.sbr \
	.\WinDebug\rcscmds.sbr \
	.\WinDebug\import.sbr \
	.\WinDebug\ignore.sbr \
	.\WinDebug\log.sbr \
	.\WinDebug\wrapper.sbr \
	.\WinDebug\getwd.sbr \
	.\WinDebug\error.sbr \
	.\WinDebug\sighandle.sbr \
	.\WinDebug\getopt.sbr \
	.\WinDebug\argmatch.sbr \
	.\WinDebug\md5.sbr \
	.\WinDebug\yesno.sbr \
	.\WinDebug\getopt1.sbr \
	.\WinDebug\valloc.sbr \
	.\WinDebug\xgetwd.sbr \
	.\WinDebug\regex.sbr \
	.\WinDebug\fnmatch.sbr \
	.\WinDebug\getdate.sbr \
	".\WinDebug\save-cwd.sbr" \
	.\WinDebug\mkdir.sbr \
	.\WinDebug\run.sbr \
	.\WinDebug\pwd.sbr \
	.\WinDebug\filesubr.sbr \
	.\WinDebug\win32.sbr \
	.\WinDebug\waitpid.sbr \
	.\WinDebug\ndir.sbr \
	.\WinDebug\strippath.sbr \
	.\WinDebug\stripslash.sbr \
	.\WinDebug\rcmd.sbr \
	.\WinDebug\startserver.sbr

.\WinDebug\cvsnt.bsc : $(OUTDIR)  $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /NOLOGO /SUBSYSTEM:console /DEBUG /MACHINE:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib wsock32.lib /NOLOGO /SUBSYSTEM:console /DEBUG /MACHINE:I386 /OUT:"WinDebug/cvs.exe"
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib\
 advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib\
 odbccp32.lib wsock32.lib /NOLOGO /SUBSYSTEM:console /INCREMENTAL:yes\
 /PDB:$(OUTDIR)/"cvsnt.pdb" /DEBUG /MACHINE:I386 /OUT:"WinDebug/cvs.exe" 
DEF_FILE=
LINK32_OBJS= \
	.\WinDebug\subr.obj \
	.\WinDebug\admin.obj \
	.\WinDebug\server.obj \
	.\WinDebug\diff.obj \
	.\WinDebug\client.obj \
	.\WinDebug\checkout.obj \
	.\WinDebug\no_diff.obj \
	.\WinDebug\entries.obj \
	.\WinDebug\tag.obj \
	.\WinDebug\rtag.obj \
	.\WinDebug\status.obj \
	.\WinDebug\root.obj \
	.\WinDebug\myndbm.obj \
	.\WinDebug\hash.obj \
	.\WinDebug\repos.obj \
	.\WinDebug\parseinfo.obj \
	.\WinDebug\vers_ts.obj \
	.\WinDebug\checkin.obj \
	.\WinDebug\commit.obj \
	.\WinDebug\version.obj \
	.\WinDebug\cvsrc.obj \
	.\WinDebug\remove.obj \
	.\WinDebug\update.obj \
	.\WinDebug\logmsg.obj \
	.\WinDebug\classify.obj \
	.\WinDebug\history.obj \
	.\WinDebug\add.obj \
	.\WinDebug\lock.obj \
	.\WinDebug\recurse.obj \
	.\WinDebug\modules.obj \
	.\WinDebug\find_names.obj \
	.\WinDebug\rcs.obj \
	.\WinDebug\create_adm.obj \
	.\WinDebug\main.obj \
	.\WinDebug\patch.obj \
	.\WinDebug\release.obj \
	.\WinDebug\rcscmds.obj \
	.\WinDebug\import.obj \
	.\WinDebug\ignore.obj \
	.\WinDebug\log.obj \
	.\WinDebug\wrapper.obj \
	.\WinDebug\getwd.obj \
	.\WinDebug\error.obj \
	.\WinDebug\sighandle.obj \
	.\WinDebug\getopt.obj \
	.\WinDebug\argmatch.obj \
	.\WinDebug\md5.obj \
	.\WinDebug\yesno.obj \
	.\WinDebug\getopt1.obj \
	.\WinDebug\valloc.obj \
	.\WinDebug\xgetwd.obj \
	.\WinDebug\regex.obj \
	.\WinDebug\fnmatch.obj \
	.\WinDebug\getdate.obj \
	".\WinDebug\save-cwd.obj" \
	.\WinDebug\mkdir.obj \
	.\WinDebug\run.obj \
	.\WinDebug\pwd.obj \
	.\WinDebug\filesubr.obj \
	.\WinDebug\win32.obj \
	.\WinDebug\waitpid.obj \
	.\WinDebug\ndir.obj \
	.\WinDebug\strippath.obj \
	.\WinDebug\stripslash.obj \
	.\WinDebug\rcmd.obj \
	.\WinDebug\startserver.obj

.\WinDebug\cvs.exe : $(OUTDIR)  $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 

.c{$(CPP_OBJS)}.obj:
   $(CPP) $(CPP_PROJ) $<  

.cpp{$(CPP_OBJS)}.obj:
   $(CPP) $(CPP_PROJ) $<  

.cxx{$(CPP_OBJS)}.obj:
   $(CPP) $(CPP_PROJ) $<  

################################################################################
# Begin Group "src"

################################################################################
# Begin Source File

SOURCE=.\src\subr.c
DEP_SUBR_=\
	.\src\cvs.h\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
	".\windows-NT\alloca.h"\
	.\lib\fnmatch.h\
	".\windows-NT\pwd.h"\
	.\lib\system.h\
	.\src\hash.h\
	.\src\server.h\
	.\src\client.h\
	.\src\myndbm.h\
	.\lib\regex.h\
	.\lib\getopt.h\
	.\lib\wait.h\
	.\src\rcs.h\
	".\windows-NT\ndir.h"

!IF  "$(CFG)" == "Win32 Release"

.\WinRel\subr.obj :  $(SOURCE)  $(DEP_SUBR_) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

.\WinDebug\subr.obj :  $(SOURCE)  $(DEP_SUBR_) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\admin.c

!IF  "$(CFG)" == "Win32 Release"

.\WinRel\admin.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

.\WinDebug\admin.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\server.c
DEP_SERVE=\
	.\src\cvs.h\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
	".\windows-NT\alloca.h"\
	.\lib\fnmatch.h\
	".\windows-NT\pwd.h"\
	.\lib\system.h\
	.\src\hash.h\
	.\src\server.h\
	.\src\client.h\
	.\src\myndbm.h\
	.\lib\regex.h\
	.\lib\getopt.h\
	.\lib\wait.h\
	.\src\rcs.h\
	".\windows-NT\ndir.h"

!IF  "$(CFG)" == "Win32 Release"

.\WinRel\server.obj :  $(SOURCE)  $(DEP_SERVE) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

.\WinDebug\server.obj :  $(SOURCE)  $(DEP_SERVE) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\diff.c

!IF  "$(CFG)" == "Win32 Release"

.\WinRel\diff.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

.\WinDebug\diff.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\client.c
DEP_CLIEN=\
	.\src\cvs.h\
	.\src\update.h\
	.\lib\md5.h\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
	".\windows-NT\alloca.h"\
	.\lib\fnmatch.h\
	".\windows-NT\pwd.h"\
	.\lib\system.h\
	.\src\hash.h\
	.\src\server.h\
	.\src\client.h\
	.\src\myndbm.h\
	.\lib\regex.h\
	.\lib\getopt.h\
	.\lib\wait.h\
	.\src\rcs.h\
	".\windows-NT\ndir.h"

!IF  "$(CFG)" == "Win32 Release"

.\WinRel\client.obj :  $(SOURCE)  $(DEP_CLIEN) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

.\WinDebug\client.obj :  $(SOURCE)  $(DEP_CLIEN) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\checkout.c
DEP_CHECK=\
	.\src\cvs.h\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
	".\windows-NT\alloca.h"\
	.\lib\fnmatch.h\
	".\windows-NT\pwd.h"\
	.\lib\system.h\
	.\src\hash.h\
	.\src\server.h\
	.\src\client.h\
	.\src\myndbm.h\
	.\lib\regex.h\
	.\lib\getopt.h\
	.\lib\wait.h\
	.\src\rcs.h\
	".\windows-NT\ndir.h"

!IF  "$(CFG)" == "Win32 Release"

.\WinRel\checkout.obj :  $(SOURCE)  $(DEP_CHECK) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

.\WinDebug\checkout.obj :  $(SOURCE)  $(DEP_CHECK) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\no_diff.c
DEP_NO_DI=\
	.\src\cvs.h\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
	".\windows-NT\alloca.h"\
	.\lib\fnmatch.h\
	".\windows-NT\pwd.h"\
	.\lib\system.h\
	.\src\hash.h\
	.\src\server.h\
	.\src\client.h\
	.\src\myndbm.h\
	.\lib\regex.h\
	.\lib\getopt.h\
	.\lib\wait.h\
	.\src\rcs.h\
	".\windows-NT\ndir.h"

!IF  "$(CFG)" == "Win32 Release"

.\WinRel\no_diff.obj :  $(SOURCE)  $(DEP_NO_DI) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

.\WinDebug\no_diff.obj :  $(SOURCE)  $(DEP_NO_DI) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\entries.c
DEP_ENTRI=\
	.\src\cvs.h\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
	".\windows-NT\alloca.h"\
	.\lib\fnmatch.h\
	".\windows-NT\pwd.h"\
	.\lib\system.h\
	.\src\hash.h\
	.\src\server.h\
	.\src\client.h\
	.\src\myndbm.h\
	.\lib\regex.h\
	.\lib\getopt.h\
	.\lib\wait.h\
	.\src\rcs.h\
	".\windows-NT\ndir.h"

!IF  "$(CFG)" == "Win32 Release"

.\WinRel\entries.obj :  $(SOURCE)  $(DEP_ENTRI) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

.\WinDebug\entries.obj :  $(SOURCE)  $(DEP_ENTRI) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\tag.c

!IF  "$(CFG)" == "Win32 Release"

.\WinRel\tag.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

.\WinDebug\tag.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\rtag.c

!IF  "$(CFG)" == "Win32 Release"

.\WinRel\rtag.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

.\WinDebug\rtag.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\status.c

!IF  "$(CFG)" == "Win32 Release"

.\WinRel\status.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

.\WinDebug\status.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\root.c

!IF  "$(CFG)" == "Win32 Release"

.\WinRel\root.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

.\WinDebug\root.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\myndbm.c

!IF  "$(CFG)" == "Win32 Release"

.\WinRel\myndbm.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

.\WinDebug\myndbm.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\hash.c
DEP_HASH_=\
	.\src\cvs.h\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
	".\windows-NT\alloca.h"\
	.\lib\fnmatch.h\
	".\windows-NT\pwd.h"\
	.\lib\system.h\
	.\src\hash.h\
	.\src\server.h\
	.\src\client.h\
	.\src\myndbm.h\
	.\lib\regex.h\
	.\lib\getopt.h\
	.\lib\wait.h\
	.\src\rcs.h\
	".\windows-NT\ndir.h"

!IF  "$(CFG)" == "Win32 Release"

.\WinRel\hash.obj :  $(SOURCE)  $(DEP_HASH_) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

.\WinDebug\hash.obj :  $(SOURCE)  $(DEP_HASH_) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\repos.c

!IF  "$(CFG)" == "Win32 Release"

.\WinRel\repos.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

.\WinDebug\repos.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\parseinfo.c

!IF  "$(CFG)" == "Win32 Release"

.\WinRel\parseinfo.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

.\WinDebug\parseinfo.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\vers_ts.c

!IF  "$(CFG)" == "Win32 Release"

.\WinRel\vers_ts.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

.\WinDebug\vers_ts.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\checkin.c

!IF  "$(CFG)" == "Win32 Release"

.\WinRel\checkin.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

.\WinDebug\checkin.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\commit.c
DEP_COMMI=\
	.\src\cvs.h\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
	".\windows-NT\alloca.h"\
	.\lib\fnmatch.h\
	".\windows-NT\pwd.h"\
	.\lib\system.h\
	.\src\hash.h\
	.\src\server.h\
	.\src\client.h\
	.\src\myndbm.h\
	.\lib\regex.h\
	.\lib\getopt.h\
	.\lib\wait.h\
	.\src\rcs.h\
	".\windows-NT\ndir.h"

!IF  "$(CFG)" == "Win32 Release"

.\WinRel\commit.obj :  $(SOURCE)  $(DEP_COMMI) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

.\WinDebug\commit.obj :  $(SOURCE)  $(DEP_COMMI) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\version.c

!IF  "$(CFG)" == "Win32 Release"

.\WinRel\version.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

.\WinDebug\version.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\cvsrc.c

!IF  "$(CFG)" == "Win32 Release"

.\WinRel\cvsrc.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

.\WinDebug\cvsrc.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\remove.c

!IF  "$(CFG)" == "Win32 Release"

.\WinRel\remove.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

.\WinDebug\remove.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\update.c

!IF  "$(CFG)" == "Win32 Release"

.\WinRel\update.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

.\WinDebug\update.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\logmsg.c

!IF  "$(CFG)" == "Win32 Release"

.\WinRel\logmsg.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

.\WinDebug\logmsg.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\classify.c

!IF  "$(CFG)" == "Win32 Release"

.\WinRel\classify.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

.\WinDebug\classify.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\history.c
DEP_HISTO=\
	.\src\cvs.h\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
	".\windows-NT\alloca.h"\
	.\lib\fnmatch.h\
	".\windows-NT\pwd.h"\
	.\lib\system.h\
	.\src\hash.h\
	.\src\server.h\
	.\src\client.h\
	.\src\myndbm.h\
	.\lib\regex.h\
	.\lib\getopt.h\
	.\lib\wait.h\
	.\src\rcs.h\
	".\windows-NT\ndir.h"

!IF  "$(CFG)" == "Win32 Release"

.\WinRel\history.obj :  $(SOURCE)  $(DEP_HISTO) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

.\WinDebug\history.obj :  $(SOURCE)  $(DEP_HISTO) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\add.c
DEP_ADD_C=\
	.\src\cvs.h\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
	".\windows-NT\alloca.h"\
	.\lib\fnmatch.h\
	".\windows-NT\pwd.h"\
	.\lib\system.h\
	.\src\hash.h\
	.\src\server.h\
	.\src\client.h\
	.\src\myndbm.h\
	.\lib\regex.h\
	.\lib\getopt.h\
	.\lib\wait.h\
	.\src\rcs.h\
	".\windows-NT\ndir.h"

!IF  "$(CFG)" == "Win32 Release"

.\WinRel\add.obj :  $(SOURCE)  $(DEP_ADD_C) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

.\WinDebug\add.obj :  $(SOURCE)  $(DEP_ADD_C) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\lock.c
DEP_LOCK_=\
	.\src\cvs.h\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
	".\windows-NT\alloca.h"\
	.\lib\fnmatch.h\
	".\windows-NT\pwd.h"\
	.\lib\system.h\
	.\src\hash.h\
	.\src\server.h\
	.\src\client.h\
	.\src\myndbm.h\
	.\lib\regex.h\
	.\lib\getopt.h\
	.\lib\wait.h\
	.\src\rcs.h\
	".\windows-NT\ndir.h"

!IF  "$(CFG)" == "Win32 Release"

.\WinRel\lock.obj :  $(SOURCE)  $(DEP_LOCK_) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

.\WinDebug\lock.obj :  $(SOURCE)  $(DEP_LOCK_) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\recurse.c

!IF  "$(CFG)" == "Win32 Release"

.\WinRel\recurse.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

.\WinDebug\recurse.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\modules.c
DEP_MODUL=\
	.\src\cvs.h\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
	".\windows-NT\alloca.h"\
	.\lib\fnmatch.h\
	".\windows-NT\pwd.h"\
	.\lib\system.h\
	.\src\hash.h\
	.\src\server.h\
	.\src\client.h\
	.\src\myndbm.h\
	.\lib\regex.h\
	.\lib\getopt.h\
	.\lib\wait.h\
	.\src\rcs.h\
	".\windows-NT\ndir.h"

!IF  "$(CFG)" == "Win32 Release"

.\WinRel\modules.obj :  $(SOURCE)  $(DEP_MODUL) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

.\WinDebug\modules.obj :  $(SOURCE)  $(DEP_MODUL) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\find_names.c
DEP_FIND_=\
	.\src\cvs.h\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
	".\windows-NT\alloca.h"\
	.\lib\fnmatch.h\
	".\windows-NT\pwd.h"\
	.\lib\system.h\
	.\src\hash.h\
	.\src\server.h\
	.\src\client.h\
	.\src\myndbm.h\
	.\lib\regex.h\
	.\lib\getopt.h\
	.\lib\wait.h\
	.\src\rcs.h\
	".\windows-NT\ndir.h"

!IF  "$(CFG)" == "Win32 Release"

.\WinRel\find_names.obj :  $(SOURCE)  $(DEP_FIND_) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

.\WinDebug\find_names.obj :  $(SOURCE)  $(DEP_FIND_) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\rcs.c

!IF  "$(CFG)" == "Win32 Release"

.\WinRel\rcs.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

.\WinDebug\rcs.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\create_adm.c

!IF  "$(CFG)" == "Win32 Release"

.\WinRel\create_adm.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

.\WinDebug\create_adm.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\main.c
DEP_MAIN_=\
	.\src\cvs.h\
	.\src\patchlevel.h\
	".\windows-NT\pwd.h"\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
	".\windows-NT\alloca.h"\
	.\lib\fnmatch.h\
	.\lib\system.h\
	.\src\hash.h\
	.\src\server.h\
	.\src\client.h\
	.\src\myndbm.h\
	.\lib\regex.h\
	.\lib\getopt.h\
	.\lib\wait.h\
	.\src\rcs.h\
	".\windows-NT\ndir.h"

!IF  "$(CFG)" == "Win32 Release"

.\WinRel\main.obj :  $(SOURCE)  $(DEP_MAIN_) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

.\WinDebug\main.obj :  $(SOURCE)  $(DEP_MAIN_) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\patch.c
DEP_PATCH=\
	.\src\cvs.h\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
	".\windows-NT\alloca.h"\
	.\lib\fnmatch.h\
	".\windows-NT\pwd.h"\
	.\lib\system.h\
	.\src\hash.h\
	.\src\server.h\
	.\src\client.h\
	.\src\myndbm.h\
	.\lib\regex.h\
	.\lib\getopt.h\
	.\lib\wait.h\
	.\src\rcs.h\
	".\windows-NT\ndir.h"

!IF  "$(CFG)" == "Win32 Release"

.\WinRel\patch.obj :  $(SOURCE)  $(DEP_PATCH) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

.\WinDebug\patch.obj :  $(SOURCE)  $(DEP_PATCH) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\release.c

!IF  "$(CFG)" == "Win32 Release"

.\WinRel\release.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

.\WinDebug\release.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\rcscmds.c

!IF  "$(CFG)" == "Win32 Release"

.\WinRel\rcscmds.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

.\WinDebug\rcscmds.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\import.c
DEP_IMPOR=\
	.\src\cvs.h\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
	".\windows-NT\alloca.h"\
	.\lib\fnmatch.h\
	".\windows-NT\pwd.h"\
	.\lib\system.h\
	.\src\hash.h\
	.\src\server.h\
	.\src\client.h\
	.\src\myndbm.h\
	.\lib\regex.h\
	.\lib\getopt.h\
	.\lib\wait.h\
	.\src\rcs.h\
	".\windows-NT\ndir.h"

!IF  "$(CFG)" == "Win32 Release"

.\WinRel\import.obj :  $(SOURCE)  $(DEP_IMPOR) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

.\WinDebug\import.obj :  $(SOURCE)  $(DEP_IMPOR) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\ignore.c

!IF  "$(CFG)" == "Win32 Release"

.\WinRel\ignore.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

.\WinDebug\ignore.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\log.c

!IF  "$(CFG)" == "Win32 Release"

.\WinRel\log.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

.\WinDebug\log.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\wrapper.c
DEP_WRAPP=\
	.\src\cvs.h\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
	".\windows-NT\alloca.h"\
	.\lib\fnmatch.h\
	".\windows-NT\pwd.h"\
	.\lib\system.h\
	.\src\hash.h\
	.\src\server.h\
	.\src\client.h\
	.\src\myndbm.h\
	.\lib\regex.h\
	.\lib\getopt.h\
	.\lib\wait.h\
	.\src\rcs.h\
	".\windows-NT\ndir.h"

!IF  "$(CFG)" == "Win32 Release"

.\WinRel\wrapper.obj :  $(SOURCE)  $(DEP_WRAPP) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

.\WinDebug\wrapper.obj :  $(SOURCE)  $(DEP_WRAPP) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
# End Group
################################################################################
# Begin Group "lib"

################################################################################
# Begin Source File

SOURCE=.\lib\getwd.c

!IF  "$(CFG)" == "Win32 Release"

.\WinRel\getwd.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

.\WinDebug\getwd.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\lib\error.c

!IF  "$(CFG)" == "Win32 Release"

.\WinRel\error.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

.\WinDebug\error.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\lib\sighandle.c

!IF  "$(CFG)" == "Win32 Release"

.\WinRel\sighandle.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

.\WinDebug\sighandle.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\lib\getopt.c

!IF  "$(CFG)" == "Win32 Release"

.\WinRel\getopt.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

.\WinDebug\getopt.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\lib\argmatch.c

!IF  "$(CFG)" == "Win32 Release"

.\WinRel\argmatch.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

.\WinDebug\argmatch.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\lib\md5.c

!IF  "$(CFG)" == "Win32 Release"

.\WinRel\md5.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

.\WinDebug\md5.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\lib\yesno.c

!IF  "$(CFG)" == "Win32 Release"

.\WinRel\yesno.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

.\WinDebug\yesno.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\lib\getopt1.c

!IF  "$(CFG)" == "Win32 Release"

.\WinRel\getopt1.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

.\WinDebug\getopt1.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\lib\valloc.c

!IF  "$(CFG)" == "Win32 Release"

.\WinRel\valloc.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

.\WinDebug\valloc.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\lib\xgetwd.c

!IF  "$(CFG)" == "Win32 Release"

.\WinRel\xgetwd.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

.\WinDebug\xgetwd.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\lib\regex.c

!IF  "$(CFG)" == "Win32 Release"

.\WinRel\regex.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

.\WinDebug\regex.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\lib\fnmatch.c

!IF  "$(CFG)" == "Win32 Release"

.\WinRel\fnmatch.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

.\WinDebug\fnmatch.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\lib\getdate.c

!IF  "$(CFG)" == "Win32 Release"

.\WinRel\getdate.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

.\WinDebug\getdate.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=".\lib\save-cwd.c"
DEP_SAVE_=\
	".\windows-NT\config.h"\
	".\lib\save-cwd.h"\
	.\lib\error.h

!IF  "$(CFG)" == "Win32 Release"

".\WinRel\save-cwd.obj" :  $(SOURCE)  $(DEP_SAVE_) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

".\WinDebug\save-cwd.obj" :  $(SOURCE)  $(DEP_SAVE_) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
# End Group
################################################################################
# Begin Group "windows-NT"

################################################################################
# Begin Source File

SOURCE=".\windows-NT\mkdir.c"

!IF  "$(CFG)" == "Win32 Release"

.\WinRel\mkdir.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

.\WinDebug\mkdir.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=".\windows-NT\run.c"

!IF  "$(CFG)" == "Win32 Release"

.\WinRel\run.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

.\WinDebug\run.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=".\windows-NT\pwd.c"
DEP_PWD_C=\
	".\windows-NT\pwd.h"

!IF  "$(CFG)" == "Win32 Release"

.\WinRel\pwd.obj :  $(SOURCE)  $(DEP_PWD_C) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

.\WinDebug\pwd.obj :  $(SOURCE)  $(DEP_PWD_C) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=".\windows-NT\filesubr.c"

!IF  "$(CFG)" == "Win32 Release"

.\WinRel\filesubr.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

.\WinDebug\filesubr.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=".\windows-NT\win32.c"
DEP_WIN32=\
	".\windows-NT\config.h"

!IF  "$(CFG)" == "Win32 Release"

.\WinRel\win32.obj :  $(SOURCE)  $(DEP_WIN32) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

.\WinDebug\win32.obj :  $(SOURCE)  $(DEP_WIN32) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=".\windows-NT\waitpid.c"

!IF  "$(CFG)" == "Win32 Release"

.\WinRel\waitpid.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

.\WinDebug\waitpid.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=".\windows-NT\ndir.c"
DEP_NDIR_=\
	".\windows-NT\ndir.h"

!IF  "$(CFG)" == "Win32 Release"

.\WinRel\ndir.obj :  $(SOURCE)  $(DEP_NDIR_) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

.\WinDebug\ndir.obj :  $(SOURCE)  $(DEP_NDIR_) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=".\windows-NT\strippath.c"
DEP_STRIP=\
	".\windows-NT\config.h"

!IF  "$(CFG)" == "Win32 Release"

.\WinRel\strippath.obj :  $(SOURCE)  $(DEP_STRIP) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

.\WinDebug\strippath.obj :  $(SOURCE)  $(DEP_STRIP) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=".\windows-NT\stripslash.c"
DEP_STRIPS=\
	".\windows-NT\config.h"

!IF  "$(CFG)" == "Win32 Release"

.\WinRel\stripslash.obj :  $(SOURCE)  $(DEP_STRIPS) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

.\WinDebug\stripslash.obj :  $(SOURCE)  $(DEP_STRIPS) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=".\windows-NT\rcmd.c"
DEP_RCMD_=\
	".\windows-NT\rcmd.h"

!IF  "$(CFG)" == "Win32 Release"

.\WinRel\rcmd.obj :  $(SOURCE)  $(DEP_RCMD_) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

.\WinDebug\rcmd.obj :  $(SOURCE)  $(DEP_RCMD_) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=".\windows-NT\startserver.c"
DEP_START=\
	.\src\cvs.h\
	".\windows-NT\rcmd.h"\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
	".\windows-NT\alloca.h"\
	.\lib\fnmatch.h\
	".\windows-NT\pwd.h"\
	.\lib\system.h\
	.\src\hash.h\
	.\src\server.h\
	.\src\client.h\
	.\src\myndbm.h\
	.\lib\regex.h\
	.\lib\getopt.h\
	.\lib\wait.h\
	.\src\rcs.h\
	".\windows-NT\ndir.h"

!IF  "$(CFG)" == "Win32 Release"

.\WinRel\startserver.obj :  $(SOURCE)  $(DEP_START) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Debug"

.\WinDebug\startserver.obj :  $(SOURCE)  $(DEP_START) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
# End Group
# End Project
################################################################################
