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

ALL : $(OUTDIR)/cvs.exe $(OUTDIR)/cvsnt.bsc

$(OUTDIR) : 
    if not exist $(OUTDIR)/nul mkdir $(OUTDIR)

# ADD BASE CPP /nologo /W3 /GX /YX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /FR /c
# ADD CPP /nologo /W3 /GX /YX /Ob1 /I "windows-NT" /I "lib" /I "src" /I "zlib" /D "NDEBUG" /D "WIN32" /D "_CONSOLE" /D "HAVE_CONFIG_H" /FR /c
# SUBTRACT CPP /WX
CPP_PROJ=/nologo /W3 /GX /YX /Ob1 /I "windows-NT" /I "lib" /I "src" /I "zlib" /D "NDEBUG"\
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
	$(INTDIR)/mkmodules.sbr \
	$(INTDIR)/subr.sbr \
	$(INTDIR)/admin.sbr \
	$(INTDIR)/server.sbr \
	$(INTDIR)/diff.sbr \
	$(INTDIR)/client.sbr \
	$(INTDIR)/checkout.sbr \
	$(INTDIR)/no_diff.sbr \
	$(INTDIR)/entries.sbr \
	$(INTDIR)/tag.sbr \
	$(INTDIR)/rtag.sbr \
	$(INTDIR)/status.sbr \
	$(INTDIR)/root.sbr \
	$(INTDIR)/myndbm.sbr \
	$(INTDIR)/hash.sbr \
	$(INTDIR)/repos.sbr \
	$(INTDIR)/parseinfo.sbr \
	$(INTDIR)/vers_ts.sbr \
	$(INTDIR)/checkin.sbr \
	$(INTDIR)/commit.sbr \
	$(INTDIR)/version.sbr \
	$(INTDIR)/cvsrc.sbr \
	$(INTDIR)/remove.sbr \
	$(INTDIR)/update.sbr \
	$(INTDIR)/logmsg.sbr \
	$(INTDIR)/classify.sbr \
	$(INTDIR)/history.sbr \
	$(INTDIR)/add.sbr \
	$(INTDIR)/lock.sbr \
	$(INTDIR)/recurse.sbr \
	$(INTDIR)/modules.sbr \
	$(INTDIR)/find_names.sbr \
	$(INTDIR)/rcs.sbr \
	$(INTDIR)/create_adm.sbr \
	$(INTDIR)/main.sbr \
	$(INTDIR)/patch.sbr \
	$(INTDIR)/release.sbr \
	$(INTDIR)/rcscmds.sbr \
	$(INTDIR)/import.sbr \
	$(INTDIR)/ignore.sbr \
	$(INTDIR)/log.sbr \
	$(INTDIR)/wrapper.sbr \
	$(INTDIR)/error.sbr \
	$(INTDIR)/expand_path.sbr \
	$(INTDIR)/edit.sbr \
	$(INTDIR)/fileattr.sbr \
	$(INTDIR)/watch.sbr \
	$(INTDIR)/login.sbr \
	$(INTDIR)/scramble.sbr \
	$(INTDIR)/buffer.sbr \
	$(INTDIR)/zlib.sbr \
	$(INTDIR)/getwd.sbr \
	$(INTDIR)/sighandle.sbr \
	$(INTDIR)/getopt.sbr \
	$(INTDIR)/argmatch.sbr \
	$(INTDIR)/md5.sbr \
	$(INTDIR)/yesno.sbr \
	$(INTDIR)/getopt1.sbr \
	$(INTDIR)/valloc.sbr \
	$(INTDIR)/xgetwd.sbr \
	$(INTDIR)/regex.sbr \
	$(INTDIR)/fnmatch.sbr \
	$(INTDIR)/getdate.sbr \
	$(INTDIR)/getline.sbr \
	$(INTDIR)/savecwd.sbr \
	$(INTDIR)/vasprintf.sbr \
	$(INTDIR)/mkdir.sbr \
	$(INTDIR)/run.sbr \
	$(INTDIR)/pwd.sbr \
	$(INTDIR)/filesubr.sbr \
	$(INTDIR)/win32.sbr \
	$(INTDIR)/waitpid.sbr \
	$(INTDIR)/ndir.sbr \
	$(INTDIR)/strippath.sbr \
	$(INTDIR)/stripslash.sbr \
	$(INTDIR)/rcmd.sbr \
	$(INTDIR)/startserver.sbr \
	$(INTDIR)/zutil.sbr \
	$(INTDIR)/infutil.sbr \
	$(INTDIR)/infblock.sbr \
	$(INTDIR)/compress.sbr \
	$(INTDIR)/uncompr.sbr \
	$(INTDIR)/inflate.sbr \
	$(INTDIR)/inftrees.sbr \
	$(INTDIR)/gzio.sbr \
	$(INTDIR)/infcodes.sbr \
	$(INTDIR)/deflate.sbr \
	$(INTDIR)/adler32.sbr \
	$(INTDIR)/crc32.sbr \
	$(INTDIR)/inffast.sbr \
	$(INTDIR)/trees.sbr

$(OUTDIR)/cvsnt.bsc : $(OUTDIR)  $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LINK32=link.exe
# ADD BASE LINK32 advapi32.lib /NOLOGO /SUBSYSTEM:console /MACHINE:I386
# ADD LINK32 advapi32.lib wsock32.lib /NOLOGO /SUBSYSTEM:console /MACHINE:I386 /OUT:"WinRel/cvs.exe"
LINK32_FLAGS=advapi32.lib wsock32.lib /NOLOGO /SUBSYSTEM:console\
 /INCREMENTAL:no /PDB:$(OUTDIR)/"cvsnt.pdb" /MACHINE:I386 /OUT:"WinRel/cvs.exe" 
