#!./perl
#
#  Copyright (c) 1995-2000, Raphael Manfredi
#  
#  You may redistribute only under the same terms as Perl 5, as specified
#  in the README file that comes with the distribution.
#
# Original Author: Ulrich Pfeifer
# (C) Copyright 1997, Universitat Dortmund, all rights reserved.
#

sub BEGIN {
    if ($ENV{PERL_CORE}){
	chdir('t') if -d 't';
	@INC = ('.', '../lib');
    } else {
	unshift @INC, 't';
    }
    require Config; import Config;
    if ($ENV{PERL_CORE} and $Config{'extensions'} !~ /\bStorable\b/) {
        print "1..0 # Skip: Storable was not built\n";
        exit 0;
    }
}

use Storable qw(store retrieve);

# problems with 5.00404 when in an BEGIN block, so this is defined here
if (!eval { require File::Spec; 1 } || $File::Spec::VERSION < 0.8) {
    print "1..0 # Skip: File::Spec 0.8 needed\n";
    exit 0;
    # Mention $File::Spec::VERSION again, as 5.00503's harness seems to have
    # warnings on.
    exit $File::Spec::VERSION;
}

print "1..8\n";

my $test = 1;
*GLOB = *GLOB; # peacify -w
my $bad = ['foo', \*GLOB,  'bar'];
my $result;

eval {$result = store ($bad , 'store')};
print ((!defined $result)?"ok $test\n":"not ok $test\n"); $test++;
print (($@ ne '')?"ok $test\n":"not ok $test\n"); $test++;

$Storable::forgive_me=1;

my $devnull = File::Spec->devnull;

open(SAVEERR, ">&STDERR");
open(STDERR, ">$devnull") or 
  ( print SAVEERR "Unable to redirect STDERR: $!\n" and exit(1) );

eval {$result = store ($bad , 'store')};

open(STDERR, ">&SAVEERR");

print ((defined $result)?"ok $test\n":"not ok $test\n"); $test++;
print (($@ eq '')?"ok $test\n":"not ok $test\n"); $test++;

my $ret = retrieve('store');
print ((defined $ret)?"ok $test\n":"not ok $test\n"); $test++;
print (($ret->[0] eq 'foo')?"ok $test\n":"not ok $test\n"); $test++;
print (($ret->[2] eq 'bar')?"ok $test\n":"not ok $test\n"); $test++;
print ((ref $ret->[1] eq 'SCALAR')?"ok $test\n":"not ok $test\n"); $test++;


END { 1 while unlink 'store' }
