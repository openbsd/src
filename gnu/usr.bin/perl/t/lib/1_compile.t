#!./perl

# Modules should have their own tests.  For historical reasons, some
# do not.  This does basic compile tests on modules that have no tests
# of their own.

BEGIN {
    chdir 't';
    @INC = '../lib';
}

use strict;
use warnings;
use File::Spec::Functions;

# Okay, this is the list.

my @Core_Modules = grep /\S/, <DATA>;
chomp @Core_Modules;

if (eval { require Socket }) {
  push @Core_Modules, qw(Net::Domain);
  # Two Net:: modules need the Convert::EBCDIC if in EBDCIC.
  if (ord("A") != 193 || eval { require Convert::EBCDIC }) {
      push @Core_Modules, qw(Net::Cmd Net::POP3);
  }
}

@Core_Modules = sort @Core_Modules;

print "1..".(1+@Core_Modules)."\n";

my $message
  = "ok 1 - All modules should have tests # TODO Make Schwern Poorer\n";
if (@Core_Modules) {
  print "not $message";
} else {
  print $message;
}

my $test_num = 2;

foreach my $module (@Core_Modules) {
    my $todo = '';
    $todo = "# TODO $module needs porting on $^O" if $module eq 'ByteLoader' && $^O eq 'VMS';
    print "# $module compile failed\nnot " unless compile_module($module);
    print "ok $test_num $todo\n";
    $test_num++;
}

# We do this as a separate process else we'll blow the hell
# out of our namespace.
sub compile_module {
    my ($module) = $_[0];

    my $compmod = catfile(curdir(), 'lib', 'compmod.pl');
    my $lib     = '-I' . catdir(updir(), 'lib');

    my $out = scalar `$^X $lib $compmod $module`;
    print "# $out";
    return $out =~ /^ok/;
}

# These modules have no tests of their own.
# Keep up to date with
# http://www.pobox.com/~schwern/cgi-bin/perl-qa-wiki.cgi?UntestedModules
# and vice-versa.  The list should only shrink.
__DATA__
B::C
B::CC
B::Stackobj
ByteLoader
CPAN::FirstTime
DynaLoader
Pod::Plainer
