# Microsoft Developer Studio Generated NMAKE File, Format Version 40001
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

!IF "$(CFG)" == ""
CFG=cvsnt - Win32 Debug
!MESSAGE No configuration specified.  Defaulting to cvsnt - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "cvsnt - Win32 Release" && "$(CFG)" != "cvsnt - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE on this makefile
!MESSAGE by defining the macro CFG on the command line.  For example:
!MESSAGE 
!MESSAGE NMAKE /f "cvsnt.mak" CFG="cvsnt - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "cvsnt - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "cvsnt - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 
################################################################################
# Begin Project
# PROP Target_Last_Scanned "cvsnt - Win32 Debug"
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "cvsnt - Win32 Release"

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

ALL : "$(OUTDIR)\cvs.exe"

CLEAN : 
	-@erase ".\WinRel\cvs.exe"
	-@erase ".\WinRel\inflate.obj"
	-@erase ".\WinRel\trees.obj"
	-@erase ".\WinRel\getline.obj"
	-@erase ".\WinRel\edit.obj"
	-@erase ".\WinRel\zutil.obj"
	-@erase ".\WinRel\ifdef.obj"
	-@erase ".\WinRel\create_adm.obj"
	-@erase ".\WinRel\zlib.obj"
	-@erase ".\WinRel\main.obj"
	-@erase ".\WinRel\cmpbuf.obj"
	-@erase ".\WinRel\crc32.obj"
	-@erase ".\WinRel\infcodes.obj"
	-@erase ".\WinRel\myndbm.obj"
	-@erase ".\WinRel\savecwd.obj"
	-@erase ".\WinRel\client.obj"
	-@erase ".\WinRel\1\diff.obj"
	-@erase ".\WinRel\scramble.obj"
	-@erase ".\WinRel\filesubr.obj"
	-@erase ".\WinRel\infutil.obj"
	-@erase ".\WinRel\gzio.obj"
	-@erase ".\WinRel\fnmatch.obj"
	-@erase ".\WinRel\side.obj"
	-@erase ".\WinRel\win32.obj"
	-@erase ".\WinRel\inffast.obj"
	-@erase ".\WinRel\startserver.obj"
	-@erase ".\WinRel\ignore.obj"
	-@erase ".\WinRel\expand_path.obj"
	-@erase ".\WinRel\repos.obj"
	-@erase ".\WinRel\rtag.obj"
	-@erase ".\WinRel\valloc.obj"
	-@erase ".\WinRel\cvsrc.obj"
	-@erase ".\WinRel\lock.obj"
	-@erase ".\WinRel\log.obj"
	-@erase ".\WinRel\ed.obj"
	-@erase ".\WinRel\deflate.obj"
	-@erase ".\WinRel\analyze.obj"
	-@erase ".\WinRel\commit.obj"
	-@erase ".\WinRel\patch.obj"
	-@erase ".\WinRel\classify.obj"
	-@erase ".\WinRel\2\diff.obj"
	-@erase ".\WinRel\history.obj"
	-@erase ".\WinRel\add.obj"
	-@erase ".\WinRel\update.obj"
	-@erase ".\WinRel\2\version.obj"
	-@erase ".\WinRel\uncompr.obj"
	-@erase ".\WinRel\buffer.obj"
	-@erase ".\WinRel\rcmd.obj"
	-@erase ".\WinRel\find_names.obj"
	-@erase ".\WinRel\watch.obj"
	-@erase ".\WinRel\getopt.obj"
	-@erase ".\WinRel\checkin.obj"
	-@erase ".\WinRel\fileattr.obj"
	-@erase ".\WinRel\modules.obj"
	-@erase ".\WinRel\rcs.obj"
	-@erase ".\WinRel\admin.obj"
	-@erase ".\WinRel\mkmodules.obj"
	-@erase ".\WinRel\diff3.obj"
	-@erase ".\WinRel\run.obj"
	-@erase ".\WinRel\compress.obj"
	-@erase ".\WinRel\waitpid.obj"
	-@erase ".\WinRel\normal.obj"
	-@erase ".\WinRel\adler32.obj"
	-@erase ".\WinRel\login.obj"
	-@erase ".\WinRel\sighandle.obj"
	-@erase ".\WinRel\util.obj"
	-@erase ".\WinRel\logmsg.obj"
	-@erase ".\WinRel\remove.obj"
	-@erase ".\WinRel\entries.obj"
	-@erase ".\WinRel\dir.obj"
	-@erase ".\WinRel\getopt1.obj"
	-@erase ".\WinRel\md5.obj"
	-@erase ".\WinRel\xgetwd.obj"
	-@erase ".\WinRel\yesno.obj"
	-@erase ".\WinRel\no_diff.obj"
	-@erase ".\WinRel\root.obj"
	-@erase ".\WinRel\tag.obj"
	-@erase ".\WinRel\server.obj"
	-@erase ".\WinRel\pwd.obj"
	-@erase ".\WinRel\parseinfo.obj"
	-@erase ".\WinRel\infblock.obj"
	-@erase ".\WinRel\inftrees.obj"
	-@erase ".\WinRel\regex.obj"
	-@erase ".\WinRel\io.obj"
	-@erase ".\WinRel\1\version.obj"
	-@erase ".\WinRel\vasprintf.obj"
	-@erase ".\WinRel\vers_ts.obj"
	-@erase ".\WinRel\checkout.obj"
	-@erase ".\WinRel\stripslash.obj"
	-@erase ".\WinRel\error.obj"
	-@erase ".\WinRel\hash.obj"
	-@erase ".\WinRel\subr.obj"
	-@erase ".\WinRel\import.obj"
	-@erase ".\WinRel\release.obj"
	-@erase ".\WinRel\recurse.obj"
	-@erase ".\WinRel\argmatch.obj"
	-@erase ".\WinRel\sockerror.obj"
	-@erase ".\WinRel\context.obj"
	-@erase ".\WinRel\rcscmds.obj"
	-@erase ".\WinRel\wrapper.obj"
	-@erase ".\WinRel\status.obj"
	-@erase ".\WinRel\ndir.obj"
	-@erase ".\WinRel\getdate.obj"
	-@erase ".\WinRel\mkdir.obj"
	-@erase ".\WinRel\fncase.obj"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /FR /YX /c
# ADD CPP /nologo /W3 /GX /Ob1 /I "windows-NT" /I "lib" /I "src" /I "zlib" /D "NDEBUG" /D "WIN32" /D "_CONSOLE" /D "HAVE_CONFIG_H" /YX /c
# SUBTRACT CPP /WX /Fr
CPP_PROJ=/nologo /ML /W3 /GX /Ob1 /I "windows-NT" /I "lib" /I "src" /I "zlib"\
 /D "NDEBUG" /D "WIN32" /D "_CONSOLE" /D "HAVE_CONFIG_H"\
 /Fp"$(INTDIR)/cvsnt.pch" /YX /Fo"$(INTDIR)/" /c 
CPP_OBJS=.\WinRel/
CPP_SBRS=
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o"$(OUTDIR)/cvsnt.bsc" 
BSC32_SBRS=
LINK32=link.exe
# ADD BASE LINK32 advapi32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 wsock32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib /nologo /subsystem:console /machine:I386 /out:"WinRel/cvs.exe"
LINK32_FLAGS=wsock32.lib kernel32.lib user32.lib gdi32.lib winspool.lib\
 comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib /nologo\
 /subsystem:console /incremental:no /pdb:"$(OUTDIR)/cvs.pdb" /machine:I386\
 /out:"$(OUTDIR)/cvs.exe" 
LINK32_OBJS= \
	".\WinRel\inflate.obj" \
	".\WinRel\trees.obj" \
	".\WinRel\getline.obj" \
	".\WinRel\edit.obj" \
	".\WinRel\zutil.obj" \
	".\WinRel\ifdef.obj" \
	".\WinRel\create_adm.obj" \
	".\WinRel\zlib.obj" \
	".\WinRel\main.obj" \
	".\WinRel\cmpbuf.obj" \
	".\WinRel\crc32.obj" \
	".\WinRel\infcodes.obj" \
	".\WinRel\myndbm.obj" \
	".\WinRel\savecwd.obj" \
	".\WinRel\client.obj" \
	".\WinRel\1\diff.obj" \
	".\WinRel\scramble.obj" \
	".\WinRel\filesubr.obj" \
	".\WinRel\infutil.obj" \
	".\WinRel\gzio.obj" \
	".\WinRel\fnmatch.obj" \
	".\WinRel\side.obj" \
	".\WinRel\win32.obj" \
	".\WinRel\inffast.obj" \
	".\WinRel\startserver.obj" \
	".\WinRel\ignore.obj" \
	".\WinRel\expand_path.obj" \
	".\WinRel\repos.obj" \
	".\WinRel\rtag.obj" \
	".\WinRel\valloc.obj" \
	".\WinRel\cvsrc.obj" \
	".\WinRel\lock.obj" \
	".\WinRel\log.obj" \
	".\WinRel\ed.obj" \
	".\WinRel\deflate.obj" \
	".\WinRel\analyze.obj" \
	".\WinRel\commit.obj" \
	".\WinRel\patch.obj" \
	".\WinRel\classify.obj" \
	".\WinRel\2\diff.obj" \
	".\WinRel\history.obj" \
	".\WinRel\add.obj" \
	".\WinRel\update.obj" \
	".\WinRel\2\version.obj" \
	".\WinRel\uncompr.obj" \
	".\WinRel\buffer.obj" \
	".\WinRel\rcmd.obj" \
	".\WinRel\find_names.obj" \
	".\WinRel\watch.obj" \
	".\WinRel\getopt.obj" \
	".\WinRel\checkin.obj" \
	".\WinRel\fileattr.obj" \
	".\WinRel\modules.obj" \
	".\WinRel\rcs.obj" \
	".\WinRel\admin.obj" \
	".\WinRel\mkmodules.obj" \
	".\WinRel\diff3.obj" \
	".\WinRel\run.obj" \
	".\WinRel\compress.obj" \
	".\WinRel\waitpid.obj" \
	".\WinRel\normal.obj" \
	".\WinRel\adler32.obj" \
	".\WinRel\login.obj" \
	".\WinRel\sighandle.obj" \
	".\WinRel\util.obj" \
	".\WinRel\logmsg.obj" \
	".\WinRel\remove.obj" \
	".\WinRel\entries.obj" \
	".\WinRel\dir.obj" \
	".\WinRel\getopt1.obj" \
	".\WinRel\md5.obj" \
	".\WinRel\xgetwd.obj" \
	".\WinRel\yesno.obj" \
	".\WinRel\no_diff.obj" \
	".\WinRel\root.obj" \
	".\WinRel\tag.obj" \
	".\WinRel\server.obj" \
	".\WinRel\pwd.obj" \
	".\WinRel\parseinfo.obj" \
	".\WinRel\infblock.obj" \
	".\WinRel\inftrees.obj" \
	".\WinRel\regex.obj" \
	".\WinRel\io.obj" \
	".\WinRel\1\version.obj" \
	".\WinRel\vasprintf.obj" \
	".\WinRel\vers_ts.obj" \
	".\WinRel\checkout.obj" \
	".\WinRel\stripslash.obj" \
	".\WinRel\error.obj" \
	".\WinRel\hash.obj" \
	".\WinRel\subr.obj" \
	".\WinRel\import.obj" \
	".\WinRel\release.obj" \
	".\WinRel\recurse.obj" \
	".\WinRel\argmatch.obj" \
	".\WinRel\sockerror.obj" \
	".\WinRel\context.obj" \
	".\WinRel\rcscmds.obj" \
	".\WinRel\wrapper.obj" \
	".\WinRel\status.obj" \
	".\WinRel\ndir.obj" \
	".\WinRel\getdate.obj" \
	".\WinRel\mkdir.obj" \
	".\WinRel\fncase.obj"

"$(OUTDIR)\cvs.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "cvsnt - Win32 Debug"

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

ALL : "$(OUTDIR)\cvs.exe"

