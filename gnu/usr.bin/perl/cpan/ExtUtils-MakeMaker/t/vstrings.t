#!/usr/bin/perl -w

# test support for various forms of vstring versions in PREREQ_PM

# Magic for core
BEGIN {
    # Always run in t to unify behaviour with core
    chdir 't' if -d 't';
}

# Use things from t/lib/
use lib './lib';
use strict;
use warnings;
use TieOut;
use MakeMaker::Test::Utils qw(makefile_name);
use File::Temp qw[tempdir];

use ExtUtils::MakeMaker;
use Test::More;

my $tmpdir = tempdir( DIR => '.', CLEANUP => 1 );
chdir $tmpdir;

sub capture_make {
    my ($package, $version) = @_ ;

    my $warnings = '';
    local $SIG{__WARN__} = sub {
        $warnings .= join '', @_;
    };

    local $ExtUtils::MakeMaker::UNDER_CORE = 0;

    WriteMakefile(
        NAME      => 'VString::Test',
        PREREQ_PM => { $package , $version }
    );

    return $warnings;
}

sub makefile_content {
    open my $fh, '<', makefile_name or die;
    return <$fh>;
}

# [ pkg, version, pattern, descrip, invertre ]
my @DATA = (
  [ DecimalString => '1.2.3', qr/isn't\s+numeric/, '3-part Decimal String' ],
  [ VDecimalString => 'v1.2.3', qr/Unparsable\s+version/, '3-part V-Decimal String' ],
  [ BareVString => v1.2.3, qr/Unparsable\s+version/, '3-part bare V-string' ],
  [ VDecimalString => 'v1.2', qr/Unparsable\s+version/, '2-part v-decimal string' ],
  [ BareVString => v1.2, qr/Unparsable\s+version/, '2-part bare v-string' ],
  [ BrokenString => 'nan', qr/Unparsable\s+version/, 'random string', 1 ],
);

ok(my $stdout = tie *STDOUT, 'TieOut');
for my $tuple (@DATA) {
  my ($pkg, $version, $pattern, $descrip, $invertre) = @$tuple;
  next if $] < 5.008 && $pkg eq 'BareVString' && $descrip =~ m!^2-part!;
  my $out;
  eval { $out = capture_make("Fake::$pkg" => $version); };
  is($@, '', "$descrip not fatal");
  if ($invertre) {
    like ( $out , qr/$pattern/i, "$descrip parses");
  } else {
    unlike ( $out , qr/$pattern/i , "$descrip parses");
  }
#  note(join q{}, grep { $_ =~ /Fake/i } makefile_content);
}

done_testing();