DEF_FILE=
LINK32_OBJS= \
	$(INTDIR)/mkmodules.obj \
	$(INTDIR)/subr.obj \
	$(INTDIR)/admin.obj \
	$(INTDIR)/server.obj \
	$(INTDIR)/diff.obj \
	$(INTDIR)/client.obj \
	$(INTDIR)/checkout.obj \
	$(INTDIR)/no_diff.obj \
	$(INTDIR)/entries.obj \
	$(INTDIR)/tag.obj \
	$(INTDIR)/rtag.obj \
	$(INTDIR)/status.obj \
	$(INTDIR)/root.obj \
	$(INTDIR)/myndbm.obj \
	$(INTDIR)/hash.obj \
	$(INTDIR)/repos.obj \
	$(INTDIR)/parseinfo.obj \
	$(INTDIR)/vers_ts.obj \
	$(INTDIR)/checkin.obj \
	$(INTDIR)/commit.obj \
	$(INTDIR)/version.obj \
	$(INTDIR)/cvsrc.obj \
	$(INTDIR)/remove.obj \
	$(INTDIR)/update.obj \
	$(INTDIR)/logmsg.obj \
	$(INTDIR)/classify.obj \
	$(INTDIR)/history.obj \
	$(INTDIR)/add.obj \
	$(INTDIR)/lock.obj \
	$(INTDIR)/recurse.obj \
	$(INTDIR)/modules.obj \
	$(INTDIR)/find_names.obj \
	$(INTDIR)/rcs.obj \
	$(INTDIR)/create_adm.obj \
	$(INTDIR)/main.obj \
	$(INTDIR)/patch.obj \
	$(INTDIR)/release.obj \
	$(INTDIR)/rcscmds.obj \
	$(INTDIR)/import.obj \
	$(INTDIR)/ignore.obj \
	$(INTDIR)/log.obj \
	$(INTDIR)/wrapper.obj \
	$(INTDIR)/error.obj \
	$(INTDIR)/expand_path.obj \
	$(INTDIR)/edit.obj \
	$(INTDIR)/fileattr.obj \
	$(INTDIR)/watch.obj \
	$(INTDIR)/login.obj \
	$(INTDIR)/scramble.obj \
	$(INTDIR)/buffer.obj \
	$(INTDIR)/zlib.obj \
	$(INTDIR)/getwd.obj \
	$(INTDIR)/sighandle.obj \
	$(INTDIR)/getopt.obj \
	$(INTDIR)/argmatch.obj \
	$(INTDIR)/md5.obj \
	$(INTDIR)/yesno.obj \
	$(INTDIR)/getopt1.obj \
	$(INTDIR)/valloc.obj \
	$(INTDIR)/xgetwd.obj \
	$(INTDIR)/regex.obj \
	$(INTDIR)/fnmatch.obj \
	$(INTDIR)/getdate.obj \
	$(INTDIR)/getline.obj \
	$(INTDIR)/savecwd.obj \
	$(INTDIR)/vasprintf.obj \
	$(INTDIR)/mkdir.obj \
	$(INTDIR)/run.obj \
	$(INTDIR)/pwd.obj \
	$(INTDIR)/filesubr.obj \
	$(INTDIR)/win32.obj \
	$(INTDIR)/waitpid.obj \
	$(INTDIR)/ndir.obj \
	$(INTDIR)/strippath.obj \
	$(INTDIR)/stripslash.obj \
	$(INTDIR)/rcmd.obj \
	$(INTDIR)/startserver.obj \
	$(INTDIR)/zutil.obj \
	$(INTDIR)/infutil.obj \
	$(INTDIR)/infblock.obj \
	$(INTDIR)/compress.obj \
	$(INTDIR)/uncompr.obj \
	$(INTDIR)/inflate.obj \
	$(INTDIR)/inftrees.obj \
	$(INTDIR)/gzio.obj \
	$(INTDIR)/infcodes.obj \
	$(INTDIR)/deflate.obj \
	$(INTDIR)/adler32.obj \
	$(INTDIR)/crc32.obj \
	$(INTDIR)/inffast.obj \
	$(INTDIR)/trees.obj

$(OUTDIR)/cvs.exe : $(OUTDIR)  $(DEF_FILE) $(LINK32_OBJS)
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

ALL : $(OUTDIR)/cvs.exe $(OUTDIR)/cvsnt.bsc

$(OUTDIR) : 
    if not exist $(OUTDIR)/nul mkdir $(OUTDIR)

# ADD BASE CPP /nologo /W3 /GX /Zi /YX /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /FR /c
# ADD CPP /nologo /W3 /GX /Zi /YX /Ob1 /I "windows-NT" /I "lib" /I "src" /I "zlib" /D "_DEBUG" /D "WIN32" /D "_CONSOLE" /D "HAVE_CONFIG_H" /FR /c
CPP_PROJ=/nologo /W3 /GX /Zi /YX /Ob1 /I "windows-NT" /I "lib" /I "src" /I\
 "zlib" /D "_DEBUG" /D "WIN32" /D "_CONSOLE" /D "HAVE_CONFIG_H" /FR$(INTDIR)/\
 /Fp$(OUTDIR)/"cvsnt.pch" /Fo$(INTDIR)/ /Fd$(OUTDIR)/"cvsnt.pdb" /c 
