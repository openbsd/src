#!/usr/bin/perl -w

use strict;
use lib $ENV{PERL_CORE} ? '../lib/Module/Build/t/lib' : 't/lib';
use MBTest;

my @unix_splits = 
  (
   { q{one t'wo th'ree f"o\"ur " "five" } => [ 'one', 'two three', 'fo"ur ', 'five' ] },
   { q{ foo bar }                         => [ 'foo', 'bar'                         ] },
  );

my @win_splits = 
  (
   { 'a" "b\\c" "d'         => [ 'a b\c d'       ] },
   { '"a b\\c d"'           => [ 'a b\c d'       ] },
   { '"a b"\\"c d"'         => [ 'a b"c', 'd'    ] },
   { '"a b"\\\\"c d"'       => [ 'a b\c d'       ] },
   { '"a"\\"b" "a\\"b"'     => [ 'a"b a"b'       ] },
   { '"a"\\\\"b" "a\\\\"b"' => [ 'a\b', 'a\b'    ] },
   { '"a"\\"b a\\"b"'       => [ 'a"b', 'a"b'    ] },
   { 'a"\\"b" "a\\"b'       => [ 'a"b', 'a"b'    ] },
   { 'a"\\"b"  "a\\"b'      => [ 'a"b', 'a"b'    ] },
   { 'a           b'        => [ 'a', 'b'        ] },
   { 'a"\\"b a\\"b'         => [ 'a"b a"b'       ] },
   { '"a""b" "a"b"'         => [ 'a"b ab'        ] },
   { '\\"a\\"'              => [ '"a"'           ] },
   { '"a"" "b"'             => [ 'a"', 'b'       ] },
   { 'a"b'                  => [ 'ab'            ] },
   { 'a""b'                 => [ 'ab'            ] },
   { 'a"""b'                => [ 'a"b'           ] },
   { 'a""""b'               => [ 'a"b'           ] },
   { 'a"""""b'              => [ 'a"b'           ] },
   { 'a""""""b'             => [ 'a""b'          ] },
   { '"a"b"'                => [ 'ab'            ] },
   { '"a""b"'               => [ 'a"b'           ] },
   { '"a"""b"'              => [ 'a"b'           ] },
   { '"a""""b"'             => [ 'a"b'           ] },
   { '"a"""""b"'            => [ 'a""b'          ] },
   { '"a""""""b"'           => [ 'a""b'          ] },
   { ''                     => [                 ] },
   { ' '                    => [                 ] },
   { '""'                   => [ ''              ] },
   { '" "'                  => [ ' '             ] },
   { '""a'                  => [ 'a'             ] },
   { '""a b'                => [ 'a', 'b'        ] },
   { 'a""'                  => [ 'a'             ] },
   { 'a"" b'                => [ 'a', 'b'        ] },
   { '"" a'                 => [ '', 'a'         ] },
   { 'a ""'                 => [ 'a', ''         ] },
   { 'a "" b'               => [ 'a', '', 'b'    ] },
   { 'a " " b'              => [ 'a', ' ', 'b'   ] },
   { 'a " b " c'            => [ 'a', ' b ', 'c' ] },
);

plan tests => 10 + 2*@unix_splits + 2*@win_splits;

#########################

use Module::Build;
ok(1);

# Should always return an array unscathed
foreach my $platform ('', '::Platform::Unix', '::Platform::Windows') {
  my $pkg = "Module::Build$platform";
  my @result = $pkg->split_like_shell(['foo', 'bar', 'baz']);
  is @result, 3, "Split using $pkg";
  is "@result", "foo bar baz", "Split using $pkg";
}

use Module::Build::Platform::Unix;
foreach my $test (@unix_splits) {
  do_split_tests('Module::Build::Platform::Unix', $test);
}

use Module::Build::Platform::Windows;
foreach my $test (@win_splits) {
  do_split_tests('Module::Build::Platform::Windows', $test);
}


{
  # Make sure read_args() functions properly as a class method
  my @args = qw(foo=bar --food bard --foods=bards);
  my ($args) = Module::Build->read_args(@args);
  is_deeply($args, {foo => 'bar', food => 'bard', foods => 'bards', ARGV => []});
}

{
  # Make sure data can make a round-trip through unparse_args() and read_args()
  my %args = (foo => 'bar', food => 'bard', config => {a => 1, b => 2}, ARGV => []);
  my ($args) = Module::Build->read_args( Module::Build->unparse_args(\%args) );
  is_deeply($args, \%args);
}

{
  # Make sure run_perl_script() propagates @INC
  my $dir = 'whosiewhatzit';
  mkdir $dir, 0777;
  local @INC = ($dir, @INC);
  my $output = stdout_of( sub { Module::Build->run_perl_script('', ['-le', 'print for @INC']) } );
  like $output, qr{^$dir}m;
  rmdir $dir;
}

##################################################################
sub do_split_tests {
  my ($package, $test) = @_;

  my ($string, $expected) = %$test;
  my @result = $package->split_like_shell($string);
  is( 0 + grep( !defined(), @result ), # all defined
      0,
      "'$string' result all defined" );
  is_deeply(\@result, $expected);
}
