use strict;

my $SYMBIAN_VERSION;
my $SYMBIAN_ROOT;
my $SDK_NAME;
my $SDK_VARIANT;
my $SDK_VERSION;
my $WIN;

if ($ENV{PATH} =~ m!\\Symbian\\(.+?)\\(.+?)\\Epoc32\\gcc\\bin!i) {
    $SYMBIAN_VERSION = $1;
    $SDK_NAME = $2;
    $WIN = ($SDK_NAME =~ m!_CW!i || $SDK_NAME eq '8.1a') ?
	'winscw' : 'wins';
    $ENV{WIN} = $WIN;
    if ($SDK_NAME =~ m!Series60_v20!) {
	$SDK_VARIANT = 'S60';
	$SDK_VERSION = $ENV{S60SDK} = '2.0';
    } elsif ($SDK_NAME =~ m!Series60_v21!) {
	$SDK_VARIANT = 'S60';
	$SDK_VERSION = $ENV{S60SDK} = '2.1';
    } elsif ($SDK_NAME =~ m!S60_2nd_FP2!) {
	$SDK_VARIANT = 'S60';
	$SDK_VERSION = $ENV{S60SDK} = '2.6';
    } elsif ($SDK_NAME =~ m!S60_2nd_FP3!) {
	$SDK_VARIANT = 'S60';
	$SDK_VERSION = $ENV{S60SDK} = '2.8';
    } elsif ($SDK_NAME =~ m!S80_DP2_0_SDK!) {
	$SDK_VARIANT = 'S80';
	$SDK_VERSION = $ENV{S80SDK} = '2.0';
    } elsif ($SDK_NAME =~ m!Nokia_7710_SDK!) {
	$SDK_VARIANT = 'S90';
	$SDK_VERSION = $ENV{S90SDK} = '1.1';
    }
} elsif ($ENV{PATH} =~ m!\\S60\\devices\\(.+?)\\epoc32\\gcc\\bin!i) {
	$SDK_VARIANT = 'S60';
	$SDK_NAME = $1;
	$WIN = $ENV{WIN} = 'winscw';
	$SYMBIAN_VERSION = '9.4';
	$SDK_VERSION = $ENV{S60SDK} = '5.0';
	$SYMBIAN_ROOT = $ENV{EPOCROOT};
} elsif ($ENV{PATH} =~ m!\\Symbian\\UIQ_(\d)(\d)\\Epoc32\\gcc\\bin!i) {
    $SDK_NAME    = 'UIQ';
    $SDK_VARIANT = 'UIQ';
    $SDK_VERSION = $ENV{UIQSDK} = "$1.$2";
    if ($SDK_VERSION =~ /^2\./) {
	$SYMBIAN_VERSION = '7.0s';
    } else {
	die "$0: Unknown UIQ version '$SDK_VERSION'\n";
    }
    $WIN = 'winscw'; # This is CodeWarrior, how about Borland?
    $ENV{WIN} = $WIN;
}

if (open(GCC, "gcc -v 2>&1 |")) {
   while (<GCC>) {
     # print;
     if (/Reading specs from (.+?)\\Epoc32\\/i) {
       $SYMBIAN_ROOT = $1;
       # The S60SDK tells the Series 60 SDK version.
       if ($ENV{S60SDK}) {
	   if ($SYMBIAN_ROOT eq 'C:\Symbian\6.1\Shared') { # Visual C.
	       $SYMBIAN_ROOT = 'C:\Symbian\6.1\Series60';
	       $SDK_VERSION = $ENV{S60SDK} = '1.2';
	   } elsif ($SYMBIAN_ROOT eq 'C:\Symbian\Series60_1_2_CW') { # CodeWarrior.
	       $SDK_VERSION = $ENV{S60SDK} = '1.2';
	   }
       }
       last;
     }
   }
   close GCC;
} else {
  die "$0: failed to run gcc: $!\n";
}

die "$0: failed to locate the Symbian SDK\n" unless defined $SYMBIAN_ROOT;

my $UARM = $ENV{UARM} ? $ENV{UARM} : "urel";
my $UREL = "$SYMBIAN_ROOT\\epoc32\\release\\-ARM-\\$UARM";
if ($SYMBIAN_ROOT eq 'C:\Symbian\6.1\Series60' && $ENV{WIN} eq 'winscw') {
    $UREL = "C:\\Symbian\\Series60_1_2_CW\\epoc32\\release\\-ARM-\\urel";
}
$ENV{UREL} = $UREL;
$ENV{UARM} = $UARM;

[ $SYMBIAN_ROOT, $SYMBIAN_VERSION, $SDK_NAME, $SDK_VARIANT, $SDK_VERSION ];