CPP_OBJS=.\WinDebug/
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o$(OUTDIR)/"cvsnt.bsc" 
BSC32_SBRS= \
	$(INTDIR)/mkmodules.sbr \
	$(INTDIR)/subr.sbr \
	$(INTDIR)/admin.sbr \
	$(INTDIR)/server.sbr \
	$(INTDIR)/diff.sbr \
	$(INTDIR)/client.sbr \
	$(INTDIR)/checkout.sbr \
	$(INTDIR)/no_diff.sbr \
	$(INTDIR)/entries.sbr \
	$(INTDIR)/tag.sbr \
	$(INTDIR)/rtag.sbr \
	$(INTDIR)/status.sbr \
	$(INTDIR)/root.sbr \
	$(INTDIR)/myndbm.sbr \
	$(INTDIR)/hash.sbr \
	$(INTDIR)/repos.sbr \
	$(INTDIR)/parseinfo.sbr \
	$(INTDIR)/vers_ts.sbr \
	$(INTDIR)/checkin.sbr \
	$(INTDIR)/commit.sbr \
	$(INTDIR)/version.sbr \
	$(INTDIR)/cvsrc.sbr \
	$(INTDIR)/remove.sbr \
	$(INTDIR)/update.sbr \
	$(INTDIR)/logmsg.sbr \
	$(INTDIR)/classify.sbr \
	$(INTDIR)/history.sbr \
	$(INTDIR)/add.sbr \
	$(INTDIR)/lock.sbr \
	$(INTDIR)/recurse.sbr \
	$(INTDIR)/modules.sbr \
	$(INTDIR)/find_names.sbr \
	$(INTDIR)/rcs.sbr \
	$(INTDIR)/create_adm.sbr \
	$(INTDIR)/main.sbr \
	$(INTDIR)/patch.sbr \
	$(INTDIR)/release.sbr \
	$(INTDIR)/rcscmds.sbr \
	$(INTDIR)/import.sbr \
	$(INTDIR)/ignore.sbr \
	$(INTDIR)/log.sbr \
	$(INTDIR)/wrapper.sbr \
	$(INTDIR)/error.sbr \
	$(INTDIR)/expand_path.sbr \
	$(INTDIR)/edit.sbr \
	$(INTDIR)/fileattr.sbr \
	$(INTDIR)/watch.sbr \
	$(INTDIR)/login.sbr \
	$(INTDIR)/scramble.sbr \
	$(INTDIR)/buffer.sbr \
	$(INTDIR)/zlib.sbr \
	$(INTDIR)/getwd.sbr \
	$(INTDIR)/sighandle.sbr \
	$(INTDIR)/getopt.sbr \
	$(INTDIR)/argmatch.sbr \
	$(INTDIR)/md5.sbr \
	$(INTDIR)/yesno.sbr \
	$(INTDIR)/getopt1.sbr \
	$(INTDIR)/valloc.sbr \
	$(INTDIR)/xgetwd.sbr \
	$(INTDIR)/regex.sbr \
	$(INTDIR)/fnmatch.sbr \
	$(INTDIR)/getdate.sbr \
	$(INTDIR)/getline.sbr \
	$(INTDIR)/savecwd.sbr \
	$(INTDIR)/vasprintf.sbr \
	$(INTDIR)/mkdir.sbr \
	$(INTDIR)/run.sbr \
	$(INTDIR)/pwd.sbr \
	$(INTDIR)/filesubr.sbr \
	$(INTDIR)/win32.sbr \
	$(INTDIR)/waitpid.sbr \
	$(INTDIR)/ndir.sbr \
	$(INTDIR)/strippath.sbr \
	$(INTDIR)/stripslash.sbr \
	$(INTDIR)/rcmd.sbr \
	$(INTDIR)/startserver.sbr \
	$(INTDIR)/zutil.sbr \
	$(INTDIR)/infutil.sbr \
	$(INTDIR)/infblock.sbr \
	$(INTDIR)/compress.sbr \
	$(INTDIR)/uncompr.sbr \
	$(INTDIR)/inflate.sbr \
	$(INTDIR)/inftrees.sbr \
	$(INTDIR)/gzio.sbr \
	$(INTDIR)/infcodes.sbr \
	$(INTDIR)/deflate.sbr \
	$(INTDIR)/adler32.sbr \
	$(INTDIR)/crc32.sbr \
	$(INTDIR)/inffast.sbr \
	$(INTDIR)/trees.sbr

$(OUTDIR)/cvsnt.bsc : $(OUTDIR)  $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LINK32=link.exe
# ADD BASE LINK32 advapi32.lib /NOLOGO /SUBSYSTEM:console /DEBUG /MACHINE:I386
# ADD LINK32 advapi32.lib wsock32.lib /NOLOGO /SUBSYSTEM:console /DEBUG /MACHINE:I386 /OUT:"WinDebug/cvs.exe"
LINK32_FLAGS=advapi32.lib wsock32.lib /NOLOGO /SUBSYSTEM:console\
 /INCREMENTAL:yes /PDB:$(OUTDIR)/"cvsnt.pdb" /DEBUG /MACHINE:I386\
 /OUT:"WinDebug/cvs.exe" 
