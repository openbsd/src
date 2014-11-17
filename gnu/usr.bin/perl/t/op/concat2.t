#!./perl

# This file is for concatenation tests that require test.pl.
#
# concat.t cannot use test.pl as it needs to avoid using concatenation in
# its ok() function.

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
}

plan 3;

SKIP: {
skip_if_miniperl("no dynamic loading on miniperl, no Encode", 1);
fresh_perl_is <<'end', "ok\n", {},
    no warnings 'deprecated';
    use encoding 'utf8';
    map { "a" . $a } ((1)x5000);
    print "ok\n";
end
 "concat does not lose its stack pointer after utf8 upgrade [perl #78674]";
}

# This test is in the file because overload.pm uses concatenation.
{ package o; use overload '""' => sub { $_[0][0] } }
$x = bless[chr 256],o::;
"$x";
$x->[0] = "\xff";
$x.= chr 257;
$x.= chr 257;
is $x, "\xff\x{101}\x{101}", '.= is not confused by changing utf8ness';

# Ops should not share the same TARG between recursion levels.  This may
# affect other ops, too, but concat seems more susceptible to this than
# others, since it can call itself recursively.  (Where else would I put
# this test, anyway?)
fresh_perl_is <<'end', "tmp\ntmp\n", {},
 sub canonpath {
     my ($path) = @_;
     my $node = '';
     $path =~ s|/\z||;
     return "$node$path";
 }
 
 {
  package Path::Class::Dir;
  use overload q[""] => sub { ::canonpath("tmp") };
 }
 
 print canonpath("tmp"), "\n";
 print canonpath(bless {},"Path::Class::Dir"), "\n";
end
 "recursive concat does not share TARGs";
