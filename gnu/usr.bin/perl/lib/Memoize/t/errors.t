#!/usr/bin/perl

use lib '..';
use Memoize;
use Config;

$|=1;
print "1..11\n";

eval { memoize({}) };
print $@ ? "ok 1\n" : "not ok 1 # $@\n";

eval { memoize([]) };
print $@ ? "ok 2\n" : "not ok 2 # $@\n";

eval { my $x; memoize(\$x) };
print $@ ? "ok 3\n" : "not ok 3 # $@\n";

# 4--8
$n = 4;
my $dummyfile = './dummydb';
use Fcntl;
my %args = ( DB_File => [],
             GDBM_File => [$dummyfile, 2, 0666],
             ODBM_File => [$dummyfile, O_RDWR|O_CREAT, 0666],
             NDBM_File => [$dummyfile, O_RDWR|O_CREAT, 0666],
             SDBM_File => [$dummyfile, O_RDWR|O_CREAT, 0666],
           );
for $mod (qw(DB_File GDBM_File SDBM_File ODBM_File NDBM_File)) {
  eval {
    require "$mod.pm";
    tie my %cache => $mod, @{$args{$mod}};
    memoize(sub {}, LIST_CACHE => [HASH => \%cache ]);
  };
  print $@ =~ /can only store scalars/
     || $@ =~ /Can't locate.*in \@INC/
     || $@ =~ /Can't load '.*?' for module/ ? "ok $n\n" : "not ok $n # $@\n";
  1 while unlink $dummyfile, "$dummyfile.dir", "$dummyfile.pag", "$dummyfile.db";
  $n++;
}

# 9
eval { local $^W = 0;
       memoize(sub {}, LIST_CACHE => ['TIE', 'WuggaWugga']) 
     };
print $@ ? "ok 9\n" : "not ok 9 # $@\n";

# 10
eval { memoize(sub {}, LIST_CACHE => 'YOB GORGLE') };
print $@ ? "ok 10\n" : "not ok 10 # $@\n";

# 11
eval { memoize(sub {}, SCALAR_CACHE => ['YOB GORGLE']) };
print $@ ? "ok 11\n" : "not ok 11 # $@\n";