DEF_FILE=
LINK32_OBJS= \
	$(INTDIR)/mkmodules.obj \
	$(INTDIR)/subr.obj \
	$(INTDIR)/admin.obj \
	$(INTDIR)/server.obj \
	$(INTDIR)/diff.obj \
	$(INTDIR)/client.obj \
	$(INTDIR)/checkout.obj \
	$(INTDIR)/no_diff.obj \
	$(INTDIR)/entries.obj \
	$(INTDIR)/tag.obj \
	$(INTDIR)/rtag.obj \
	$(INTDIR)/status.obj \
	$(INTDIR)/root.obj \
	$(INTDIR)/myndbm.obj \
	$(INTDIR)/hash.obj \
	$(INTDIR)/repos.obj \
	$(INTDIR)/parseinfo.obj \
	$(INTDIR)/vers_ts.obj \
	$(INTDIR)/checkin.obj \
	$(INTDIR)/commit.obj \
	$(INTDIR)/version.obj \
	$(INTDIR)/cvsrc.obj \
	$(INTDIR)/remove.obj \
	$(INTDIR)/update.obj \
	$(INTDIR)/logmsg.obj \
	$(INTDIR)/classify.obj \
	$(INTDIR)/history.obj \
	$(INTDIR)/add.obj \
	$(INTDIR)/lock.obj \
	$(INTDIR)/recurse.obj \
	$(INTDIR)/modules.obj \
	$(INTDIR)/find_names.obj \
	$(INTDIR)/rcs.obj \
	$(INTDIR)/create_adm.obj \
	$(INTDIR)/main.obj \
	$(INTDIR)/patch.obj \
	$(INTDIR)/release.obj \
	$(INTDIR)/rcscmds.obj \
	$(INTDIR)/import.obj \
	$(INTDIR)/ignore.obj \
	$(INTDIR)/log.obj \
	$(INTDIR)/wrapper.obj \
	$(INTDIR)/error.obj \
	$(INTDIR)/expand_path.obj \
	$(INTDIR)/edit.obj \
	$(INTDIR)/fileattr.obj \
	$(INTDIR)/watch.obj \
	$(INTDIR)/login.obj \
	$(INTDIR)/scramble.obj \
	$(INTDIR)/buffer.obj \
	$(INTDIR)/zlib.obj \
	$(INTDIR)/getwd.obj \
	$(INTDIR)/sighandle.obj \
	$(INTDIR)/getopt.obj \
	$(INTDIR)/argmatch.obj \
	$(INTDIR)/md5.obj \
	$(INTDIR)/yesno.obj \
	$(INTDIR)/getopt1.obj \
	$(INTDIR)/valloc.obj \
	$(INTDIR)/xgetwd.obj \
	$(INTDIR)/regex.obj \
	$(INTDIR)/fnmatch.obj \
	$(INTDIR)/getdate.obj \
	$(INTDIR)/getline.obj \
	$(INTDIR)/savecwd.obj \
	$(INTDIR)/vasprintf.obj \
	$(INTDIR)/mkdir.obj \
	$(INTDIR)/run.obj \
	$(INTDIR)/pwd.obj \
	$(INTDIR)/filesubr.obj \
	$(INTDIR)/win32.obj \
	$(INTDIR)/waitpid.obj \
	$(INTDIR)/ndir.obj \
	$(INTDIR)/strippath.obj \
	$(INTDIR)/stripslash.obj \
	$(INTDIR)/rcmd.obj \
	$(INTDIR)/startserver.obj \
	$(INTDIR)/zutil.obj \
	$(INTDIR)/infutil.obj \
	$(INTDIR)/infblock.obj \
	$(INTDIR)/compress.obj \
	$(INTDIR)/uncompr.obj \
	$(INTDIR)/inflate.obj \
	$(INTDIR)/inftrees.obj \
	$(INTDIR)/gzio.obj \
	$(INTDIR)/infcodes.obj \
	$(INTDIR)/deflate.obj \
	$(INTDIR)/adler32.obj \
	$(INTDIR)/crc32.obj \
	$(INTDIR)/inffast.obj \
	$(INTDIR)/trees.obj

$(OUTDIR)/cvs.exe : $(OUTDIR)  $(DEF_FILE) $(LINK32_OBJS)
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

SOURCE=.\src\mkmodules.c
DEP_MKMOD=\
	.\src\cvs.h\
	.\lib\savecwd.h\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
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
	.\src\error.h\
	.\src\update.h\
	".\windows-NT\ndir.h"

$(INTDIR)/mkmodules.obj :  $(SOURCE)  $(DEP_MKMOD) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\subr.c
DEP_SUBR_=\
	.\src\cvs.h\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
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
	.\src\error.h\
	.\src\update.h\
	".\windows-NT\ndir.h"

$(INTDIR)/subr.obj :  $(SOURCE)  $(DEP_SUBR_) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\admin.c
DEP_ADMIN=\
	.\src\cvs.h\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
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
	.\src\error.h\
	.\src\update.h\
	".\windows-NT\ndir.h"

$(INTDIR)/admin.obj :  $(SOURCE)  $(DEP_ADMIN) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\server.c
DEP_SERVE=\
	.\src\cvs.h\
	.\src\watch.h\
	.\src\edit.h\
	.\src\fileattr.h\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
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
	.\src\error.h\
	.\src\update.h\
	".\windows-NT\ndir.h"

$(INTDIR)/server.obj :  $(SOURCE)  $(DEP_SERVE) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\diff.c
DEP_DIFF_=\
	.\src\cvs.h\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
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
	.\src\error.h\
	.\src\update.h\
	".\windows-NT\ndir.h"

$(INTDIR)/diff.obj :  $(SOURCE)  $(DEP_DIFF_) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\client.c
DEP_CLIEN=\
	".\windows-NT\config.h"\
	.\src\cvs.h\
	.\lib\getline.h\
	.\src\edit.h\
	.\lib\md5.h\
	".\windows-NT\options.h"\
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
	.\src\error.h\
	.\src\update.h\
	".\windows-NT\ndir.h"

$(INTDIR)/client.obj :  $(SOURCE)  $(DEP_CLIEN) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\checkout.c
DEP_CHECK=\
	.\src\cvs.h\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
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
	.\src\error.h\
	.\src\update.h\
	".\windows-NT\ndir.h"

$(INTDIR)/checkout.obj :  $(SOURCE)  $(DEP_CHECK) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\no_diff.c
DEP_NO_DI=\
	.\src\cvs.h\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
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
	.\src\error.h\
	.\src\update.h\
	".\windows-NT\ndir.h"

$(INTDIR)/no_diff.obj :  $(SOURCE)  $(DEP_NO_DI) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\entries.c
DEP_ENTRI=\
	.\src\cvs.h\
	.\lib\getline.h\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
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
	.\src\error.h\
	.\src\update.h\
	".\windows-NT\ndir.h"

