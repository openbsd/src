#!/usr/bin/perl

use lib '..';
use Memoize;

my $n = 0;
$|=1;


if (-e '.fast') {
  print "1..0\n";
  exit 0;
}

print "1..12\n";
# (1)
++$n; print "ok $n\n";

my $READFILE_CALLS = 0;
my $FILE = './TESTFILE';

sub writefile {
  my $FILE = shift;
  open F, "> $FILE" or die "Couldn't write temporary file $FILE: $!";
  print F scalar(localtime), "\n";
  close F;
}

sub readfile {
  $READFILE_CALLS++;
  my $FILE = shift;
  open F, "< $FILE" or die "Couldn't write temporary file $FILE: $!";
  my $data = <F>;
  close F;
  $data;
}

require Memoize::ExpireFile;
# (2)
++$n; print "ok $n\n";

tie my %cache => 'Memoize::ExpireFile';
memoize 'readfile',
    SCALAR_CACHE => [HASH => \%cache],
    LIST_CACHE => 'FAULT'
    ;

# (3)
++$n; print "ok $n\n";

# (4)
writefile($FILE);
++$n; print "ok $n\n";
sleep 4;

# (5-6)
my $t1 = readfile($FILE);
++$n; print "ok $n\n";
++$n; print ((($READFILE_CALLS == 1) ? '' : 'not '), "ok $n\n");

# (7-9)
my $t2 = readfile($FILE);
++$n; print "ok $n\n";  
++$n; print ((($READFILE_CALLS == 1) ? '' : 'not '), "ok $n\n");
++$n; print ((($t1 eq $t2) ? '' : 'not '), "ok $n\n");

# (10-12)
sleep 4;
writefile($FILE);
my $t3 = readfile($FILE);
++$n; print "ok $n\n";
++$n; print ((($READFILE_CALLS == 2) ? '' : 'not '), "ok $n\n");
++$n; print ((($t1 ne $t3) ? '' : 'not '), "ok $n\n");

END { 1 while unlink $FILE }