CLEAN : 
	-@erase ".\WinDebug\vc40.pdb"
	-@erase ".\WinDebug\vc40.idb"
	-@erase ".\WinDebug\1\vc40.pdb"
	-@erase ".\WinDebug\1\vc40.idb"
	-@erase ".\WinDebug\2\vc40.pdb"
	-@erase ".\WinDebug\2\vc40.idb"
	-@erase ".\WinDebug\cvs.exe"
	-@erase ".\WinDebug\regex.obj"
	-@erase ".\WinDebug\normal.obj"
	-@erase ".\WinDebug\client.obj"
	-@erase ".\WinDebug\util.obj"
	-@erase ".\WinDebug\create_adm.obj"
	-@erase ".\WinDebug\vers_ts.obj"
	-@erase ".\WinDebug\logmsg.obj"
	-@erase ".\WinDebug\mkmodules.obj"
	-@erase ".\WinDebug\remove.obj"
	-@erase ".\WinDebug\sighandle.obj"
	-@erase ".\WinDebug\watch.obj"
	-@erase ".\WinDebug\release.obj"
	-@erase ".\WinDebug\server.obj"
	-@erase ".\WinDebug\recurse.obj"
	-@erase ".\WinDebug\checkout.obj"
	-@erase ".\WinDebug\context.obj"
	-@erase ".\WinDebug\rcscmds.obj"
	-@erase ".\WinDebug\side.obj"
	-@erase ".\WinDebug\wrapper.obj"
	-@erase ".\WinDebug\getdate.obj"
	-@erase ".\WinDebug\parseinfo.obj"
	-@erase ".\WinDebug\login.obj"
	-@erase ".\WinDebug\hash.obj"
	-@erase ".\WinDebug\1\version.obj"
	-@erase ".\WinDebug\subr.obj"
	-@erase ".\WinDebug\vasprintf.obj"
	-@erase ".\WinDebug\fileattr.obj"
	-@erase ".\WinDebug\import.obj"
	-@erase ".\WinDebug\getline.obj"
	-@erase ".\WinDebug\ifdef.obj"
	-@erase ".\WinDebug\crc32.obj"
	-@erase ".\WinDebug\find_names.obj"
	-@erase ".\WinDebug\status.obj"
	-@erase ".\WinDebug\ndir.obj"
	-@erase ".\WinDebug\savecwd.obj"
	-@erase ".\WinDebug\log.obj"
	-@erase ".\WinDebug\sockerror.obj"
	-@erase ".\WinDebug\infutil.obj"
	-@erase ".\WinDebug\win32.obj"
	-@erase ".\WinDebug\inffast.obj"
	-@erase ".\WinDebug\ed.obj"
	-@erase ".\WinDebug\edit.obj"
	-@erase ".\WinDebug\add.obj"
	-@erase ".\WinDebug\repos.obj"
	-@erase ".\WinDebug\error.obj"
	-@erase ".\WinDebug\zlib.obj"
	-@erase ".\WinDebug\main.obj"
	-@erase ".\WinDebug\filesubr.obj"
	-@erase ".\WinDebug\cmpbuf.obj"
	-@erase ".\WinDebug\myndbm.obj"
	-@erase ".\WinDebug\xgetwd.obj"
	-@erase ".\WinDebug\infblock.obj"
	-@erase ".\WinDebug\inftrees.obj"
	-@erase ".\WinDebug\deflate.obj"
	-@erase ".\WinDebug\1\diff.obj"
	-@erase ".\WinDebug\analyze.obj"
	-@erase ".\WinDebug\gzio.obj"
	-@erase ".\WinDebug\patch.obj"
	-@erase ".\WinDebug\rcs.obj"
	-@erase ".\WinDebug\history.obj"
	-@erase ".\WinDebug\run.obj"
	-@erase ".\WinDebug\mkdir.obj"
	-@erase ".\WinDebug\uncompr.obj"
	-@erase ".\WinDebug\ignore.obj"
	-@erase ".\WinDebug\rtag.obj"
	-@erase ".\WinDebug\root.obj"
	-@erase ".\WinDebug\trees.obj"
	-@erase ".\WinDebug\checkin.obj"
	-@erase ".\WinDebug\valloc.obj"
	-@erase ".\WinDebug\argmatch.obj"
	-@erase ".\WinDebug\lock.obj"
	-@erase ".\WinDebug\modules.obj"
	-@erase ".\WinDebug\classify.obj"
	-@erase ".\WinDebug\dir.obj"
	-@erase ".\WinDebug\startserver.obj"
	-@erase ".\WinDebug\zutil.obj"
	-@erase ".\WinDebug\stripslash.obj"
	-@erase ".\WinDebug\md5.obj"
	-@erase ".\WinDebug\admin.obj"
	-@erase ".\WinDebug\commit.obj"
	-@erase ".\WinDebug\diff3.obj"
	-@erase ".\WinDebug\expand_path.obj"
	-@erase ".\WinDebug\2\diff.obj"
	-@erase ".\WinDebug\update.obj"
	-@erase ".\WinDebug\waitpid.obj"
	-@erase ".\WinDebug\adler32.obj"
	-@erase ".\WinDebug\inflate.obj"
	-@erase ".\WinDebug\tag.obj"
	-@erase ".\WinDebug\buffer.obj"
	-@erase ".\WinDebug\rcmd.obj"
	-@erase ".\WinDebug\pwd.obj"
	-@erase ".\WinDebug\getopt.obj"
	-@erase ".\WinDebug\entries.obj"
	-@erase ".\WinDebug\getopt1.obj"
	-@erase ".\WinDebug\2\version.obj"
	-@erase ".\WinDebug\yesno.obj"
	-@erase ".\WinDebug\compress.obj"
	-@erase ".\WinDebug\io.obj"
	-@erase ".\WinDebug\infcodes.obj"
	-@erase ".\WinDebug\no_diff.obj"
	-@erase ".\WinDebug\scramble.obj"
	-@erase ".\WinDebug\cvsrc.obj"
	-@erase ".\WinDebug\fnmatch.obj"
	-@erase ".\WinDebug\fncase.obj"
	-@erase ".\WinDebug\cvs.ilk"
	-@erase ".\WinDebug\cvs.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

# ADD BASE CPP /nologo /W3 /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /FR /YX /c
# ADD CPP /nologo /W3 /Gm /GX /Zi /Ob1 /I "windows-NT" /I "lib" /I "src" /I "zlib" /D "_DEBUG" /D "WIN32" /D "_CONSOLE" /D "HAVE_CONFIG_H" /YX /c
# SUBTRACT CPP /Fr
CPP_PROJ=/nologo /MLd /W3 /Gm /GX /Zi /Ob1 /I "windows-NT" /I "lib" /I "src" /I\
 "zlib" /D "_DEBUG" /D "WIN32" /D "_CONSOLE" /D "HAVE_CONFIG_H"\
 /Fp"$(INTDIR)/cvsnt.pch" /YX /Fo"$(INTDIR)/" /Fd"$(INTDIR)/" /c 
CPP_OBJS=.\WinDebug/
CPP_SBRS=
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o"$(OUTDIR)/cvsnt.bsc" 
BSC32_SBRS=
LINK32=link.exe
# ADD BASE LINK32 advapi32.lib /nologo /subsystem:console /debug /machine:I386
# ADD LINK32 wsock32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib /nologo /subsystem:console /debug /machine:I386 /out:"WinDebug/cvs.exe"
LINK32_FLAGS=wsock32.lib kernel32.lib user32.lib gdi32.lib winspool.lib\
 comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib /nologo\
 /subsystem:console /incremental:yes /pdb:"$(OUTDIR)/cvs.pdb" /debug\
 /machine:I386 /out:"$(OUTDIR)/cvs.exe" 
LINK32_OBJS= \
	".\WinDebug\regex.obj" \
	".\WinDebug\normal.obj" \
	".\WinDebug\client.obj" \
	".\WinDebug\util.obj" \
	".\WinDebug\create_adm.obj" \
	".\WinDebug\vers_ts.obj" \
	".\WinDebug\logmsg.obj" \
	".\WinDebug\mkmodules.obj" \
	".\WinDebug\remove.obj" \
	".\WinDebug\sighandle.obj" \
	".\WinDebug\watch.obj" \
	".\WinDebug\release.obj" \
	".\WinDebug\server.obj" \
	".\WinDebug\recurse.obj" \
	".\WinDebug\checkout.obj" \
	".\WinDebug\context.obj" \
	".\WinDebug\rcscmds.obj" \
	".\WinDebug\side.obj" \
	".\WinDebug\wrapper.obj" \
	".\WinDebug\getdate.obj" \
	".\WinDebug\parseinfo.obj" \
	".\WinDebug\login.obj" \
	".\WinDebug\hash.obj" \
	".\WinDebug\1\version.obj" \
	".\WinDebug\subr.obj" \
	".\WinDebug\vasprintf.obj" \
	".\WinDebug\fileattr.obj" \
	".\WinDebug\import.obj" \
	".\WinDebug\getline.obj" \
	".\WinDebug\ifdef.obj" \
	".\WinDebug\crc32.obj" \
	".\WinDebug\find_names.obj" \
	".\WinDebug\status.obj" \
	".\WinDebug\ndir.obj" \
	".\WinDebug\savecwd.obj" \
	".\WinDebug\log.obj" \
	".\WinDebug\sockerror.obj" \
	".\WinDebug\infutil.obj" \
	".\WinDebug\win32.obj" \
	".\WinDebug\inffast.obj" \
	".\WinDebug\ed.obj" \
	".\WinDebug\edit.obj" \
	".\WinDebug\add.obj" \
	".\WinDebug\repos.obj" \
	".\WinDebug\error.obj" \
	".\WinDebug\zlib.obj" \
	".\WinDebug\main.obj" \
	".\WinDebug\filesubr.obj" \
	".\WinDebug\cmpbuf.obj" \
	".\WinDebug\myndbm.obj" \
	".\WinDebug\xgetwd.obj" \
	".\WinDebug\infblock.obj" \
	".\WinDebug\inftrees.obj" \
	".\WinDebug\deflate.obj" \
	".\WinDebug\1\diff.obj" \
	".\WinDebug\analyze.obj" \
	".\WinDebug\gzio.obj" \
	".\WinDebug\patch.obj" \
	".\WinDebug\rcs.obj" \
	".\WinDebug\history.obj" \
	".\WinDebug\run.obj" \
	".\WinDebug\mkdir.obj" \
	".\WinDebug\uncompr.obj" \
	".\WinDebug\ignore.obj" \
	".\WinDebug\rtag.obj" \
	".\WinDebug\root.obj" \
	".\WinDebug\trees.obj" \
	".\WinDebug\checkin.obj" \
	".\WinDebug\valloc.obj" \
	".\WinDebug\argmatch.obj" \
	".\WinDebug\lock.obj" \
	".\WinDebug\modules.obj" \
	".\WinDebug\classify.obj" \
	".\WinDebug\dir.obj" \
	".\WinDebug\startserver.obj" \
	".\WinDebug\zutil.obj" \
	".\WinDebug\stripslash.obj" \
	".\WinDebug\md5.obj" \
	".\WinDebug\admin.obj" \
	".\WinDebug\commit.obj" \
	".\WinDebug\diff3.obj" \
	".\WinDebug\expand_path.obj" \
	".\WinDebug\2\diff.obj" \
	".\WinDebug\update.obj" \
	".\WinDebug\waitpid.obj" \
	".\WinDebug\adler32.obj" \
	".\WinDebug\inflate.obj" \
	".\WinDebug\tag.obj" \
	".\WinDebug\buffer.obj" \
	".\WinDebug\rcmd.obj" \
	".\WinDebug\pwd.obj" \
	".\WinDebug\getopt.obj" \
	".\WinDebug\entries.obj" \
	".\WinDebug\getopt1.obj" \
	".\WinDebug\2\version.obj" \
	".\WinDebug\yesno.obj" \
	".\WinDebug\compress.obj" \
	".\WinDebug\io.obj" \
	".\WinDebug\infcodes.obj" \
	".\WinDebug\no_diff.obj" \
	".\WinDebug\scramble.obj" \
	".\WinDebug\cvsrc.obj" \
	".\WinDebug\fnmatch.obj" \
	".\WinDebug\fncase.obj"