$(INTDIR)/entries.obj :  $(SOURCE)  $(DEP_ENTRI) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\tag.c
DEP_TAG_C=\
	.\src\cvs.h\
	.\lib\savecwd.h\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
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
	.\src\error.h\
	.\src\update.h\
	".\windows-NT\ndir.h"

$(INTDIR)/tag.obj :  $(SOURCE)  $(DEP_TAG_C) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\rtag.c
DEP_RTAG_=\
	.\src\cvs.h\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
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
	.\src\error.h\
	.\src\update.h\
	".\windows-NT\ndir.h"

$(INTDIR)/rtag.obj :  $(SOURCE)  $(DEP_RTAG_) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\status.c
DEP_STATU=\
	.\src\cvs.h\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
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
	.\src\error.h\
	.\src\update.h\
	".\windows-NT\ndir.h"

$(INTDIR)/status.obj :  $(SOURCE)  $(DEP_STATU) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\root.c
DEP_ROOT_=\
	.\src\cvs.h\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
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
	.\src\error.h\
	.\src\update.h\
	".\windows-NT\ndir.h"

$(INTDIR)/root.obj :  $(SOURCE)  $(DEP_ROOT_) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\myndbm.c
DEP_MYNDB=\
	.\src\cvs.h\
	.\lib\getline.h\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
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
	.\src\error.h\
	.\src\update.h\
	".\windows-NT\ndir.h"

$(INTDIR)/myndbm.obj :  $(SOURCE)  $(DEP_MYNDB) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\hash.c
DEP_HASH_=\
	.\src\cvs.h\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
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
	.\src\error.h\
	.\src\update.h\
	".\windows-NT\ndir.h"

$(INTDIR)/hash.obj :  $(SOURCE)  $(DEP_HASH_) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\repos.c
DEP_REPOS=\
	.\src\cvs.h\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
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
	.\src\error.h\
	.\src\update.h\
	".\windows-NT\ndir.h"

$(INTDIR)/repos.obj :  $(SOURCE)  $(DEP_REPOS) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\parseinfo.c
DEP_PARSE=\
	.\src\cvs.h\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
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
	.\src\error.h\
	.\src\update.h\
	".\windows-NT\ndir.h"

$(INTDIR)/parseinfo.obj :  $(SOURCE)  $(DEP_PARSE) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\vers_ts.c
DEP_VERS_=\
	.\src\cvs.h\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
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
	.\src\error.h\
	.\src\update.h\
	".\windows-NT\ndir.h"

$(INTDIR)/vers_ts.obj :  $(SOURCE)  $(DEP_VERS_) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\checkin.c
DEP_CHECKI=\
	.\src\cvs.h\
	.\src\fileattr.h\
	.\src\edit.h\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
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
	.\src\error.h\
	.\src\update.h\
	".\windows-NT\ndir.h"

$(INTDIR)/checkin.obj :  $(SOURCE)  $(DEP_CHECKI) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\commit.c
DEP_COMMI=\
	.\src\cvs.h\
	.\lib\getline.h\
	.\src\edit.h\
	.\src\fileattr.h\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
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
	.\src\error.h\
	.\src\update.h\
	".\windows-NT\ndir.h"

$(INTDIR)/commit.obj :  $(SOURCE)  $(DEP_COMMI) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\version.c
DEP_VERSI=\
	.\src\cvs.h\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
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
	.\src\error.h\
	.\src\update.h\
	".\windows-NT\ndir.h"

$(INTDIR)/version.obj :  $(SOURCE)  $(DEP_VERSI) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\cvsrc.c
DEP_CVSRC=\
	.\src\cvs.h\
	.\lib\getline.h\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
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
	.\src\error.h\
	.\src\update.h\
	".\windows-NT\ndir.h"

$(INTDIR)/cvsrc.obj :  $(SOURCE)  $(DEP_CVSRC) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\remove.c
DEP_REMOV=\
	.\src\cvs.h\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
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
	.\src\error.h\
	.\src\update.h\
	".\windows-NT\ndir.h"

$(INTDIR)/remove.obj :  $(SOURCE)  $(DEP_REMOV) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\update.c
DEP_UPDAT=\
	.\src\cvs.h\
	.\lib\md5.h\
	.\src\watch.h\
	.\src\fileattr.h\
	.\src\edit.h\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
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
	.\src\error.h\
	.\src\update.h\
	".\windows-NT\ndir.h"

$(INTDIR)/update.obj :  $(SOURCE)  $(DEP_UPDAT) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\logmsg.c
DEP_LOGMS=\
	.\src\cvs.h\
	.\lib\getline.h\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
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
	.\src\error.h\
	.\src\update.h\
	".\windows-NT\ndir.h"

$(INTDIR)/logmsg.obj :  $(SOURCE)  $(DEP_LOGMS) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\classify.c
DEP_CLASS=\
	.\src\cvs.h\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
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
	.\src\error.h\
	.\src\update.h\
	".\windows-NT\ndir.h"

$(INTDIR)/classify.obj :  $(SOURCE)  $(DEP_CLASS) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\history.c
DEP_HISTO=\
	.\src\cvs.h\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
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
	.\src\error.h\
	.\src\update.h\
	".\windows-NT\ndir.h"

$(INTDIR)/history.obj :  $(SOURCE)  $(DEP_HISTO) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\add.c
DEP_ADD_C=\
	.\src\cvs.h\
	.\lib\savecwd.h\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
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
	.\src\error.h\
	.\src\update.h\
	".\windows-NT\ndir.h"

$(INTDIR)/add.obj :  $(SOURCE)  $(DEP_ADD_C) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\lock.c
DEP_LOCK_=\
	.\src\cvs.h\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
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
	.\src\error.h\
	.\src\update.h\
	".\windows-NT\ndir.h"