# The following is a cheat sheet for the right S60/S80 SDK settings.
#
# symbiancommon.bat:
# set EPOC_BIN=%EPOCROOT%Epoc32\gcc\bin;%EPOCROOT%Epoc32\Tools
# set MWCW=C:\Program Files\Metrowerks\CodeWarrior for Symbian OEM v2.8
# set MSVC=C:\Program Files\Microsoft Visual Studio
# set MSVC_BIN=%MSVC%\VC98\Bin;%MSVC%\Aux\MSDev98\Bin
# set MSVC_INC=%MSVC%\VC98\atl\include;%MSVC%\VC98\include;%MSVC%\mfc\include;%MSVC%\include
# set MSVC_LIB=%MSVC%\mfc\lib;%MSVC%\lib
#
# Note that if you are using Microsoft Visual Studio 8
# (for example because you are using the Microsoft Visual C++
#  2005 Express Edition), the MSVC settings will be different:
# set MSVC=C:\Program Files\Microsoft Visual Studio 8\VC
# set MSVC_BIN=%MSVC%\bin
# set MSVC_INC=%MSVC%\include
# set MSVC_LIB=%MSVC%\lib
#
# s60-1.2-cw:
#
# set EPOCROOT=\Symbian\Series60_1_2_CW\
# symbiancommon
# set PATH=%EPOC_BIN%;%MSVC_BIN%;%MWCW%\Bin;%MWCW%\Symbian_Tools\Command_Line_Tools;%MSVC_BIN%;C:\perl\bin;C:\winnt\system32;%PATH%
# set USERDEFS=%USERDEFS% -D__SERIES60_12__ -D__SERIES60_MAJOR__=1 -D__SERIES60_MINOR__=2 -D__SERIES60_1X__
#
# s60-1.2-vc:
#
# set EPOCROOT=\Symbian\6.1\Series60\
# symbiancommon
# set PATH=\Symbian\6.1\Shared\Epoc32\gcc\bin;\Symbian\6.1\Shared\Epoc32\Tools;%MSVC_BIN%;C:\perl\bin;C:\winnt\system32;%PATH%
# set INCLUDE=%MSVC_INC%
# set LIB=%MSVC_LIB%
# set USERDEFS=%USERDEFS% -D__SERIES60_12__ -D__SERIES60_MAJOR__=1 -D__SERIES60_MINOR__=2 -D__SERIES60_1X__
#
# s60-2.0-cw:
#
# set EPOCROOT=\Symbian\7.0s\Series60_v20_CW\
# set EPOCDEVICE=Series60_2_0_CW:com.Nokia.Series60_2_0_CW
# symbiancommon
# set PATH=%EPOC_BIN%;%MWCW%\Bin;%MWCW%\Symbian_Tools\Command_Line_Tools;%MSVC_BIN%;C:\perl\bin;C:\winnt\system32;%PATH%
# set USERDEFS=%USERDEFS% -D__SERIES60_20__ -D__SERIES60_MAJOR__=2 -D__SERIES60_MINOR__=0 -D__SERIES60_2X__
#
# s60-2.0-vc:
#
# set EPOCROOT=\Symbian\7.0s\Series60_v20\
# set EPOCDEVICE=Series60_v20:com.nokia.series60
# symbiancommon
# set PATH=%EPOC_BIN%;%MSVC_BIN%;C:\perl\bin;C:\winnt\system32;%PATH%
# set INCLUDE=%MSVC_INC%
# set LIB=%MSVC_LIB%
# set USERDEFS=%USERDEFS% -D__SERIES60_20__ -D__SERIES60_MAJOR__=2 -D__SERIES60_MINOR__=0 -D__SERIES60_2X__
#
# s60-2.1-cw:
#
# set EPOCROOT=\Symbian\7.0s\Series60_v21_CW\
# set EPOCDEVICE=Series60_v21_CW:com.Nokia.series60
# symbiancommon
# set PATH=%EPOC_BIN%;%MWCW%\Bin;%MWCW%\Symbian_Tools\Command_Line_Tools;%MSVC_BIN%;C:\perl\bin;C:\winnt\system32;%PATH%
# set USERDEFS=%USERDEFS% -D__SERIES60_21__ -D__SERIES60_MAJOR__=2 -D__SERIES60_MINOR__=1 -D__SERIES60_2X__
#
# s60-2.6-cw:
#
# set EPOCROOT=\Symbian\8.0a\S60_2nd_FP2_CW\
# set EPOCDEVICE=S60_2nd_FP2_CW:com.nokia.series60
# symbiancommon
# set PATH=%EPOC_BIN%;%MWCW%\Bin;%MWCW%\Symbian_Tools\Command_Line_Tools;%MSVC_BIN%;C:\perl\bin;C:\winnt\system32;%PATH%
# set USERDEFS=%USERDEFS% -D__SERIES60_26__ -D__SERIES60_MAJOR__=2 -D__SERIES60_MINOR__=6 -D__SERIES60_2X__ -D__BLUETOOTH_API_V2__
#
# s60-2.6-vc:
#
# set EPOCROOT=\Symbian\8.0a\S60_2nd_FP2\
# set EPOCDEVICE=S60_2nd_FP2:com.nokia.Series60
# symbiancommon
# set PATH=%EPOC_BIN%;%MSVC_BIN%;C:\perl\bin;C:\winnt\system32;%PATH%
# set INCLUDE=%MSVC_INC%
# set LIB=%MSVC_LIB%
# set USERDEFS=%USERDEFS% -D__SERIES60_26__ -D__SERIES60_MAJOR__=2 -D__SERIES60_MINOR__=6 -D__SERIES60_2X__ -D__BLUETOOTH_API_V2__
#
# s60-2.8-cw:
#
# set EPOCROOT=\Symbian\8.1a\S60_2nd_FP3\
# set EPOCDEVICE=S60_2nd_FP3:com.nokia.series60
# symbiancommon
# set PATH=%EPOC_BIN%;%MWCW%\Bin;%MWCW%\Symbian_Tools\Command_Line_Tools;%MSVC_BIN%;C:\perl\bin;C:\winnt\system32;%PATH%
# set USERDEFS=%USERDEFS% -D__SERIES60_28__ -D__SERIES60_MAJOR__=2 -D__SERIES60_MINOR__=8 -D__SERIES60_2X__ -D__BLUETOOTH_API_V2__
#
# s60-2.8-vc:
#
# set EPOCROOT=\Symbian\8.1a\S60_2nd_FP3\
# set EPOCDEVICE=S60_2nd_FP3:com.nokia.series60
# symbiancommon
# set PATH=%EPOC_BIN%;%MSVC_BIN%;C:\perl\bin;C:\winnt\system32;%PATH%
# set USERDEFS=%USERDEFS% -D__SERIES60_28__ -D__SERIES60_MAJOR__=2 -D__SERIES60_MINOR__=8 -D__SERIES60_2X__ -D__BLUETOOTH_API_V2__
#
# s60-5.0  - S60 5th Edition SDK v1.0:
#
# set EPOCROOT=\S60\devices\S60_5th_Edition_SDK_v1.0\
# set PATH=%EPOCROOT%Epoc32\gcc\bin;%EPOCROOT%Epoc32\tools;%PATH%
#
# s80-2.0-cw:
#
# set EPOCROOT=\Symbian\7.0s\S80_DP2_0_SDK_CW\
# set EPOCDEVICE=Series80_DP2_0_SDK_CW:com.nokia.Series80
# symbiancommon
# set PATH=%EPOC_BIN%;%MWCW%\Bin;%MWCW%\Symbian_Tools\Command_Line_Tools;%MSVC_BIN%;C:\perl\bin;C:\winnt\system32;%PATH%
# set USERDEFS=%USERDEFS% -D__SERIES80_20__ -D__SERIES80_MAJOR__=2 -D__SERIES80_MINOR__=0 -D__SERIES80_2X__
#
# s80-2.0-vc:
#
# set EPOCROOT=\Symbian\7.0s\S80_DP2_0_SDK\
# set EPOCDEVICE=Series80_DP2_0_SDK:com.nokia.Series80
# symbiancommon
# set PATH=%EPOC_BIN%;%MWCW%\Bin;%MWCW%\Symbian_Tools\Command_Line_Tools;%MSVC_BIN%;C:\perl\bin;C:\winnt\system32;%PATH%
# set USERDEFS=%USERDEFS% -D__SERIES80_20__ -D__SERIES80_MAJOR__=2 -D__SERIES80_MINOR__=0 -D__SERIES80_2X__
#
# UIQ-2.1-vc:
# set EPOCROOT=\Symbian\UIQ_21\
# set EPOCDEVICE=
# set EPOC_BIN=%EPOCROOT%Epoc32\gcc\bin;%EPOCROOT%Epoc32\Tools
# set MWCW=C:\APPS\codewarrior_3.0
# set MSVC=C:\Program Files\Microsoft Visual Studio
# set MSVC_BIN=%MSVC%;%MSVC%\Common\MSDev98\Bin
# set MSVC_INC=%MSVC%\VC98\atl\include;%MSVC%\mfc\include;%MSVC%\include
# set MSVC_LIB=%MSVC%\mfc\lib;%MSVC%\lib
# set PATH=%EPOC_BIN%;%MWCW%\Bin;%MWCW%\Symbian_Tools\Command_Line_Tools;%MSVC_BIN%;C:\perl\bin;C:\winnt\system32;%PATH%
# set USERDEFS=%USERDEFS% -D__UIQ_21__ -D__UIQ_MAJOR__=2 -D__UIQ_MINOR__=1 -D__UIQ_2X__
#
# EOF