"$(OUTDIR)\cvs.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
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

.c{$(CPP_SBRS)}.sbr:
   $(CPP) $(CPP_PROJ) $<  

.cpp{$(CPP_SBRS)}.sbr:
   $(CPP) $(CPP_PROJ) $<  

.cxx{$(CPP_SBRS)}.sbr:
   $(CPP) $(CPP_PROJ) $<  

################################################################################
# Begin Target

# Name "cvsnt - Win32 Release"
# Name "cvsnt - Win32 Debug"

!IF  "$(CFG)" == "cvsnt - Win32 Release"

!ELSEIF  "$(CFG)" == "cvsnt - Win32 Debug"

!ENDIF 

################################################################################
# Begin Source File

SOURCE=.\src\mkmodules.c
DEP_CPP_MKMOD=\
	".\src\cvs.h"\
	".\lib\savecwd.h"\
	".\lib\getline.h"\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
	".\lib\fnmatch.h"\
	".\windows-NT\pwd.h"\
	".\lib\system.h"\
	".\src\hash.h"\
	".\src\client.h"\
	".\src\myndbm.h"\
	".\lib\regex.h"\
	".\lib\getopt.h"\
	".\lib\wait.h"\
	".\src\rcs.h"\
	".\src\update.h"\
	".\src\server.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	{$(INCLUDE)}"\sys\Timeb.h"\
	{$(INCLUDE)}"\sys\Utime.h"\
	".\windows-NT\ndir.h"\
	
NODEP_CPP_MKMOD=\
	".\src\popen.h"\
	".\lib\tcpip.h"\
	

"$(INTDIR)\mkmodules.obj" : $(SOURCE) $(DEP_CPP_MKMOD) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\subr.c
DEP_CPP_SUBR_=\
	".\src\cvs.h"\
	".\lib\getline.h"\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
	".\lib\fnmatch.h"\
	".\windows-NT\pwd.h"\
	".\lib\system.h"\
	".\src\hash.h"\
	".\src\client.h"\
	".\src\myndbm.h"\
	".\lib\regex.h"\
	".\lib\getopt.h"\
	".\lib\wait.h"\
	".\src\rcs.h"\
	".\src\update.h"\
	".\src\server.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	{$(INCLUDE)}"\sys\Timeb.h"\
	{$(INCLUDE)}"\sys\Utime.h"\
	".\windows-NT\ndir.h"\
	
NODEP_CPP_SUBR_=\
	".\src\popen.h"\
	".\lib\tcpip.h"\
	

"$(INTDIR)\subr.obj" : $(SOURCE) $(DEP_CPP_SUBR_) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\admin.c
DEP_CPP_ADMIN=\
	".\src\cvs.h"\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
	".\lib\fnmatch.h"\
	".\windows-NT\pwd.h"\
	".\lib\system.h"\
	".\src\hash.h"\
	".\src\client.h"\
	".\src\myndbm.h"\
	".\lib\regex.h"\
	".\lib\getopt.h"\
	".\lib\wait.h"\
	".\src\rcs.h"\
	".\src\update.h"\
	".\src\server.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	{$(INCLUDE)}"\sys\Timeb.h"\
	{$(INCLUDE)}"\sys\Utime.h"\
	".\windows-NT\ndir.h"\
	
NODEP_CPP_ADMIN=\
	".\src\popen.h"\
	".\lib\tcpip.h"\
	

"$(INTDIR)\admin.obj" : $(SOURCE) $(DEP_CPP_ADMIN) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\server.c
DEP_CPP_SERVE=\
	".\src\cvs.h"\
	".\src\watch.h"\
	".\src\edit.h"\
	".\src\fileattr.h"\
	".\lib\getline.h"\
	".\src\buffer.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
	".\lib\fnmatch.h"\
	".\windows-NT\pwd.h"\
	".\lib\system.h"\
	".\src\hash.h"\
	".\src\client.h"\
	".\src\myndbm.h"\
	".\lib\regex.h"\
	".\lib\getopt.h"\
	".\lib\wait.h"\
	".\src\rcs.h"\
	".\src\update.h"\
	".\src\server.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	{$(INCLUDE)}"\sys\Timeb.h"\
	{$(INCLUDE)}"\sys\Utime.h"\
	".\windows-NT\ndir.h"\
	
NODEP_CPP_SERVE=\
	".\src\popen.h"\
	".\lib\tcpip.h"\
	

"$(INTDIR)\server.obj" : $(SOURCE) $(DEP_CPP_SERVE) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\diff.c
DEP_CPP_DIFF_=\
	".\src\cvs.h"\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
	".\lib\fnmatch.h"\
	".\windows-NT\pwd.h"\
	".\lib\system.h"\
	".\src\hash.h"\
	".\src\client.h"\
	".\src\myndbm.h"\
	".\lib\regex.h"\
	".\lib\getopt.h"\
	".\lib\wait.h"\
	".\src\rcs.h"\
	".\src\update.h"\
	".\src\server.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	{$(INCLUDE)}"\sys\Timeb.h"\
	{$(INCLUDE)}"\sys\Utime.h"\
	".\windows-NT\ndir.h"\
	
NODEP_CPP_DIFF_=\
	".\src\popen.h"\
	".\lib\tcpip.h"\
	
INTDIR_SRC=.\WinRel\1
"$(INTDIR_SRC)" :
    if not exist "$(INTDIR_SRC)/$(NULL)" mkdir "$(INTDIR_SRC)"

!IF  "$(CFG)" == "cvsnt - Win32 Release"

# PROP Intermediate_Dir "WinRel\1"

".\WinRel\1\diff.obj" : $(SOURCE) $(DEP_CPP_DIFF_) "$(INTDIR_SRC)"
   $(CPP) /nologo /ML /W3 /GX /Ob1 /I "windows-NT" /I "lib" /I "src" /I "zlib"\
 /D "NDEBUG" /D "WIN32" /D "_CONSOLE" /D "HAVE_CONFIG_H" /Fp"WinRel/cvsnt.pch"\
 /YX /Fo"$(INTDIR_SRC)/" /c $(SOURCE)


!ELSEIF  "$(CFG)" == "cvsnt - Win32 Debug"

# PROP Intermediate_Dir "WinDebug\1"

".\WinDebug\1\diff.obj" : $(SOURCE) $(DEP_CPP_DIFF_) "$(INTDIR_SRC)"
   $(CPP) /nologo /MLd /W3 /Gm /GX /Zi /Ob1 /I "windows-NT" /I "lib" /I "src"\
 /I "zlib" /D "_DEBUG" /D "WIN32" /D "_CONSOLE" /D "HAVE_CONFIG_H"\
 /Fp"WinDebug/cvsnt.pch" /YX /Fo"$(INTDIR_SRC)/" /Fd"$(INTDIR_SRC)/" /c\
 $(SOURCE)


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\client.c
DEP_CPP_CLIEN=\
	".\windows-NT\config.h"\
	".\src\cvs.h"\
	".\lib\getline.h"\
	".\src\edit.h"\
	".\src\buffer.h"\
	".\lib\md5.h"\
	".\windows-NT\options.h"\
	".\lib\fnmatch.h"\
	".\windows-NT\pwd.h"\
	".\lib\system.h"\
	".\src\hash.h"\
	".\src\client.h"\
	".\src\myndbm.h"\
	".\lib\regex.h"\
	".\lib\getopt.h"\
	".\lib\wait.h"\
	".\src\rcs.h"\
	".\src\update.h"\
	".\src\server.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	{$(INCLUDE)}"\sys\Timeb.h"\
	{$(INCLUDE)}"\sys\Utime.h"\
	".\windows-NT\ndir.h"\
	
NODEP_CPP_CLIEN=\
	".\src\popen.h"\
	".\lib\tcpip.h"\
	

"$(INTDIR)\client.obj" : $(SOURCE) $(DEP_CPP_CLIEN) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\checkout.c
DEP_CPP_CHECK=\
	".\src\cvs.h"\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
	".\lib\fnmatch.h"\
	".\windows-NT\pwd.h"\
	".\lib\system.h"\
	".\src\hash.h"\
	".\src\client.h"\
	".\src\myndbm.h"\
	".\lib\regex.h"\
	".\lib\getopt.h"\
	".\lib\wait.h"\
	".\src\rcs.h"\
	".\src\update.h"\
	".\src\server.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	{$(INCLUDE)}"\sys\Timeb.h"\
	{$(INCLUDE)}"\sys\Utime.h"\
	".\windows-NT\ndir.h"\
	
NODEP_CPP_CHECK=\
	".\src\popen.h"\
	".\lib\tcpip.h"\
	

"$(INTDIR)\checkout.obj" : $(SOURCE) $(DEP_CPP_CHECK) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\no_diff.c
DEP_CPP_NO_DI=\
	".\src\cvs.h"\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
	".\lib\fnmatch.h"\
	".\windows-NT\pwd.h"\
	".\lib\system.h"\
	".\src\hash.h"\
	".\src\client.h"\
	".\src\myndbm.h"\
	".\lib\regex.h"\
	".\lib\getopt.h"\
	".\lib\wait.h"\
	".\src\rcs.h"\
	".\src\update.h"\
	".\src\server.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	{$(INCLUDE)}"\sys\Timeb.h"\
	{$(INCLUDE)}"\sys\Utime.h"\
	".\windows-NT\ndir.h"\
	
NODEP_CPP_NO_DI=\
	".\src\popen.h"\
	".\lib\tcpip.h"\
	

"$(INTDIR)\no_diff.obj" : $(SOURCE) $(DEP_CPP_NO_DI) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\entries.c
DEP_CPP_ENTRI=\
	".\src\cvs.h"\
	".\lib\getline.h"\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
	".\lib\fnmatch.h"\
	".\windows-NT\pwd.h"\
	".\lib\system.h"\
	".\src\hash.h"\
	".\src\client.h"\
	".\src\myndbm.h"\
	".\lib\regex.h"\
	".\lib\getopt.h"\
	".\lib\wait.h"\
	".\src\rcs.h"\
	".\src\update.h"\
	".\src\server.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	{$(INCLUDE)}"\sys\Timeb.h"\
	{$(INCLUDE)}"\sys\Utime.h"\
	".\windows-NT\ndir.h"\
	
NODEP_CPP_ENTRI=\
	".\src\popen.h"\
	".\lib\tcpip.h"\
	

"$(INTDIR)\entries.obj" : $(SOURCE) $(DEP_CPP_ENTRI) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\tag.c
DEP_CPP_TAG_C=\
	".\src\cvs.h"\
	".\lib\savecwd.h"\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
	".\lib\fnmatch.h"\
	".\windows-NT\pwd.h"\
	".\lib\system.h"\
	".\src\hash.h"\
	".\src\client.h"\
	".\src\myndbm.h"\
	".\lib\regex.h"\
	".\lib\getopt.h"\
	".\lib\wait.h"\
	".\src\rcs.h"\
	".\src\update.h"\
	".\src\server.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	{$(INCLUDE)}"\sys\Timeb.h"\
	{$(INCLUDE)}"\sys\Utime.h"\
	".\windows-NT\ndir.h"\
	
NODEP_CPP_TAG_C=\
	".\src\popen.h"\
	".\lib\tcpip.h"\
	

"$(INTDIR)\tag.obj" : $(SOURCE) $(DEP_CPP_TAG_C) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\rtag.c
DEP_CPP_RTAG_=\
	".\src\cvs.h"\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
	".\lib\fnmatch.h"\
	".\windows-NT\pwd.h"\
	".\lib\system.h"\
	".\src\hash.h"\
	".\src\client.h"\
	".\src\myndbm.h"\
	".\lib\regex.h"\
	".\lib\getopt.h"\
	".\lib\wait.h"\
	".\src\rcs.h"\
	".\src\update.h"\
	".\src\server.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	{$(INCLUDE)}"\sys\Timeb.h"\
	{$(INCLUDE)}"\sys\Utime.h"\
	".\windows-NT\ndir.h"\
	
