#!/usr/bin/perl -w

$epoc="/usr/local/epoc";
@objs=@ARGV;
$basname=$objs[0];
$basname =~ s/.o//;
$baspe = $basname . "pe";


system("arm-pe-ld -s -e _E32Startup --base-file $basname.bas " .
       "-o $baspe.exe $epoc/lib/eexe.o @objs " .
       "$epoc/lib/ecrt0.o $epoc/lib/estlib.lib $epoc/lib/euser.lib");

system("arm-pe-dlltool --as=arm-pe-as --output-exp $basname.exp " .
       "--base-file $basname.bas $epoc/lib/eexe.o @objs " .
       "$epoc/lib/ecrt0.o $epoc/lib/estlib.lib  $epoc/lib/euser.lib");

system("arm-pe-ld -s -e _E32Startup $basname.exp " .
       "-o $baspe.exe $epoc/lib/eexe.o @objs " .
       "$epoc/lib/ecrt0.o $epoc/lib/estlib.lib $epoc/lib/euser.lib");

system( "wine $epoc/bin/petran.exe \"$baspe.exe $basname.exe " .
        "-nocall -heap 0x00000400 0x00400000 -stack 0x0000c000 " .
        "-uid1 0x1000007a -uid2 0x100051d8 -uid3 0x00000000\" ");

