#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
}

plan tests => 12;

$x='banana';
$x=~/.a/g;
is(pos($x), 2, "matching, pos() leaves off at offset 2");

$x=~/.z/gc;
is(pos($x), 2, "not matching, pos() remains at offset 2");

sub f { my $p=$_[0]; return $p }

$x=~/.a/g;
is(f(pos($x)), 4, "matching again, pos() next leaves off at offset 4");

# Is pos() set inside //g? (bug id 19990615.008)
$x = "test string?"; $x =~ s/\w/pos($x)/eg;
is($x, "0123 5678910?", "pos() set inside //g");

$x = "123 56"; $x =~ / /g;
is(pos($x), 4, "matching, pos() leaves off at offset 4");
{ local $x }
is(pos($x), 4, "value of pos() unaffected by intermediate localization");

# Explicit test that triggers the utf8_mg_len_cache_update() code path in
# Perl_sv_pos_b2u().

$x = "\x{100}BC";
$x =~ m/.*/g;
is(pos $x, 3, "utf8_mg_len_cache_update() test");


my $destroyed;
{ package Class; DESTROY { ++$destroyed; } }

$destroyed = 0;
{
    my $x = '';
    pos($x) = 0;
    $x = bless({}, 'Class');
}
is($destroyed, 1, 'Timely scalar destruction with lvalue pos');

eval 'pos @a = 1';
like $@, qr/^Can't modify array dereference in match position at /,
  'pos refuses @arrays';
eval 'pos %a = 1';
like $@, qr/^Can't modify hash dereference in match position at /,
  'pos refuses %hashes';
eval 'pos *a = 1';
is eval 'pos *a', 1, 'pos *glob works';

# Test that UTF8-ness of $1 changing does not confuse pos
"f" =~ /(f)/; "$1";	# first make sure UTF8-ness is off
"\x{100}a" =~ /(..)/;	# give PL_curpm a UTF8 string; $1 does not know yet
pos($1) = 2;		# set pos; was ignoring UTF8-ness
"$1";			# turn on UTF8 flag
is pos($1), 2, 'pos is not confused about changing UTF8-ness';