NODEP_CPP_RTAG_=\
	".\src\popen.h"\
	".\lib\tcpip.h"\
	

"$(INTDIR)\rtag.obj" : $(SOURCE) $(DEP_CPP_RTAG_) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\status.c
DEP_CPP_STATU=\
	".\src\cvs.h"\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
	".\lib\fnmatch.h"\
	".\windows-NT\pwd.h"\
	".\lib\system.h"\
	".\src\hash.h"\
	".\src\client.h"\
	".\src\myndbm.h"\
	".\lib\regex.h"\
	".\lib\getopt.h"\
	".\lib\wait.h"\
	".\src\rcs.h"\
	".\src\update.h"\
	".\src\server.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	{$(INCLUDE)}"\sys\Timeb.h"\
	{$(INCLUDE)}"\sys\Utime.h"\
	".\windows-NT\ndir.h"\
	
NODEP_CPP_STATU=\
	".\src\popen.h"\
	".\lib\tcpip.h"\
	

"$(INTDIR)\status.obj" : $(SOURCE) $(DEP_CPP_STATU) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\root.c
DEP_CPP_ROOT_=\
	".\src\cvs.h"\
	".\lib\getline.h"\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
	".\lib\fnmatch.h"\
	".\windows-NT\pwd.h"\
	".\lib\system.h"\
	".\src\hash.h"\
	".\src\client.h"\
	".\src\myndbm.h"\
	".\lib\regex.h"\
	".\lib\getopt.h"\
	".\lib\wait.h"\
	".\src\rcs.h"\
	".\src\update.h"\
	".\src\server.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	{$(INCLUDE)}"\sys\Timeb.h"\
	{$(INCLUDE)}"\sys\Utime.h"\
	".\windows-NT\ndir.h"\
	
NODEP_CPP_ROOT_=\
	".\src\popen.h"\
	".\lib\tcpip.h"\
	

"$(INTDIR)\root.obj" : $(SOURCE) $(DEP_CPP_ROOT_) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\myndbm.c
DEP_CPP_MYNDB=\
	".\src\cvs.h"\
	".\lib\getline.h"\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
	".\lib\fnmatch.h"\
	".\windows-NT\pwd.h"\
	".\lib\system.h"\
	".\src\hash.h"\
	".\src\client.h"\
	".\src\myndbm.h"\
	".\lib\regex.h"\
	".\lib\getopt.h"\
	".\lib\wait.h"\
	".\src\rcs.h"\
	".\src\update.h"\
	".\src\server.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	{$(INCLUDE)}"\sys\Timeb.h"\
	{$(INCLUDE)}"\sys\Utime.h"\
	".\windows-NT\ndir.h"\
	
NODEP_CPP_MYNDB=\
	".\src\popen.h"\
	".\lib\tcpip.h"\
	

"$(INTDIR)\myndbm.obj" : $(SOURCE) $(DEP_CPP_MYNDB) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\hash.c
DEP_CPP_HASH_=\
	".\src\cvs.h"\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
	".\lib\fnmatch.h"\
	".\windows-NT\pwd.h"\
	".\lib\system.h"\
	".\src\hash.h"\
	".\src\client.h"\
	".\src\myndbm.h"\
	".\lib\regex.h"\
	".\lib\getopt.h"\
	".\lib\wait.h"\
	".\src\rcs.h"\
	".\src\update.h"\
	".\src\server.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	{$(INCLUDE)}"\sys\Timeb.h"\
	{$(INCLUDE)}"\sys\Utime.h"\
	".\windows-NT\ndir.h"\
	
NODEP_CPP_HASH_=\
	".\src\popen.h"\
	".\lib\tcpip.h"\
	

"$(INTDIR)\hash.obj" : $(SOURCE) $(DEP_CPP_HASH_) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\repos.c
DEP_CPP_REPOS=\
	".\src\cvs.h"\
	".\lib\getline.h"\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
	".\lib\fnmatch.h"\
	".\windows-NT\pwd.h"\
	".\lib\system.h"\
	".\src\hash.h"\
	".\src\client.h"\
	".\src\myndbm.h"\
	".\lib\regex.h"\
	".\lib\getopt.h"\
	".\lib\wait.h"\
	".\src\rcs.h"\
	".\src\update.h"\
	".\src\server.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	{$(INCLUDE)}"\sys\Timeb.h"\
	{$(INCLUDE)}"\sys\Utime.h"\
	".\windows-NT\ndir.h"\
	
NODEP_CPP_REPOS=\
	".\src\popen.h"\
	".\lib\tcpip.h"\
	

"$(INTDIR)\repos.obj" : $(SOURCE) $(DEP_CPP_REPOS) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\parseinfo.c
DEP_CPP_PARSE=\
	".\src\cvs.h"\
	".\lib\getline.h"\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
	".\lib\fnmatch.h"\
	".\windows-NT\pwd.h"\
	".\lib\system.h"\
	".\src\hash.h"\
	".\src\client.h"\
	".\src\myndbm.h"\
	".\lib\regex.h"\
	".\lib\getopt.h"\
	".\lib\wait.h"\
	".\src\rcs.h"\
	".\src\update.h"\
	".\src\server.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	{$(INCLUDE)}"\sys\Timeb.h"\
	{$(INCLUDE)}"\sys\Utime.h"\
	".\windows-NT\ndir.h"\
	
NODEP_CPP_PARSE=\
	".\src\popen.h"\
	".\lib\tcpip.h"\
	

"$(INTDIR)\parseinfo.obj" : $(SOURCE) $(DEP_CPP_PARSE) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\vers_ts.c
DEP_CPP_VERS_=\
	".\src\cvs.h"\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
	".\lib\fnmatch.h"\
	".\windows-NT\pwd.h"\
	".\lib\system.h"\
	".\src\hash.h"\
	".\src\client.h"\
	".\src\myndbm.h"\
	".\lib\regex.h"\
	".\lib\getopt.h"\
	".\lib\wait.h"\
	".\src\rcs.h"\
	".\src\update.h"\
	".\src\server.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	{$(INCLUDE)}"\sys\Timeb.h"\
	{$(INCLUDE)}"\sys\Utime.h"\
	".\windows-NT\ndir.h"\
	
NODEP_CPP_VERS_=\
	".\src\popen.h"\
	".\lib\tcpip.h"\
	

"$(INTDIR)\vers_ts.obj" : $(SOURCE) $(DEP_CPP_VERS_) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\checkin.c
DEP_CPP_CHECKI=\
	".\src\cvs.h"\
	".\src\fileattr.h"\
	".\src\edit.h"\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
	".\lib\fnmatch.h"\
	".\windows-NT\pwd.h"\
	".\lib\system.h"\
	".\src\hash.h"\
	".\src\client.h"\
	".\src\myndbm.h"\
	".\lib\regex.h"\
	".\lib\getopt.h"\
	".\lib\wait.h"\
	".\src\rcs.h"\
	".\src\update.h"\
	".\src\server.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	{$(INCLUDE)}"\sys\Timeb.h"\
	{$(INCLUDE)}"\sys\Utime.h"\
	".\windows-NT\ndir.h"\
	
NODEP_CPP_CHECKI=\
	".\src\popen.h"\
	".\lib\tcpip.h"\
	

"$(INTDIR)\checkin.obj" : $(SOURCE) $(DEP_CPP_CHECKI) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\commit.c
DEP_CPP_COMMI=\
	".\src\cvs.h"\
	".\lib\getline.h"\
	".\src\edit.h"\
	".\src\fileattr.h"\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
	".\lib\fnmatch.h"\
	".\windows-NT\pwd.h"\
	".\lib\system.h"\
	".\src\hash.h"\
	".\src\client.h"\
	".\src\myndbm.h"\
	".\lib\regex.h"\
	".\lib\getopt.h"\
	".\lib\wait.h"\
	".\src\rcs.h"\
	".\src\update.h"\
	".\src\server.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	{$(INCLUDE)}"\sys\Timeb.h"\
	{$(INCLUDE)}"\sys\Utime.h"\
	".\windows-NT\ndir.h"\
	
NODEP_CPP_COMMI=\
	".\src\popen.h"\
	".\lib\tcpip.h"\
	

"$(INTDIR)\commit.obj" : $(SOURCE) $(DEP_CPP_COMMI) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\version.c
DEP_CPP_VERSI=\
	".\src\cvs.h"\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
	".\lib\fnmatch.h"\
	".\windows-NT\pwd.h"\
	".\lib\system.h"\
	".\src\hash.h"\
	".\src\client.h"\
	".\src\myndbm.h"\
	".\lib\regex.h"\
	".\lib\getopt.h"\
	".\lib\wait.h"\
	".\src\rcs.h"\
	".\src\update.h"\
	".\src\server.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	{$(INCLUDE)}"\sys\Timeb.h"\
	{$(INCLUDE)}"\sys\Utime.h"\
	".\windows-NT\ndir.h"\
	
NODEP_CPP_VERSI=\
	".\src\popen.h"\
	".\lib\tcpip.h"\
	
INTDIR_SRC=.\WinRel\1
"$(INTDIR_SRC)" :
    if not exist "$(INTDIR_SRC)/$(NULL)" mkdir "$(INTDIR_SRC)"

!IF  "$(CFG)" == "cvsnt - Win32 Release"

# PROP Intermediate_Dir "WinRel\1"

".\WinRel\1\version.obj" : $(SOURCE) $(DEP_CPP_VERSI) "$(INTDIR_SRC)"
   $(CPP) /nologo /ML /W3 /GX /Ob1 /I "windows-NT" /I "lib" /I "src" /I "zlib"\
 /D "NDEBUG" /D "WIN32" /D "_CONSOLE" /D "HAVE_CONFIG_H" /Fp"WinRel/cvsnt.pch"\
 /YX /Fo"$(INTDIR_SRC)/" /c $(SOURCE)


!ELSEIF  "$(CFG)" == "cvsnt - Win32 Debug"

# PROP Intermediate_Dir "WinDebug\1"

".\WinDebug\1\version.obj" : $(SOURCE) $(DEP_CPP_VERSI) "$(INTDIR_SRC)"
   $(CPP) /nologo /MLd /W3 /Gm /GX /Zi /Ob1 /I "windows-NT" /I "lib" /I "src"\
 /I "zlib" /D "_DEBUG" /D "WIN32" /D "_CONSOLE" /D "HAVE_CONFIG_H"\
 /Fp"WinDebug/cvsnt.pch" /YX /Fo"$(INTDIR_SRC)/" /Fd"$(INTDIR_SRC)/" /c\
 $(SOURCE)


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\cvsrc.c
DEP_CPP_CVSRC=\
	".\src\cvs.h"\
	".\lib\getline.h"\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
	".\lib\fnmatch.h"\
	".\windows-NT\pwd.h"\
	".\lib\system.h"\
	".\src\hash.h"\
	".\src\client.h"\
	".\src\myndbm.h"\
	".\lib\regex.h"\
	".\lib\getopt.h"\
	".\lib\wait.h"\
	".\src\rcs.h"\
	".\src\update.h"\
	".\src\server.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	{$(INCLUDE)}"\sys\Timeb.h"\
	{$(INCLUDE)}"\sys\Utime.h"\
	".\windows-NT\ndir.h"\
	
NODEP_CPP_CVSRC=\
	".\src\popen.h"\
	".\lib\tcpip.h"\
	

"$(INTDIR)\cvsrc.obj" : $(SOURCE) $(DEP_CPP_CVSRC) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\remove.c
DEP_CPP_REMOV=\
	".\src\cvs.h"\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
	".\lib\fnmatch.h"\
	".\windows-NT\pwd.h"\
	".\lib\system.h"\
	".\src\hash.h"\
	".\src\client.h"\
	".\src\myndbm.h"\
	".\lib\regex.h"\
	".\lib\getopt.h"\
	".\lib\wait.h"\
	".\src\rcs.h"\
	".\src\update.h"\
	".\src\server.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	{$(INCLUDE)}"\sys\Timeb.h"\
	{$(INCLUDE)}"\sys\Utime.h"\
	".\windows-NT\ndir.h"\
	
