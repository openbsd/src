#!/usr/bin/perl

use File::Find;
use Cwd;

$VERSION="5.6";
$PATCH="1";
$EPOC_VERSION=26;
$CROSSCOMPILEPATH=cwd;
$CROSSREPLACEPATH="H:\\perl";


sub filefound {
    my $f = $File::Find::name;
    
    return if ( $f =~ /CVS|unicode|CPAN|ExtUtils|IPC|User|DB.pm|\.a$|\.ld$|\.exists$|\.pod$/i);
    my $back = $f;

    $back =~ s|$CROSSCOMPILEPATH||;

    $back =~ s|/|\\|g;

    my $psiback = $back;

    $psiback =~ s/\\lib\\/\\perl\\lib\\$VERSION.$PATCH\\/i;

    print OUT "\"$CROSSREPLACEPATH$back\"-\"!:$psiback\"\n"  if ( -f $f );
;
}

open OUT,">perl.pkg";

print OUT "#{\"perl$VERSION\"},(0x100051d8),$PATCH,$EPOC_VERSION,0\n";
print OUT "\"$CROSSREPLACEPATH\\Artistic\"-\"\",FT,TA\n";
print OUT "\"$CROSSREPLACEPATH\\perlmain.exe\"-\"!:\\system\\programs\\perl.exe\"\n";

find(\&filefound, cwd.'/lib');
print OUT "@\"G:\\lib\\stdlib.sis\",(0x0100002c3)\n"