$(INTDIR)/lock.obj :  $(SOURCE)  $(DEP_LOCK_) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\recurse.c
DEP_RECUR=\
	.\src\cvs.h\
	.\lib\savecwd.h\
	.\src\fileattr.h\
	.\src\edit.h\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
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
	.\src\error.h\
	.\src\update.h\
	".\windows-NT\ndir.h"

$(INTDIR)/recurse.obj :  $(SOURCE)  $(DEP_RECUR) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\modules.c
DEP_MODUL=\
	.\src\cvs.h\
	.\lib\savecwd.h\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
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
	.\src\error.h\
	.\src\update.h\
	".\windows-NT\ndir.h"

$(INTDIR)/modules.obj :  $(SOURCE)  $(DEP_MODUL) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\find_names.c
DEP_FIND_=\
	.\src\cvs.h\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
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
	.\src\error.h\
	.\src\update.h\
	".\windows-NT\ndir.h"

$(INTDIR)/find_names.obj :  $(SOURCE)  $(DEP_FIND_) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\rcs.c
DEP_RCS_C=\
	.\src\cvs.h\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
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
	.\src\error.h\
	.\src\update.h\
	".\windows-NT\ndir.h"

$(INTDIR)/rcs.obj :  $(SOURCE)  $(DEP_RCS_C) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\create_adm.c
DEP_CREAT=\
	.\src\cvs.h\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
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
	.\src\error.h\
	.\src\update.h\
	".\windows-NT\ndir.h"

$(INTDIR)/create_adm.obj :  $(SOURCE)  $(DEP_CREAT) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\main.c
DEP_MAIN_=\
	.\src\cvs.h\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
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
	.\src\error.h\
	.\src\update.h\
	".\windows-NT\ndir.h"

$(INTDIR)/main.obj :  $(SOURCE)  $(DEP_MAIN_) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\patch.c
DEP_PATCH=\
	.\src\cvs.h\
	.\lib\getline.h\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
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
	.\src\error.h\
	.\src\update.h\
	".\windows-NT\ndir.h"

$(INTDIR)/patch.obj :  $(SOURCE)  $(DEP_PATCH) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\release.c
DEP_RELEA=\
	.\src\cvs.h\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
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
	.\src\error.h\
	.\src\update.h\
	".\windows-NT\ndir.h"

$(INTDIR)/release.obj :  $(SOURCE)  $(DEP_RELEA) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\rcscmds.c
DEP_RCSCM=\
	.\src\cvs.h\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
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
	.\src\error.h\
	.\src\update.h\
	".\windows-NT\ndir.h"

$(INTDIR)/rcscmds.obj :  $(SOURCE)  $(DEP_RCSCM) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\import.c
DEP_IMPOR=\
	.\src\cvs.h\
	.\lib\savecwd.h\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
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
	.\src\error.h\
	.\src\update.h\
	".\windows-NT\ndir.h"

$(INTDIR)/import.obj :  $(SOURCE)  $(DEP_IMPOR) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\ignore.c
DEP_IGNOR=\
	.\src\cvs.h\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
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
	.\src\error.h\
	.\src\update.h\
	".\windows-NT\ndir.h"

$(INTDIR)/ignore.obj :  $(SOURCE)  $(DEP_IGNOR) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\log.c
DEP_LOG_C=\
	.\src\cvs.h\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
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
	.\src\error.h\
	.\src\update.h\
	".\windows-NT\ndir.h"

$(INTDIR)/log.obj :  $(SOURCE)  $(DEP_LOG_C) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\wrapper.c
DEP_WRAPP=\
	.\src\cvs.h\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
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
	.\src\error.h\
	.\src\update.h\
	".\windows-NT\ndir.h"

$(INTDIR)/wrapper.obj :  $(SOURCE)  $(DEP_WRAPP) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\error.c
DEP_ERROR=\
	.\src\cvs.h\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
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
	.\src\error.h\
	.\src\update.h\
	".\windows-NT\ndir.h"

$(INTDIR)/error.obj :  $(SOURCE)  $(DEP_ERROR) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\expand_path.c
DEP_EXPAN=\
	.\src\cvs.h\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
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
	.\src\error.h\
	.\src\update.h\
	".\windows-NT\ndir.h"

$(INTDIR)/expand_path.obj :  $(SOURCE)  $(DEP_EXPAN) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\edit.c
DEP_EDIT_=\
	.\src\cvs.h\
	.\lib\getline.h\
	.\src\watch.h\
	.\src\edit.h\
	.\src\fileattr.h\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
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
	.\src\error.h\
	.\src\update.h\
	".\windows-NT\ndir.h"

$(INTDIR)/edit.obj :  $(SOURCE)  $(DEP_EDIT_) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\fileattr.c
DEP_FILEA=\
	.\src\cvs.h\
	.\lib\getline.h\
	.\src\fileattr.h\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
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
	.\src\error.h\
	.\src\update.h\
	".\windows-NT\ndir.h"

$(INTDIR)/fileattr.obj :  $(SOURCE)  $(DEP_FILEA) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\watch.c
DEP_WATCH=\
	.\src\cvs.h\
	.\src\edit.h\
	.\src\fileattr.h\
	.\src\watch.h\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
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
	.\src\error.h\
	.\src\update.h\
	".\windows-NT\ndir.h"

$(INTDIR)/watch.obj :  $(SOURCE)  $(DEP_WATCH) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\login.c
DEP_LOGIN=\
	.\src\cvs.h\
	.\lib\getline.h\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
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
	.\src\error.h\
	.\src\update.h\
	".\windows-NT\ndir.h"

$(INTDIR)/login.obj :  $(SOURCE)  $(DEP_LOGIN) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\scramble.c
DEP_SCRAM=\
	.\src\cvs.h\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
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
	.\src\error.h\
	.\src\update.h\
	".\windows-NT\ndir.h"