NODEP_CPP_REMOV=\
	".\src\popen.h"\
	".\lib\tcpip.h"\
	

"$(INTDIR)\remove.obj" : $(SOURCE) $(DEP_CPP_REMOV) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\update.c
DEP_CPP_UPDAT=\
	".\src\cvs.h"\
	".\lib\savecwd.h"\
	".\lib\md5.h"\
	".\src\watch.h"\
	".\src\fileattr.h"\
	".\src\edit.h"\
	".\lib\getline.h"\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
	".\lib\fnmatch.h"\
	".\windows-NT\pwd.h"\
	".\lib\system.h"\
	".\src\hash.h"\
	".\src\client.h"\
	".\src\myndbm.h"\
	".\lib\regex.h"\
	".\lib\getopt.h"\
	".\lib\wait.h"\
	".\src\rcs.h"\
	".\src\update.h"\
	".\src\server.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	{$(INCLUDE)}"\sys\Timeb.h"\
	{$(INCLUDE)}"\sys\Utime.h"\
	".\windows-NT\ndir.h"\
	
NODEP_CPP_UPDAT=\
	".\src\popen.h"\
	".\lib\tcpip.h"\
	

"$(INTDIR)\update.obj" : $(SOURCE) $(DEP_CPP_UPDAT) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\logmsg.c
DEP_CPP_LOGMS=\
	".\src\cvs.h"\
	".\lib\getline.h"\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
	".\lib\fnmatch.h"\
	".\windows-NT\pwd.h"\
	".\lib\system.h"\
	".\src\hash.h"\
	".\src\client.h"\
	".\src\myndbm.h"\
	".\lib\regex.h"\
	".\lib\getopt.h"\
	".\lib\wait.h"\
	".\src\rcs.h"\
	".\src\update.h"\
	".\src\server.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	{$(INCLUDE)}"\sys\Timeb.h"\
	{$(INCLUDE)}"\sys\Utime.h"\
	".\windows-NT\ndir.h"\
	
NODEP_CPP_LOGMS=\
	".\src\popen.h"\
	".\lib\tcpip.h"\
	

"$(INTDIR)\logmsg.obj" : $(SOURCE) $(DEP_CPP_LOGMS) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\classify.c
DEP_CPP_CLASS=\
	".\src\cvs.h"\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
	".\lib\fnmatch.h"\
	".\windows-NT\pwd.h"\
	".\lib\system.h"\
	".\src\hash.h"\
	".\src\client.h"\
	".\src\myndbm.h"\
	".\lib\regex.h"\
	".\lib\getopt.h"\
	".\lib\wait.h"\
	".\src\rcs.h"\
	".\src\update.h"\
	".\src\server.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	{$(INCLUDE)}"\sys\Timeb.h"\
	{$(INCLUDE)}"\sys\Utime.h"\
	".\windows-NT\ndir.h"\
	
NODEP_CPP_CLASS=\
	".\src\popen.h"\
	".\lib\tcpip.h"\
	

"$(INTDIR)\classify.obj" : $(SOURCE) $(DEP_CPP_CLASS) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\history.c
DEP_CPP_HISTO=\
	".\src\cvs.h"\
	".\lib\savecwd.h"\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
	".\lib\fnmatch.h"\
	".\windows-NT\pwd.h"\
	".\lib\system.h"\
	".\src\hash.h"\
	".\src\client.h"\
	".\src\myndbm.h"\
	".\lib\regex.h"\
	".\lib\getopt.h"\
	".\lib\wait.h"\
	".\src\rcs.h"\
	".\src\update.h"\
	".\src\server.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	{$(INCLUDE)}"\sys\Timeb.h"\
	{$(INCLUDE)}"\sys\Utime.h"\
	".\windows-NT\ndir.h"\
	
NODEP_CPP_HISTO=\
	".\src\popen.h"\
	".\lib\tcpip.h"\
	

"$(INTDIR)\history.obj" : $(SOURCE) $(DEP_CPP_HISTO) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\add.c
DEP_CPP_ADD_C=\
	".\src\cvs.h"\
	".\lib\savecwd.h"\
	".\src\fileattr.h"\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
	".\lib\fnmatch.h"\
	".\windows-NT\pwd.h"\
	".\lib\system.h"\
	".\src\hash.h"\
	".\src\client.h"\
	".\src\myndbm.h"\
	".\lib\regex.h"\
	".\lib\getopt.h"\
	".\lib\wait.h"\
	".\src\rcs.h"\
	".\src\update.h"\
	".\src\server.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	{$(INCLUDE)}"\sys\Timeb.h"\
	{$(INCLUDE)}"\sys\Utime.h"\
	".\windows-NT\ndir.h"\
	
NODEP_CPP_ADD_C=\
	".\src\popen.h"\
	".\lib\tcpip.h"\
	

"$(INTDIR)\add.obj" : $(SOURCE) $(DEP_CPP_ADD_C) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\lock.c
DEP_CPP_LOCK_=\
	".\src\cvs.h"\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
	".\lib\fnmatch.h"\
	".\windows-NT\pwd.h"\
	".\lib\system.h"\
	".\src\hash.h"\
	".\src\client.h"\
	".\src\myndbm.h"\
	".\lib\regex.h"\
	".\lib\getopt.h"\
	".\lib\wait.h"\
	".\src\rcs.h"\
	".\src\update.h"\
	".\src\server.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	{$(INCLUDE)}"\sys\Timeb.h"\
	{$(INCLUDE)}"\sys\Utime.h"\
	".\windows-NT\ndir.h"\
	
NODEP_CPP_LOCK_=\
	".\src\popen.h"\
	".\lib\tcpip.h"\
	

"$(INTDIR)\lock.obj" : $(SOURCE) $(DEP_CPP_LOCK_) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\recurse.c
DEP_CPP_RECUR=\
	".\src\cvs.h"\
	".\lib\savecwd.h"\
	".\src\fileattr.h"\
	".\src\edit.h"\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
	".\lib\fnmatch.h"\
	".\windows-NT\pwd.h"\
	".\lib\system.h"\
	".\src\hash.h"\
	".\src\client.h"\
	".\src\myndbm.h"\
	".\lib\regex.h"\
	".\lib\getopt.h"\
	".\lib\wait.h"\
	".\src\rcs.h"\
	".\src\update.h"\
	".\src\server.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	{$(INCLUDE)}"\sys\Timeb.h"\
	{$(INCLUDE)}"\sys\Utime.h"\
	".\windows-NT\ndir.h"\
	
NODEP_CPP_RECUR=\
	".\src\popen.h"\
	".\lib\tcpip.h"\
	

"$(INTDIR)\recurse.obj" : $(SOURCE) $(DEP_CPP_RECUR) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\modules.c
DEP_CPP_MODUL=\
	".\src\cvs.h"\
	".\lib\savecwd.h"\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
	".\lib\fnmatch.h"\
	".\windows-NT\pwd.h"\
	".\lib\system.h"\
	".\src\hash.h"\
	".\src\client.h"\
	".\src\myndbm.h"\
	".\lib\regex.h"\
	".\lib\getopt.h"\
	".\lib\wait.h"\
	".\src\rcs.h"\
	".\src\update.h"\
	".\src\server.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	{$(INCLUDE)}"\sys\Timeb.h"\
	{$(INCLUDE)}"\sys\Utime.h"\
	".\windows-NT\ndir.h"\
	
NODEP_CPP_MODUL=\
	".\src\popen.h"\
	".\lib\tcpip.h"\
	

"$(INTDIR)\modules.obj" : $(SOURCE) $(DEP_CPP_MODUL) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\find_names.c
DEP_CPP_FIND_=\
	".\src\cvs.h"\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
	".\lib\fnmatch.h"\
	".\windows-NT\pwd.h"\
	".\lib\system.h"\
	".\src\hash.h"\
	".\src\client.h"\
	".\src\myndbm.h"\
	".\lib\regex.h"\
	".\lib\getopt.h"\
	".\lib\wait.h"\
	".\src\rcs.h"\
	".\src\update.h"\
	".\src\server.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	{$(INCLUDE)}"\sys\Timeb.h"\
	{$(INCLUDE)}"\sys\Utime.h"\
	".\windows-NT\ndir.h"\
	
NODEP_CPP_FIND_=\
	".\src\popen.h"\
	".\lib\tcpip.h"\
	

"$(INTDIR)\find_names.obj" : $(SOURCE) $(DEP_CPP_FIND_) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\rcs.c
DEP_CPP_RCS_C=\
	".\src\cvs.h"\
	".\src\edit.h"\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
	".\lib\fnmatch.h"\
	".\windows-NT\pwd.h"\
	".\lib\system.h"\
	".\src\hash.h"\
	".\src\client.h"\
	".\src\myndbm.h"\
	".\lib\regex.h"\
	".\lib\getopt.h"\
	".\lib\wait.h"\
	".\src\rcs.h"\
	".\src\update.h"\
	".\src\server.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	{$(INCLUDE)}"\sys\Timeb.h"\
	{$(INCLUDE)}"\sys\Utime.h"\
	".\windows-NT\ndir.h"\
	
NODEP_CPP_RCS_C=\
	".\src\popen.h"\
	".\lib\tcpip.h"\
	

"$(INTDIR)\rcs.obj" : $(SOURCE) $(DEP_CPP_RCS_C) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\create_adm.c
DEP_CPP_CREAT=\
	".\src\cvs.h"\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
	".\lib\fnmatch.h"\
	".\windows-NT\pwd.h"\
	".\lib\system.h"\
	".\src\hash.h"\
	".\src\client.h"\
	".\src\myndbm.h"\
	".\lib\regex.h"\
	".\lib\getopt.h"\
	".\lib\wait.h"\
	".\src\rcs.h"\
	".\src\update.h"\
	".\src\server.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	{$(INCLUDE)}"\sys\Timeb.h"\
	{$(INCLUDE)}"\sys\Utime.h"\
	".\windows-NT\ndir.h"\
	
NODEP_CPP_CREAT=\
	".\src\popen.h"\
	".\lib\tcpip.h"\
	

"$(INTDIR)\create_adm.obj" : $(SOURCE) $(DEP_CPP_CREAT) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\main.c
DEP_CPP_MAIN_=\
	".\src\cvs.h"\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
	".\lib\fnmatch.h"\
	".\windows-NT\pwd.h"\
	".\lib\system.h"\
	".\src\hash.h"\
	".\src\client.h"\
	".\src\myndbm.h"\
	".\lib\regex.h"\
	".\lib\getopt.h"\
	".\lib\wait.h"\
	".\src\rcs.h"\
	".\src\update.h"\
	".\src\server.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	{$(INCLUDE)}"\sys\Timeb.h"\
	{$(INCLUDE)}"\sys\Utime.h"\
	".\windows-NT\ndir.h"\
	
NODEP_CPP_MAIN_=\
	".\src\popen.h"\
	".\lib\tcpip.h"\
	

"$(INTDIR)\main.obj" : $(SOURCE) $(DEP_CPP_MAIN_) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\patch.c
DEP_CPP_PATCH=\
	".\src\cvs.h"\
	".\lib\getline.h"\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
	".\lib\fnmatch.h"\
	".\windows-NT\pwd.h"\
	".\lib\system.h"\
	".\src\hash.h"\
	".\src\client.h"\
	".\src\myndbm.h"\
	".\lib\regex.h"\
	".\lib\getopt.h"\
	".\lib\wait.h"\
	".\src\rcs.h"\
	".\src\update.h"\
	".\src\server.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	{$(INCLUDE)}"\sys\Timeb.h"\
	{$(INCLUDE)}"\sys\Utime.h"\
	".\windows-NT\ndir.h"\
	
NODEP_CPP_PATCH=\
	".\src\popen.h"\
	".\lib\tcpip.h"\
	

