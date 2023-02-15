#!/usr/bin/perl

use lib qw(. ..);
use Memoize 0.45 qw(memoize unmemoize);
use Fcntl;

sub i {
  $_[0];
}

sub c119 { 119 }
sub c7 { 7 }
sub c43 { 43 }
sub c23 { 23 }
sub c5 { 5 }

sub n {
  $_[0]+1;
}

eval {require GDBM_File};
if ($@) {
  print "1..0\n";
  exit 0;
}

print "1..4\n";

$file = "md$$";
1 while unlink $file, "$file.dir", "$file.pag";
tryout('GDBM_File', $file, 1);  # Test 1..4
1 while unlink $file, "$file.dir", "$file.pag";

sub tryout {
  require GDBM_File;
  my ($tiepack, $file, $testno) = @_;

  tie my %cache => $tiepack, $file, &GDBM_File::GDBM_NEWDB, 0666
    or die $!;

  memoize 'c5', 
  SCALAR_CACHE => [HASH => \%cache],
  LIST_CACHE => 'FAULT'
    ;

  my $t1 = c5();	
  my $t2 = c5();	
  print (($t1 == 5) ? "ok $testno\n" : "not ok $testno\n");
  $testno++;
  print (($t2 == 5) ? "ok $testno\n" : "not ok $testno\n");
  unmemoize 'c5';
  
  # Now something tricky---we'll memoize c23 with the wrong table that
  # has the 5 already cached.
  memoize 'c23', 
  SCALAR_CACHE => [HASH => \%cache],
  LIST_CACHE => 'FAULT'
    ;
  
  my $t3 = c23();
  my $t4 = c23();
  $testno++;
  print (($t3 == 5) ? "ok $testno\n" : "not ok $testno\n");
  $testno++;
  print (($t4 == 5) ? "ok $testno\n" : "not ok $testno\n");
  unmemoize 'c23';
}