$(INTDIR)/scramble.obj :  $(SOURCE)  $(DEP_SCRAM) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\buffer.c
DEP_BUFFE=\
	.\src\cvs.h\
	.\src\buffer.h\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
	.\lib\fnmatch.h\
	".\windows-NT\pwd.h"\
	.\lib\system.h\
	.\src\hash.h\
	.\src\client.h\
	.\src\myndbm.h\
	.\lib\regex.h\
	.\lib\getopt.h\
	.\lib\wait.h\
	.\src\rcs.h\
	.\src\error.h\
	.\src\update.h\
	.\src\server.h\
	".\windows-NT\ndir.h"

$(INTDIR)/buffer.obj :  $(SOURCE)  $(DEP_BUFFE) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\zlib.c
DEP_ZLIB_=\
	.\src\cvs.h\
	.\src\buffer.h\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
	.\lib\fnmatch.h\
	".\windows-NT\pwd.h"\
	.\lib\system.h\
	.\src\hash.h\
	.\src\client.h\
	.\src\myndbm.h\
	.\lib\regex.h\
	.\lib\getopt.h\
	.\lib\wait.h\
	.\src\rcs.h\
	.\src\error.h\
	.\src\update.h\
	.\src\server.h\
	".\windows-NT\ndir.h"

$(INTDIR)/zlib.obj :  $(SOURCE)  $(DEP_ZLIB_) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\ChangeLog
# End Source File
# End Group
################################################################################
# Begin Group "lib"

################################################################################
# Begin Source File

SOURCE=.\lib\getwd.c
DEP_GETWD=\
	".\windows-NT\config.h"\
	.\lib\system.h\
	".\windows-NT\ndir.h"

$(INTDIR)/getwd.obj :  $(SOURCE)  $(DEP_GETWD) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\lib\sighandle.c
DEP_SIGHA=\
	".\windows-NT\config.h"\
	.\lib\system.h\
	".\windows-NT\ndir.h"

$(INTDIR)/sighandle.obj :  $(SOURCE)  $(DEP_SIGHA) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\lib\getopt.c
DEP_GETOP=\
	".\windows-NT\config.h"\
	.\lib\getopt.h

$(INTDIR)/getopt.obj :  $(SOURCE)  $(DEP_GETOP) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\lib\argmatch.c
DEP_ARGMA=\
	".\windows-NT\config.h"

$(INTDIR)/argmatch.obj :  $(SOURCE)  $(DEP_ARGMA) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\lib\md5.c
DEP_MD5_C=\
	".\windows-NT\config.h"\
	.\lib\md5.h

$(INTDIR)/md5.obj :  $(SOURCE)  $(DEP_MD5_C) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\lib\yesno.c
DEP_YESNO=\
	".\windows-NT\config.h"

$(INTDIR)/yesno.obj :  $(SOURCE)  $(DEP_YESNO) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\lib\getopt1.c
DEP_GETOPT=\
	".\windows-NT\config.h"\
	.\lib\getopt.h

$(INTDIR)/getopt1.obj :  $(SOURCE)  $(DEP_GETOPT) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\lib\valloc.c
DEP_VALLO=\
	".\windows-NT\config.h"\
	.\lib\system.h\
	".\windows-NT\ndir.h"

$(INTDIR)/valloc.obj :  $(SOURCE)  $(DEP_VALLO) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\lib\xgetwd.c
DEP_XGETW=\
	".\windows-NT\config.h"\
	.\lib\system.h\
	".\windows-NT\ndir.h"

$(INTDIR)/xgetwd.obj :  $(SOURCE)  $(DEP_XGETW) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\lib\regex.c
DEP_REGEX=\
	".\windows-NT\config.h"\
	.\lib\regex.h

$(INTDIR)/regex.obj :  $(SOURCE)  $(DEP_REGEX) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\lib\fnmatch.c
DEP_FNMAT=\
	".\windows-NT\config.h"\
	.\lib\fnmatch.h

$(INTDIR)/fnmatch.obj :  $(SOURCE)  $(DEP_FNMAT) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\lib\getdate.c
DEP_GETDA=\
	".\windows-NT\config.h"

$(INTDIR)/getdate.obj :  $(SOURCE)  $(DEP_GETDA) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\lib\getline.c
DEP_GETLI=\
	".\windows-NT\config.h"

$(INTDIR)/getline.obj :  $(SOURCE)  $(DEP_GETLI) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\lib\savecwd.c
DEP_SAVEC=\
	".\windows-NT\config.h"\
	.\lib\savecwd.h\
	.\src\error.h

$(INTDIR)/savecwd.obj :  $(SOURCE)  $(DEP_SAVEC) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\lib\vasprintf.c
DEP_VASPR=\
	".\windows-NT\config.h"

$(INTDIR)/vasprintf.obj :  $(SOURCE)  $(DEP_VASPR) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
# End Group
################################################################################
# Begin Group "windows-NT"

################################################################################
# Begin Source File

SOURCE=".\windows-NT\mkdir.c"
DEP_MKDIR=\
	.\src\cvs.h\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
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
	.\src\error.h\
	.\src\update.h\
	".\windows-NT\ndir.h"

$(INTDIR)/mkdir.obj :  $(SOURCE)  $(DEP_MKDIR) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=".\windows-NT\run.c"
DEP_RUN_C=\
	.\src\cvs.h\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
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
	.\src\error.h\
	.\src\update.h\
	".\windows-NT\ndir.h"

$(INTDIR)/run.obj :  $(SOURCE)  $(DEP_RUN_C) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=".\windows-NT\pwd.c"
DEP_PWD_C=\
	".\windows-NT\pwd.h"

$(INTDIR)/pwd.obj :  $(SOURCE)  $(DEP_PWD_C) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=".\windows-NT\filesubr.c"
DEP_FILES=\
	.\src\cvs.h\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
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
	.\src\error.h\
	.\src\update.h\
	".\windows-NT\ndir.h"