"$(INTDIR)\patch.obj" : $(SOURCE) $(DEP_CPP_PATCH) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\release.c
DEP_CPP_RELEA=\
	".\src\cvs.h"\
	".\lib\getline.h"\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
	".\lib\fnmatch.h"\
	".\windows-NT\pwd.h"\
	".\lib\system.h"\
	".\src\hash.h"\
	".\src\client.h"\
	".\src\myndbm.h"\
	".\lib\regex.h"\
	".\lib\getopt.h"\
	".\lib\wait.h"\
	".\src\rcs.h"\
	".\src\update.h"\
	".\src\server.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	{$(INCLUDE)}"\sys\Timeb.h"\
	{$(INCLUDE)}"\sys\Utime.h"\
	".\windows-NT\ndir.h"\
	
NODEP_CPP_RELEA=\
	".\src\popen.h"\
	".\lib\tcpip.h"\
	

"$(INTDIR)\release.obj" : $(SOURCE) $(DEP_CPP_RELEA) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\rcscmds.c
DEP_CPP_RCSCM=\
	".\src\cvs.h"\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
	".\lib\fnmatch.h"\
	".\windows-NT\pwd.h"\
	".\lib\system.h"\
	".\src\hash.h"\
	".\src\client.h"\
	".\src\myndbm.h"\
	".\lib\regex.h"\
	".\lib\getopt.h"\
	".\lib\wait.h"\
	".\src\rcs.h"\
	".\src\update.h"\
	".\src\server.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	{$(INCLUDE)}"\sys\Timeb.h"\
	{$(INCLUDE)}"\sys\Utime.h"\
	".\windows-NT\ndir.h"\
	
NODEP_CPP_RCSCM=\
	".\src\popen.h"\
	".\lib\tcpip.h"\
	

"$(INTDIR)\rcscmds.obj" : $(SOURCE) $(DEP_CPP_RCSCM) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\import.c
DEP_CPP_IMPOR=\
	".\src\cvs.h"\
	".\lib\savecwd.h"\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
	".\lib\fnmatch.h"\
	".\windows-NT\pwd.h"\
	".\lib\system.h"\
	".\src\hash.h"\
	".\src\client.h"\
	".\src\myndbm.h"\
	".\lib\regex.h"\
	".\lib\getopt.h"\
	".\lib\wait.h"\
	".\src\rcs.h"\
	".\src\update.h"\
	".\src\server.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	{$(INCLUDE)}"\sys\Timeb.h"\
	{$(INCLUDE)}"\sys\Utime.h"\
	".\windows-NT\ndir.h"\
	
NODEP_CPP_IMPOR=\
	".\src\popen.h"\
	".\lib\tcpip.h"\
	

"$(INTDIR)\import.obj" : $(SOURCE) $(DEP_CPP_IMPOR) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\ignore.c
DEP_CPP_IGNOR=\
	".\src\cvs.h"\
	".\lib\getline.h"\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
	".\lib\fnmatch.h"\
	".\windows-NT\pwd.h"\
	".\lib\system.h"\
	".\src\hash.h"\
	".\src\client.h"\
	".\src\myndbm.h"\
	".\lib\regex.h"\
	".\lib\getopt.h"\
	".\lib\wait.h"\
	".\src\rcs.h"\
	".\src\update.h"\
	".\src\server.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	{$(INCLUDE)}"\sys\Timeb.h"\
	{$(INCLUDE)}"\sys\Utime.h"\
	".\windows-NT\ndir.h"\
	
NODEP_CPP_IGNOR=\
	".\src\popen.h"\
	".\lib\tcpip.h"\
	

"$(INTDIR)\ignore.obj" : $(SOURCE) $(DEP_CPP_IGNOR) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\log.c
DEP_CPP_LOG_C=\
	".\src\cvs.h"\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
	".\lib\fnmatch.h"\
	".\windows-NT\pwd.h"\
	".\lib\system.h"\
	".\src\hash.h"\
	".\src\client.h"\
	".\src\myndbm.h"\
	".\lib\regex.h"\
	".\lib\getopt.h"\
	".\lib\wait.h"\
	".\src\rcs.h"\
	".\src\update.h"\
	".\src\server.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	{$(INCLUDE)}"\sys\Timeb.h"\
	{$(INCLUDE)}"\sys\Utime.h"\
	".\windows-NT\ndir.h"\
	
NODEP_CPP_LOG_C=\
	".\src\popen.h"\
	".\lib\tcpip.h"\
	

"$(INTDIR)\log.obj" : $(SOURCE) $(DEP_CPP_LOG_C) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\wrapper.c
DEP_CPP_WRAPP=\
	".\src\cvs.h"\
	".\lib\getline.h"\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
	".\lib\fnmatch.h"\
	".\windows-NT\pwd.h"\
	".\lib\system.h"\
	".\src\hash.h"\
	".\src\client.h"\
	".\src\myndbm.h"\
	".\lib\regex.h"\
	".\lib\getopt.h"\
	".\lib\wait.h"\
	".\src\rcs.h"\
	".\src\update.h"\
	".\src\server.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	{$(INCLUDE)}"\sys\Timeb.h"\
	{$(INCLUDE)}"\sys\Utime.h"\
	".\windows-NT\ndir.h"\
	
NODEP_CPP_WRAPP=\
	".\src\popen.h"\
	".\lib\tcpip.h"\
	

"$(INTDIR)\wrapper.obj" : $(SOURCE) $(DEP_CPP_WRAPP) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\error.c
DEP_CPP_ERROR=\
	".\src\cvs.h"\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
	".\lib\fnmatch.h"\
	".\windows-NT\pwd.h"\
	".\lib\system.h"\
	".\src\hash.h"\
	".\src\client.h"\
	".\src\myndbm.h"\
	".\lib\regex.h"\
	".\lib\getopt.h"\
	".\lib\wait.h"\
	".\src\rcs.h"\
	".\src\update.h"\
	".\src\server.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	{$(INCLUDE)}"\sys\Timeb.h"\
	{$(INCLUDE)}"\sys\Utime.h"\
	".\windows-NT\ndir.h"\
	
NODEP_CPP_ERROR=\
	".\src\popen.h"\
	".\lib\tcpip.h"\
	

"$(INTDIR)\error.obj" : $(SOURCE) $(DEP_CPP_ERROR) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\expand_path.c
DEP_CPP_EXPAN=\
	".\src\cvs.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
	".\lib\fnmatch.h"\
	".\windows-NT\pwd.h"\
	".\lib\system.h"\
	".\src\hash.h"\
	".\src\client.h"\
	".\src\myndbm.h"\
	".\lib\regex.h"\
	".\lib\getopt.h"\
	".\lib\wait.h"\
	".\src\rcs.h"\
	".\src\update.h"\
	".\src\server.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	{$(INCLUDE)}"\sys\Timeb.h"\
	{$(INCLUDE)}"\sys\Utime.h"\
	".\windows-NT\ndir.h"\
	
NODEP_CPP_EXPAN=\
	".\src\popen.h"\
	".\lib\tcpip.h"\
	

"$(INTDIR)\expand_path.obj" : $(SOURCE) $(DEP_CPP_EXPAN) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\edit.c
DEP_CPP_EDIT_=\
	".\src\cvs.h"\
	".\lib\getline.h"\
	".\src\watch.h"\
	".\src\edit.h"\
	".\src\fileattr.h"\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
	".\lib\fnmatch.h"\
	".\windows-NT\pwd.h"\
	".\lib\system.h"\
	".\src\hash.h"\
	".\src\client.h"\
	".\src\myndbm.h"\
	".\lib\regex.h"\
	".\lib\getopt.h"\
	".\lib\wait.h"\
	".\src\rcs.h"\
	".\src\update.h"\
	".\src\server.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	{$(INCLUDE)}"\sys\Timeb.h"\
	{$(INCLUDE)}"\sys\Utime.h"\
	".\windows-NT\ndir.h"\
	
NODEP_CPP_EDIT_=\
	".\src\popen.h"\
	".\lib\tcpip.h"\
	

"$(INTDIR)\edit.obj" : $(SOURCE) $(DEP_CPP_EDIT_) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\fileattr.c
DEP_CPP_FILEA=\
	".\src\cvs.h"\
	".\lib\getline.h"\
	".\src\fileattr.h"\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
	".\lib\fnmatch.h"\
	".\windows-NT\pwd.h"\
	".\lib\system.h"\
	".\src\hash.h"\
	".\src\client.h"\
	".\src\myndbm.h"\
	".\lib\regex.h"\
	".\lib\getopt.h"\
	".\lib\wait.h"\
	".\src\rcs.h"\
	".\src\update.h"\
	".\src\server.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	{$(INCLUDE)}"\sys\Timeb.h"\
	{$(INCLUDE)}"\sys\Utime.h"\
	".\windows-NT\ndir.h"\
	
NODEP_CPP_FILEA=\
	".\src\popen.h"\
	".\lib\tcpip.h"\
	

"$(INTDIR)\fileattr.obj" : $(SOURCE) $(DEP_CPP_FILEA) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\watch.c
DEP_CPP_WATCH=\
	".\src\cvs.h"\
	".\src\edit.h"\
	".\src\fileattr.h"\
	".\src\watch.h"\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
	".\lib\fnmatch.h"\
	".\windows-NT\pwd.h"\
	".\lib\system.h"\
	".\src\hash.h"\
	".\src\client.h"\
	".\src\myndbm.h"\
	".\lib\regex.h"\
	".\lib\getopt.h"\
	".\lib\wait.h"\
	".\src\rcs.h"\
	".\src\update.h"\
	".\src\server.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	{$(INCLUDE)}"\sys\Timeb.h"\
	{$(INCLUDE)}"\sys\Utime.h"\
	".\windows-NT\ndir.h"\
	
NODEP_CPP_WATCH=\
	".\src\popen.h"\
	".\lib\tcpip.h"\
	

"$(INTDIR)\watch.obj" : $(SOURCE) $(DEP_CPP_WATCH) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\login.c
DEP_CPP_LOGIN=\
	".\src\cvs.h"\
	".\lib\getline.h"\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
	".\lib\fnmatch.h"\
	".\windows-NT\pwd.h"\
	".\lib\system.h"\
	".\src\hash.h"\
	".\src\client.h"\
	".\src\myndbm.h"\
	".\lib\regex.h"\
	".\lib\getopt.h"\
	".\lib\wait.h"\
	".\src\rcs.h"\
	".\src\update.h"\
	".\src\server.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	{$(INCLUDE)}"\sys\Timeb.h"\
	{$(INCLUDE)}"\sys\Utime.h"\
	".\windows-NT\ndir.h"\
	
NODEP_CPP_LOGIN=\
	".\src\popen.h"\
	".\lib\tcpip.h"\
	

"$(INTDIR)\login.obj" : $(SOURCE) $(DEP_CPP_LOGIN) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\scramble.c
DEP_CPP_SCRAM=\
	".\src\cvs.h"\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
	".\lib\fnmatch.h"\
	".\windows-NT\pwd.h"\
	".\lib\system.h"\
	".\src\hash.h"\
	".\src\client.h"\
	".\src\myndbm.h"\
	".\lib\regex.h"\
	".\lib\getopt.h"\
	".\lib\wait.h"\
	".\src\rcs.h"\
	".\src\update.h"\
	".\src\server.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	{$(INCLUDE)}"\sys\Timeb.h"\
	{$(INCLUDE)}"\sys\Utime.h"\
	".\windows-NT\ndir.h"\
	
NODEP_CPP_SCRAM=\
	".\src\popen.h"\
	".\lib\tcpip.h"\
	

"$(INTDIR)\scramble.obj" : $(SOURCE) $(DEP_CPP_SCRAM) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\buffer.c
DEP_CPP_BUFFE=\
	".\src\cvs.h"\
	".\src\buffer.h"\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
	".\lib\fnmatch.h"\
	".\windows-NT\pwd.h"\
	".\lib\system.h"\
	".\src\hash.h"\
	".\src\client.h"\
	".\src\myndbm.h"\
	".\lib\regex.h"\
	".\lib\getopt.h"\
	".\lib\wait.h"\
	".\src\rcs.h"\
	".\src\update.h"\
	".\src\server.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	{$(INCLUDE)}"\sys\Timeb.h"\
	{$(INCLUDE)}"\sys\Utime.h"\
	".\windows-NT\ndir.h"\
	
