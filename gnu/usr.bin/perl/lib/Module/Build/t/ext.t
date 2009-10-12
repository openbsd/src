#!/usr/bin/perl -w

use strict;
use lib $ENV{PERL_CORE} ? '../lib/Module/Build/t/lib' : 't/lib';
use MBTest;

use Module::Build;

my @unix_splits = 
  (
   { q{one t'wo th'ree f"o\"ur " "five" } => [ 'one', 'two three', 'fo"ur ', 'five' ] },
   { q{ foo bar }                         => [ 'foo', 'bar'                         ] },
   { q{ D\'oh f\{g\'h\"i\]\* }            => [ "D'oh", "f{g'h\"i]*"                 ] },
   { q{ D\$foo }                          => [ 'D$foo'                              ] },
   { qq{one\\\ntwo}                       => [ "one\ntwo"                           ] },  # TODO
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

plan tests => 10 + 4*@unix_splits + 4*@win_splits;

ensure_blib('Module::Build');

#########################

# Should always return an array unscathed
foreach my $platform ('', '::Platform::Unix', '::Platform::Windows') {
  my $pkg = "Module::Build$platform";
  my @result = $pkg->split_like_shell(['foo', 'bar', 'baz']);
  is @result, 3, "Split using $pkg";
  is "@result", "foo bar baz", "Split using $pkg";
}

# I think 3.24 isn't actually the majik version, my 3.23 seems to pass...
my $low_TPW_version = Text::ParseWords->VERSION < 3.24;
use Module::Build::Platform::Unix;
foreach my $test (@unix_splits) {
  # Text::ParseWords bug:
  local $TODO = $low_TPW_version && ((keys %$test)[0] =~ m{\\\n});

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
  # Make sure data can make a round-trip through an external perl
  # process, which can involve the shell command line

  # silence the printing for easier matching
  local *Module::Build::log_info = sub {};

  my @data = map values(%$_), @unix_splits, @win_splits;
  for my $d (@data) {
    my $out = stdout_of
      ( sub {
	  Module::Build->run_perl_script('-le', [], ['print join " ", map "{$_}", @ARGV', @$d]);
	} );
    chomp $out;
    is($out, join(' ', map "{$_}", @$d), "perl round trip for ".join('',map "{$_}", @$d));
  }
}

{
  # Make sure data can make a round-trip through an external backtick
  # process, which can involve the shell command line

  # silence the printing for easier matching
  local *Module::Build::log_info = sub {};

  my @data = map values(%$_), @unix_splits, @win_splits;
  for my $d (@data) {
    chomp(my $out = Module::Build->_backticks($^X, '-le', 'print join " ", map "{$_}", @ARGV', @$d));
    is($out, join(' ', map "{$_}", @$d), "backticks round trip for ".join('',map "{$_}", @$d));
  }
}

{
  # Make sure run_perl_script() propagates @INC
  my $dir = MBTest->tmpdir;
  if ($^O eq 'VMS') {
      # VMS can store INC paths in Unix format with out the trailing
      # directory delimiter.
      $dir = VMS::Filespec::unixify($dir);
      $dir =~ s#/$##;
  }
  local @INC = ($dir, @INC);
  my $output = stdout_of( sub { Module::Build->run_perl_script('-le', [], ['print for @INC']) } );
  like $output, qr{^\Q$dir\E}m;
}

##################################################################
sub do_split_tests {
  my ($package, $test) = @_;

  my ($string, $expected) = %$test;
  my @result = $package->split_like_shell($string);
  is( 0 + grep( !defined(), @result ), # all defined
      0,
      "'$string' result all defined" );
  is_deeply(\@result, $expected) or
    diag("$package split_like_shell error \n" .
      ">$string< is not splitting as >" . join("|", @$expected) . '<');
}