$(INTDIR)/filesubr.obj :  $(SOURCE)  $(DEP_FILES) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=".\windows-NT\win32.c"
DEP_WIN32=\
	".\windows-NT\config.h"

$(INTDIR)/win32.obj :  $(SOURCE)  $(DEP_WIN32) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=".\windows-NT\waitpid.c"
DEP_WAITP=\
	".\windows-NT\config.h"

$(INTDIR)/waitpid.obj :  $(SOURCE)  $(DEP_WAITP) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=".\windows-NT\ndir.c"
DEP_NDIR_=\
	".\windows-NT\ndir.h"

$(INTDIR)/ndir.obj :  $(SOURCE)  $(DEP_NDIR_) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=".\windows-NT\strippath.c"

$(INTDIR)/strippath.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=".\windows-NT\stripslash.c"

$(INTDIR)/stripslash.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=".\windows-NT\rcmd.c"
DEP_RCMD_=\
	".\windows-NT\rcmd.h"

$(INTDIR)/rcmd.obj :  $(SOURCE)  $(DEP_RCMD_) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=".\windows-NT\startserver.c"
DEP_START=\
	.\src\cvs.h\
	".\windows-NT\rcmd.h"\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
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
	.\src\error.h\
	.\src\update.h\
	".\windows-NT\ndir.h"

$(INTDIR)/startserver.obj :  $(SOURCE)  $(DEP_START) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=".\windows-NT\ChangeLog"
# End Source File
# End Group
################################################################################
# Begin Group "zlib"

################################################################################
# Begin Source File

SOURCE=.\zlib\zutil.c
DEP_ZUTIL=\
	.\zlib\zutil.h\
	.\zlib\zlib.h\
	.\zlib\zconf.h

$(INTDIR)/zutil.obj :  $(SOURCE)  $(DEP_ZUTIL) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\zlib\infutil.c
DEP_INFUT=\
	.\zlib\zutil.h\
	.\zlib\infblock.h\
	.\zlib\inftrees.h\
	.\zlib\infcodes.h\
	.\zlib\infutil.h\
	.\zlib\zlib.h\
	.\zlib\zconf.h

$(INTDIR)/infutil.obj :  $(SOURCE)  $(DEP_INFUT) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\zlib\infblock.c
DEP_INFBL=\
	.\zlib\zutil.h\
	.\zlib\infblock.h\
	.\zlib\inftrees.h\
	.\zlib\infcodes.h\
	.\zlib\infutil.h\
	.\zlib\zlib.h\
	.\zlib\zconf.h

$(INTDIR)/infblock.obj :  $(SOURCE)  $(DEP_INFBL) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\zlib\compress.c
DEP_COMPR=\
	.\zlib\zlib.h\
	.\zlib\zconf.h

$(INTDIR)/compress.obj :  $(SOURCE)  $(DEP_COMPR) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\zlib\uncompr.c
DEP_UNCOM=\
	.\zlib\zlib.h\
	.\zlib\zconf.h

$(INTDIR)/uncompr.obj :  $(SOURCE)  $(DEP_UNCOM) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\zlib\inflate.c
DEP_INFLA=\
	.\zlib\zutil.h\
	.\zlib\infblock.h\
	.\zlib\zlib.h\
	.\zlib\zconf.h

$(INTDIR)/inflate.obj :  $(SOURCE)  $(DEP_INFLA) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\zlib\inftrees.c
DEP_INFTR=\
	.\zlib\zutil.h\
	.\zlib\inftrees.h\
	.\zlib\zlib.h\
	.\zlib\zconf.h

$(INTDIR)/inftrees.obj :  $(SOURCE)  $(DEP_INFTR) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\zlib\gzio.c
DEP_GZIO_=\
	.\zlib\zutil.h\
	.\zlib\zlib.h\
	.\zlib\zconf.h

$(INTDIR)/gzio.obj :  $(SOURCE)  $(DEP_GZIO_) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\zlib\infcodes.c
DEP_INFCO=\
	.\zlib\zutil.h\
	.\zlib\inftrees.h\
	.\zlib\infblock.h\
	.\zlib\infcodes.h\
	.\zlib\infutil.h\
	.\zlib\inffast.h\
	.\zlib\zlib.h\
	.\zlib\zconf.h

$(INTDIR)/infcodes.obj :  $(SOURCE)  $(DEP_INFCO) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\zlib\deflate.c
DEP_DEFLA=\
	.\zlib\deflate.h\
	.\zlib\zutil.h\
	.\zlib\zlib.h\
	.\zlib\zconf.h

$(INTDIR)/deflate.obj :  $(SOURCE)  $(DEP_DEFLA) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\zlib\adler32.c
DEP_ADLER=\
	.\zlib\zlib.h\
	.\zlib\zconf.h

$(INTDIR)/adler32.obj :  $(SOURCE)  $(DEP_ADLER) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\zlib\crc32.c
DEP_CRC32=\
	.\zlib\zlib.h\
	.\zlib\zconf.h

$(INTDIR)/crc32.obj :  $(SOURCE)  $(DEP_CRC32) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\zlib\inffast.c
DEP_INFFA=\
	.\zlib\zutil.h\
	.\zlib\inftrees.h\
	.\zlib\infblock.h\
	.\zlib\infcodes.h\
	.\zlib\infutil.h\
	.\zlib\inffast.h\
	.\zlib\zlib.h\
	.\zlib\zconf.h

$(INTDIR)/inffast.obj :  $(SOURCE)  $(DEP_INFFA) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\zlib\trees.c
DEP_TREES=\
	.\zlib\deflate.h\
	.\zlib\zutil.h\
	.\zlib\zlib.h\
	.\zlib\zconf.h

$(INTDIR)/trees.obj :  $(SOURCE)  $(DEP_TREES) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
# End Group
# End Project
################################################################################