NODEP_CPP_BUFFE=\
	".\src\popen.h"\
	".\lib\tcpip.h"\
	

"$(INTDIR)\buffer.obj" : $(SOURCE) $(DEP_CPP_BUFFE) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\zlib.c
DEP_CPP_ZLIB_=\
	".\src\cvs.h"\
	".\src\buffer.h"\
	".\zlib\zlib.h"\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
	".\lib\fnmatch.h"\
	".\windows-NT\pwd.h"\
	".\lib\system.h"\
	".\src\hash.h"\
	".\src\client.h"\
	".\src\myndbm.h"\
	".\lib\regex.h"\
	".\lib\getopt.h"\
	".\lib\wait.h"\
	".\src\rcs.h"\
	".\src\update.h"\
	".\src\server.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	{$(INCLUDE)}"\sys\Timeb.h"\
	{$(INCLUDE)}"\sys\Utime.h"\
	".\windows-NT\ndir.h"\
	".\zlib\zconf.h"\
	
NODEP_CPP_ZLIB_=\
	".\src\popen.h"\
	".\lib\tcpip.h"\
	

"$(INTDIR)\zlib.obj" : $(SOURCE) $(DEP_CPP_ZLIB_) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\src\ChangeLog

!IF  "$(CFG)" == "cvsnt - Win32 Release"

!ELSEIF  "$(CFG)" == "cvsnt - Win32 Debug"

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\lib\sighandle.c
DEP_CPP_SIGHA=\
	".\windows-NT\config.h"\
	".\lib\system.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	{$(INCLUDE)}"\sys\Timeb.h"\
	{$(INCLUDE)}"\sys\Utime.h"\
	".\windows-NT\ndir.h"\
	
NODEP_CPP_SIGHA=\
	".\lib\tcpip.h"\
	

"$(INTDIR)\sighandle.obj" : $(SOURCE) $(DEP_CPP_SIGHA) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\lib\getopt.c
DEP_CPP_GETOP=\
	".\windows-NT\config.h"\
	".\lib\getopt.h"\
	

"$(INTDIR)\getopt.obj" : $(SOURCE) $(DEP_CPP_GETOP) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\lib\argmatch.c
DEP_CPP_ARGMA=\
	".\windows-NT\config.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	

"$(INTDIR)\argmatch.obj" : $(SOURCE) $(DEP_CPP_ARGMA) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\lib\md5.c
DEP_CPP_MD5_C=\
	".\windows-NT\config.h"\
	".\lib\md5.h"\
	

"$(INTDIR)\md5.obj" : $(SOURCE) $(DEP_CPP_MD5_C) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\lib\yesno.c
DEP_CPP_YESNO=\
	".\windows-NT\config.h"\
	

"$(INTDIR)\yesno.obj" : $(SOURCE) $(DEP_CPP_YESNO) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\lib\getopt1.c
DEP_CPP_GETOPT=\
	".\windows-NT\config.h"\
	".\lib\getopt.h"\
	

"$(INTDIR)\getopt1.obj" : $(SOURCE) $(DEP_CPP_GETOPT) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\lib\valloc.c
DEP_CPP_VALLO=\
	".\windows-NT\config.h"\
	".\lib\system.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	{$(INCLUDE)}"\sys\Timeb.h"\
	{$(INCLUDE)}"\sys\Utime.h"\
	".\windows-NT\ndir.h"\
	
NODEP_CPP_VALLO=\
	".\lib\tcpip.h"\
	

"$(INTDIR)\valloc.obj" : $(SOURCE) $(DEP_CPP_VALLO) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\lib\xgetwd.c
DEP_CPP_XGETW=\
	".\windows-NT\config.h"\
	".\lib\system.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	{$(INCLUDE)}"\sys\Timeb.h"\
	{$(INCLUDE)}"\sys\Utime.h"\
	".\windows-NT\ndir.h"\
	
NODEP_CPP_XGETW=\
	".\lib\tcpip.h"\
	

"$(INTDIR)\xgetwd.obj" : $(SOURCE) $(DEP_CPP_XGETW) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\lib\regex.c
DEP_CPP_REGEX=\
	{$(INCLUDE)}"\sys\Types.h"\
	".\windows-NT\config.h"\
	".\src\buffer.h"\
	".\lib\regex.h"\
	
NODEP_CPP_REGEX=\
	".\lib\lisp.h"\
	".\lib\syntax.h"\
	

"$(INTDIR)\regex.obj" : $(SOURCE) $(DEP_CPP_REGEX) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\lib\fnmatch.c
DEP_CPP_FNMAT=\
	".\windows-NT\config.h"\
	".\lib\fnmatch.h"\
	

"$(INTDIR)\fnmatch.obj" : $(SOURCE) $(DEP_CPP_FNMAT) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\lib\getdate.c
DEP_CPP_GETDA=\
	".\windows-NT\config.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Timeb.h"\
	

"$(INTDIR)\getdate.obj" : $(SOURCE) $(DEP_CPP_GETDA) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\lib\getline.c
DEP_CPP_GETLI=\
	".\windows-NT\config.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	

"$(INTDIR)\getline.obj" : $(SOURCE) $(DEP_CPP_GETLI) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\lib\savecwd.c
DEP_CPP_SAVEC=\
	".\windows-NT\config.h"\
	".\lib\savecwd.h"\
	

"$(INTDIR)\savecwd.obj" : $(SOURCE) $(DEP_CPP_SAVEC) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\lib\vasprintf.c
DEP_CPP_VASPR=\
	".\windows-NT\config.h"\
	

"$(INTDIR)\vasprintf.obj" : $(SOURCE) $(DEP_CPP_VASPR) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=".\windows-NT\mkdir.c"
DEP_CPP_MKDIR=\
	".\src\cvs.h"\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
	".\lib\fnmatch.h"\
	".\windows-NT\pwd.h"\
	".\lib\system.h"\
	".\src\hash.h"\
	".\src\client.h"\
	".\src\myndbm.h"\
	".\lib\regex.h"\
	".\lib\getopt.h"\
	".\lib\wait.h"\
	".\src\rcs.h"\
	".\src\update.h"\
	".\src\server.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	{$(INCLUDE)}"\sys\Timeb.h"\
	{$(INCLUDE)}"\sys\Utime.h"\
	".\windows-NT\ndir.h"\
	
NODEP_CPP_MKDIR=\
	".\src\popen.h"\
	".\lib\tcpip.h"\
	

"$(INTDIR)\mkdir.obj" : $(SOURCE) $(DEP_CPP_MKDIR) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=".\windows-NT\run.c"
DEP_CPP_RUN_C=\
	".\src\cvs.h"\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
	".\lib\fnmatch.h"\
	".\windows-NT\pwd.h"\
	".\lib\system.h"\
	".\src\hash.h"\
	".\src\client.h"\
	".\src\myndbm.h"\
	".\lib\regex.h"\
	".\lib\getopt.h"\
	".\lib\wait.h"\
	".\src\rcs.h"\
	".\src\update.h"\
	".\src\server.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	{$(INCLUDE)}"\sys\Timeb.h"\
	{$(INCLUDE)}"\sys\Utime.h"\
	".\windows-NT\ndir.h"\
	
NODEP_CPP_RUN_C=\
	".\src\popen.h"\
	".\lib\tcpip.h"\
	

"$(INTDIR)\run.obj" : $(SOURCE) $(DEP_CPP_RUN_C) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=".\windows-NT\pwd.c"
DEP_CPP_PWD_C=\
	".\windows-NT\pwd.h"\
	

"$(INTDIR)\pwd.obj" : $(SOURCE) $(DEP_CPP_PWD_C) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=".\windows-NT\filesubr.c"
DEP_CPP_FILES=\
	".\src\cvs.h"\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
	".\lib\fnmatch.h"\
	".\windows-NT\pwd.h"\
	".\lib\system.h"\
	".\src\hash.h"\
	".\src\client.h"\
	".\src\myndbm.h"\
	".\lib\regex.h"\
	".\lib\getopt.h"\
	".\lib\wait.h"\
	".\src\rcs.h"\
	".\src\update.h"\
	".\src\server.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	{$(INCLUDE)}"\sys\Timeb.h"\
	{$(INCLUDE)}"\sys\Utime.h"\
	".\windows-NT\ndir.h"\
	
NODEP_CPP_FILES=\
	".\src\popen.h"\
	".\lib\tcpip.h"\
	

"$(INTDIR)\filesubr.obj" : $(SOURCE) $(DEP_CPP_FILES) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=".\windows-NT\win32.c"
DEP_CPP_WIN32=\
	".\windows-NT\config.h"\
	

"$(INTDIR)\win32.obj" : $(SOURCE) $(DEP_CPP_WIN32) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=".\windows-NT\waitpid.c"
DEP_CPP_WAITP=\
	".\windows-NT\config.h"\
	

"$(INTDIR)\waitpid.obj" : $(SOURCE) $(DEP_CPP_WAITP) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=".\windows-NT\ndir.c"
DEP_CPP_NDIR_=\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	".\windows-NT\ndir.h"\
	

"$(INTDIR)\ndir.obj" : $(SOURCE) $(DEP_CPP_NDIR_) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=".\windows-NT\stripslash.c"

"$(INTDIR)\stripslash.obj" : $(SOURCE) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=".\windows-NT\rcmd.c"
DEP_CPP_RCMD_=\
	".\src\cvs.h"\
	".\windows-NT\rcmd.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
	".\lib\fnmatch.h"\
	".\windows-NT\pwd.h"\
	".\lib\system.h"\
	".\src\hash.h"\
	".\src\client.h"\
	".\src\myndbm.h"\
	".\lib\regex.h"\
	".\lib\getopt.h"\
	".\lib\wait.h"\
	".\src\rcs.h"\
	".\src\update.h"\
	".\src\server.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	{$(INCLUDE)}"\sys\Timeb.h"\
	{$(INCLUDE)}"\sys\Utime.h"\
	".\windows-NT\ndir.h"\
	
NODEP_CPP_RCMD_=\
	".\src\popen.h"\
	".\lib\tcpip.h"\
	

"$(INTDIR)\rcmd.obj" : $(SOURCE) $(DEP_CPP_RCMD_) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=".\windows-NT\startserver.c"
DEP_CPP_START=\
	".\src\cvs.h"\
	".\windows-NT\rcmd.h"\
	".\windows-NT\config.h"\
	".\windows-NT\options.h"\
	".\lib\fnmatch.h"\
	".\windows-NT\pwd.h"\
	".\lib\system.h"\
	".\src\hash.h"\
	".\src\client.h"\
	".\src\myndbm.h"\
	".\lib\regex.h"\
	".\lib\getopt.h"\
	".\lib\wait.h"\
	".\src\rcs.h"\
	".\src\update.h"\
	".\src\server.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	{$(INCLUDE)}"\sys\Timeb.h"\
	{$(INCLUDE)}"\sys\Utime.h"\
	".\windows-NT\ndir.h"\
	
NODEP_CPP_START=\
	".\src\popen.h"\
	".\lib\tcpip.h"\
	

"$(INTDIR)\startserver.obj" : $(SOURCE) $(DEP_CPP_START) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=".\windows-NT\ChangeLog"

!IF  "$(CFG)" == "cvsnt - Win32 Release"

!ELSEIF  "$(CFG)" == "cvsnt - Win32 Debug"

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\zlib\zutil.c
DEP_CPP_ZUTIL=\
	".\zlib\zutil.h"\
	".\zlib\zlib.h"\
	".\zlib\zconf.h"\
	

"$(INTDIR)\zutil.obj" : $(SOURCE) $(DEP_CPP_ZUTIL) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\zlib\infutil.c
DEP_CPP_INFUT=\
	".\zlib\zutil.h"\
	".\zlib\infblock.h"\
	".\zlib\inftrees.h"\
	".\zlib\infcodes.h"\
	".\zlib\infutil.h"\
	".\zlib\zlib.h"\
	".\zlib\zconf.h"\
	

