#!/usr/bin/perl

use File::Find;
use Cwd;

$VERSION="5.8.0";
$EPOC_VERSION=1;


sub filefound {

  my $f = $File::Find::name;
    
  return if ( $f =~ /CVS|Unicode|unicore|CPAN|ExtUtils|IPC|User|DB.pm|\.a$|\.ld$|\.exists$|\.pod$|\.t$/i);
  my $back = $f;

  my $psiback = $back;

  $psiback =~ s|.*/lib/|\\emx\\lib\\perl\\$VERSION\\|;
  $psiback =~ s|/|\\|g;
  print OUT "\"$back\"-\"!:$psiback\"\n"  if ( -f $f );
}

open OUT,">perl.pkg";

print OUT "#{\"perl$VERSION\"},(0x100051d8),0,$EPOC_VERSION,0\n";
print OUT "\"" . cwd . "/Artistic.txt\"-\"\",FT,TC\n";
print OUT "\"" . cwd . "/perl\"-\"!:\\emx\\bin\\perl.exe\"\n";

find(\&filefound, cwd.'/lib');

open IN,  "<Artistic";
open OUT, ">Artistic.txt";
while (my $line = <IN>) {
  chomp $line;
  print OUT "$line\r\n";
}

close IN;
close OUT;