"$(INTDIR)\infutil.obj" : $(SOURCE) $(DEP_CPP_INFUT) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\zlib\infblock.c
DEP_CPP_INFBL=\
	".\zlib\zutil.h"\
	".\zlib\infblock.h"\
	".\zlib\inftrees.h"\
	".\zlib\infcodes.h"\
	".\zlib\infutil.h"\
	".\zlib\zlib.h"\
	".\zlib\zconf.h"\
	

"$(INTDIR)\infblock.obj" : $(SOURCE) $(DEP_CPP_INFBL) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\zlib\compress.c
DEP_CPP_COMPR=\
	".\zlib\zlib.h"\
	".\zlib\zconf.h"\
	

"$(INTDIR)\compress.obj" : $(SOURCE) $(DEP_CPP_COMPR) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\zlib\uncompr.c
DEP_CPP_UNCOM=\
	".\zlib\zlib.h"\
	".\zlib\zconf.h"\
	

"$(INTDIR)\uncompr.obj" : $(SOURCE) $(DEP_CPP_UNCOM) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\zlib\inflate.c
DEP_CPP_INFLA=\
	".\zlib\zutil.h"\
	".\zlib\infblock.h"\
	".\zlib\zlib.h"\
	".\zlib\zconf.h"\
	

"$(INTDIR)\inflate.obj" : $(SOURCE) $(DEP_CPP_INFLA) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\zlib\inftrees.c
DEP_CPP_INFTR=\
	".\zlib\zutil.h"\
	".\zlib\inftrees.h"\
	".\zlib\zlib.h"\
	".\zlib\zconf.h"\
	

"$(INTDIR)\inftrees.obj" : $(SOURCE) $(DEP_CPP_INFTR) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\zlib\gzio.c
DEP_CPP_GZIO_=\
	".\zlib\zutil.h"\
	".\zlib\zlib.h"\
	".\zlib\zconf.h"\
	

"$(INTDIR)\gzio.obj" : $(SOURCE) $(DEP_CPP_GZIO_) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\zlib\infcodes.c
DEP_CPP_INFCO=\
	".\zlib\zutil.h"\
	".\zlib\inftrees.h"\
	".\zlib\infblock.h"\
	".\zlib\infcodes.h"\
	".\zlib\infutil.h"\
	".\zlib\inffast.h"\
	".\zlib\zlib.h"\
	".\zlib\zconf.h"\
	

"$(INTDIR)\infcodes.obj" : $(SOURCE) $(DEP_CPP_INFCO) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\zlib\deflate.c
DEP_CPP_DEFLA=\
	".\zlib\deflate.h"\
	".\zlib\zutil.h"\
	".\zlib\zlib.h"\
	".\zlib\zconf.h"\
	

"$(INTDIR)\deflate.obj" : $(SOURCE) $(DEP_CPP_DEFLA) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\zlib\adler32.c
DEP_CPP_ADLER=\
	".\zlib\zlib.h"\
	".\zlib\zconf.h"\
	

"$(INTDIR)\adler32.obj" : $(SOURCE) $(DEP_CPP_ADLER) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\zlib\crc32.c
DEP_CPP_CRC32=\
	".\zlib\zlib.h"\
	".\zlib\zconf.h"\
	

"$(INTDIR)\crc32.obj" : $(SOURCE) $(DEP_CPP_CRC32) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\zlib\inffast.c
DEP_CPP_INFFA=\
	".\zlib\zutil.h"\
	".\zlib\inftrees.h"\
	".\zlib\infblock.h"\
	".\zlib\infcodes.h"\
	".\zlib\infutil.h"\
	".\zlib\inffast.h"\
	".\zlib\zlib.h"\
	".\zlib\zconf.h"\
	

"$(INTDIR)\inffast.obj" : $(SOURCE) $(DEP_CPP_INFFA) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\zlib\trees.c
DEP_CPP_TREES=\
	".\zlib\deflate.h"\
	".\zlib\zutil.h"\
	".\zlib\zlib.h"\
	".\zlib\zconf.h"\
	

"$(INTDIR)\trees.obj" : $(SOURCE) $(DEP_CPP_TREES) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=".\windows-NT\sockerror.c"

"$(INTDIR)\sockerror.obj" : $(SOURCE) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\diff\util.c
DEP_CPP_UTIL_=\
	".\diff\diff.h"\
	".\diff\system.h"\
	".\lib\regex.h"\
	".\windows-NT\config.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	".\windows-NT\ndir.h"\
	

"$(INTDIR)\util.obj" : $(SOURCE) $(DEP_CPP_UTIL_) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\diff\cmpbuf.c
DEP_CPP_CMPBU=\
	".\diff\system.h"\
	".\diff\cmpbuf.h"\
	".\windows-NT\config.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	".\windows-NT\ndir.h"\
	

"$(INTDIR)\cmpbuf.obj" : $(SOURCE) $(DEP_CPP_CMPBU) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\diff\context.c
DEP_CPP_CONTE=\
	".\diff\diff.h"\
	".\diff\system.h"\
	".\lib\regex.h"\
	".\windows-NT\config.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	".\windows-NT\ndir.h"\
	

"$(INTDIR)\context.obj" : $(SOURCE) $(DEP_CPP_CONTE) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\diff\diff.c
DEP_CPP_DIFF_=\
	".\diff\diff.h"\
	".\lib\getopt.h"\
	".\lib\fnmatch.h"\
	".\diff\system.h"\
	".\lib\regex.h"\
	".\windows-NT\config.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	".\windows-NT\ndir.h"\
	
INTDIR_SRC=.\WinRel\2
"$(INTDIR_SRC)" :
    if not exist "$(INTDIR_SRC)/$(NULL)" mkdir "$(INTDIR_SRC)"

!IF  "$(CFG)" == "cvsnt - Win32 Release"

# PROP Intermediate_Dir "WinRel\2"

".\WinRel\2\diff.obj" : $(SOURCE) $(DEP_CPP_DIFF_) "$(INTDIR_SRC)"
   $(CPP) /nologo /ML /W3 /GX /Ob1 /I "windows-NT" /I "lib" /I "src" /I "zlib"\
 /D "NDEBUG" /D "WIN32" /D "_CONSOLE" /D "HAVE_CONFIG_H" /Fp"WinRel/cvsnt.pch"\
 /YX /Fo"$(INTDIR_SRC)/" /c $(SOURCE)


!ELSEIF  "$(CFG)" == "cvsnt - Win32 Debug"

# PROP Intermediate_Dir "WinDebug\2"

".\WinDebug\2\diff.obj" : $(SOURCE) $(DEP_CPP_DIFF_) "$(INTDIR_SRC)"
   $(CPP) /nologo /MLd /W3 /Gm /GX /Zi /Ob1 /I "windows-NT" /I "lib" /I "src"\
 /I "zlib" /D "_DEBUG" /D "WIN32" /D "_CONSOLE" /D "HAVE_CONFIG_H"\
 /Fp"WinDebug/cvsnt.pch" /YX /Fo"$(INTDIR_SRC)/" /Fd"$(INTDIR_SRC)/" /c\
 $(SOURCE)


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\diff\diff3.c
DEP_CPP_DIFF3=\
	".\diff\system.h"\
	".\lib\getopt.h"\
	".\windows-NT\config.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	".\windows-NT\ndir.h"\
	

"$(INTDIR)\diff3.obj" : $(SOURCE) $(DEP_CPP_DIFF3) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\diff\dir.c
DEP_CPP_DIR_C=\
	".\diff\diff.h"\
	".\diff\system.h"\
	".\lib\regex.h"\
	".\windows-NT\config.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	".\windows-NT\ndir.h"\
	

"$(INTDIR)\dir.obj" : $(SOURCE) $(DEP_CPP_DIR_C) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\diff\ed.c
DEP_CPP_ED_Cba=\
	".\diff\diff.h"\
	".\diff\system.h"\
	".\lib\regex.h"\
	".\windows-NT\config.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	".\windows-NT\ndir.h"\
	

"$(INTDIR)\ed.obj" : $(SOURCE) $(DEP_CPP_ED_Cba) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\diff\ifdef.c
DEP_CPP_IFDEF=\
	".\diff\diff.h"\
	".\diff\system.h"\
	".\lib\regex.h"\
	".\windows-NT\config.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	".\windows-NT\ndir.h"\
	

"$(INTDIR)\ifdef.obj" : $(SOURCE) $(DEP_CPP_IFDEF) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\diff\io.c
DEP_CPP_IO_Cbe=\
	".\diff\diff.h"\
	".\diff\system.h"\
	".\lib\regex.h"\
	".\windows-NT\config.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	".\windows-NT\ndir.h"\
	

"$(INTDIR)\io.obj" : $(SOURCE) $(DEP_CPP_IO_Cbe) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\diff\normal.c
DEP_CPP_NORMA=\
	".\diff\diff.h"\
	".\diff\system.h"\
	".\lib\regex.h"\
	".\windows-NT\config.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	".\windows-NT\ndir.h"\
	

"$(INTDIR)\normal.obj" : $(SOURCE) $(DEP_CPP_NORMA) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\diff\side.c
DEP_CPP_SIDE_=\
	".\diff\diff.h"\
	".\diff\system.h"\
	".\lib\regex.h"\
	".\windows-NT\config.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	".\windows-NT\ndir.h"\
	

"$(INTDIR)\side.obj" : $(SOURCE) $(DEP_CPP_SIDE_) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\diff\analyze.c
DEP_CPP_ANALY=\
	".\diff\diff.h"\
	".\diff\cmpbuf.h"\
	".\diff\system.h"\
	".\lib\regex.h"\
	".\windows-NT\config.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	".\windows-NT\ndir.h"\
	

"$(INTDIR)\analyze.obj" : $(SOURCE) $(DEP_CPP_ANALY) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=.\diff\version.c
DEP_CPP_VERSI=\
	".\windows-NT\config.h"\
	
INTDIR_SRC=.\WinRel\2
"$(INTDIR_SRC)" :
    if not exist "$(INTDIR_SRC)/$(NULL)" mkdir "$(INTDIR_SRC)"

!IF  "$(CFG)" == "cvsnt - Win32 Release"

# PROP Intermediate_Dir "WinRel\2"

".\WinRel\2\version.obj" : $(SOURCE) $(DEP_CPP_VERSI) "$(INTDIR_SRC)"
   $(CPP) /nologo /ML /W3 /GX /Ob1 /I "windows-NT" /I "lib" /I "src" /I "zlib"\
 /D "NDEBUG" /D "WIN32" /D "_CONSOLE" /D "HAVE_CONFIG_H" /Fp"WinRel/cvsnt.pch"\
 /YX /Fo"$(INTDIR_SRC)/" /c $(SOURCE)


!ELSEIF  "$(CFG)" == "cvsnt - Win32 Debug"

# PROP Intermediate_Dir "WinDebug\2"

".\WinDebug\2\version.obj" : $(SOURCE) $(DEP_CPP_VERSI) "$(INTDIR_SRC)"
   $(CPP) /nologo /MLd /W3 /Gm /GX /Zi /Ob1 /I "windows-NT" /I "lib" /I "src"\
 /I "zlib" /D "_DEBUG" /D "WIN32" /D "_CONSOLE" /D "HAVE_CONFIG_H"\
 /Fp"WinDebug/cvsnt.pch" /YX /Fo"$(INTDIR_SRC)/" /Fd"$(INTDIR_SRC)/" /c\
 $(SOURCE)


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\lib\fncase.c
DEP_CPP_FNCAS=\
	".\windows-NT\config.h"\
	".\lib\system.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	{$(INCLUDE)}"\sys\Timeb.h"\
	{$(INCLUDE)}"\sys\Utime.h"\
	".\windows-NT\ndir.h"\
	
NODEP_CPP_FNCAS=\
	".\lib\tcpip.h"\
	

"$(INTDIR)\fncase.obj" : $(SOURCE) $(DEP_CPP_FNCAS) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
# End Target
# End Project
################################################################################
