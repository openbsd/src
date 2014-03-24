#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
}

use strict;	# Amazed that this hackery can be made strict ...

# read in a file
sub cat {
    my $file = shift;
    local $/;
    open my $fh, $file or die "can't open '$file': $!";
    my $data = <$fh>;
    close $fh;
    $data;
}

#-- testing numeric fields in all variants (WL)

sub swrite {
    my $format = shift;
    local $^A = ""; # don't litter, use a local bin
    formline( $format, @_ );
    return $^A;
}

my @NumTests = (
    # [ format, value1, expected1, value2, expected2, .... ]
    [ '@###',           0,   '   0',         1, '   1',     9999.6, '####',
		9999.4999,   '9999',    -999.6, '####',     1e+100, '####' ],

    [ '@0##',           0,   '0000',         1, '0001',     9999.6, '####',
		-999.4999,   '-999',    -999.6, '####',     1e+100, '####' ],

    [ '^###',           0,   '   0',     undef, '    ' ],

    [ '^0##',           0,   '0000',     undef, '    ' ],

    [ '@###.',          0,  '   0.',         1, '   1.',    9999.6, '#####',
                9999.4999,  '9999.',    -999.6, '#####' ],

    [ '@##.##',         0, '  0.00',         1, '  1.00',  999.996, '######',
                999.99499, '999.99',      -100, '######' ],

    [ '@0#.##',         0, '000.00',         1, '001.00',       10, '010.00',
                  -0.0001, qr/^[\-0]00\.00$/ ],

);


my $num_tests = 0;
for my $tref ( @NumTests ){
    $num_tests += (@$tref - 1)/2;
}
#---------------------------------------------------------

# number of tests in section 1
my $bas_tests = 21;

# number of tests in section 3
my $bug_tests = 8 + 3 * 3 * 5 * 2 * 3 + 2 + 66 + 4 + 2 + 3 + 96 + 11;

# number of tests in section 4
my $hmb_tests = 35;

my $tests = $bas_tests + $num_tests + $bug_tests + $hmb_tests;

plan $tests;

############
## Section 1
############

use vars qw($fox $multiline $foo $good);

format OUT =
the quick brown @<<
$fox
jumped
@*
$multiline
^<<<<<<<<<
$foo
^<<<<<<<<<
$foo
^<<<<<<...
$foo
now @<<the@>>>> for all@|||||men to come @<<<<
{
    'i' . 's', "time\n", $good, 'to'
}
.

open(OUT, '>Op_write.tmp') || die "Can't create Op_write.tmp";
END { unlink_all 'Op_write.tmp' }

$fox = 'foxiness';
$good = 'good';
$multiline = "forescore\nand\nseven years\n";
$foo = 'when in the course of human events it becomes necessary';
write(OUT);
close OUT or die "Could not close: $!";

my $right =
"the quick brown fox
jumped
forescore
and
seven years
when in
the course
of huma...
now is the time for all good men to come to\n";

is cat('Op_write.tmp'), $right and unlink_all 'Op_write.tmp';

$fox = 'wolfishness';
my $fox = 'foxiness';		# Test a lexical variable.

format OUT2 =
the quick brown @<<
$fox
jumped
@*
$multiline
^<<<<<<<<< ~~
$foo
now @<<the@>>>> for all@|||||men to come @<<<<
'i' . 's', "time\n", $good, 'to'
.

open OUT2, '>Op_write.tmp' or die "Can't create Op_write.tmp";

$good = 'good';
$multiline = "forescore\nand\nseven years\n";
$foo = 'when in the course of human events it becomes necessary';
write(OUT2);
close OUT2 or die "Could not close: $!";

$right =
"the quick brown fox
jumped
forescore
and
seven years
when in
the course
of human
events it
becomes
necessary
now is the time for all good men to come to\n";

is cat('Op_write.tmp'), $right and unlink_all 'Op_write.tmp';

eval <<'EOFORMAT';
format OUT2 =
the brown quick @<<
$fox
jumped
@*
$multiline
and
^<<<<<<<<< ~~
$foo
now @<<the@>>>> for all@|||||men to come @<<<<
'i' . 's', "time\n", $good, 'to'
.
EOFORMAT

open(OUT2, '>Op_write.tmp') || die "Can't create Op_write.tmp";

$fox = 'foxiness';
$good = 'good';
$multiline = "forescore\nand\nseven years\n";
$foo = 'when in the course of human events it becomes necessary';
write(OUT2);
close OUT2 or die "Could not close: $!";

$right =
"the brown quick fox
jumped
forescore
and
seven years
and
when in
the course
of human
events it
becomes
necessary
now is the time for all good men to come to\n";

is cat('Op_write.tmp'), $right and unlink_all 'Op_write.tmp';

# formline tests

$right = <<EOT;
@ a
@> ab
@>> abc
@>>>  abc
@>>>>   abc
@>>>>>    abc
@>>>>>>     abc
@>>>>>>>      abc
@>>>>>>>>       abc
@>>>>>>>>>        abc
@>>>>>>>>>>         abc
EOT

my $was1 = my $was2 = '';
use vars '$format2';
for (0..10) {           
  # lexical picture
  $^A = '';
  my $format1 = '@' . '>' x $_;
  formline $format1, 'abc';
  $was1 .= "$format1 $^A\n";
  # global
  $^A = '';
  local $format2 = '@' . '>' x $_;
  formline $format2, 'abc';
  $was2 .= "$format2 $^A\n";
}
is $was1, $right;
is $was2, $right;

$^A = '';

# more test

format OUT3 =
^<<<<<<...
$foo
.

open(OUT3, '>Op_write.tmp') || die "Can't create Op_write.tmp";

$foo = 'fit          ';
write(OUT3);
close OUT3 or die "Could not close: $!";

$right =
"fit\n";

is cat('Op_write.tmp'), $right and unlink_all 'Op_write.tmp';


# test lexicals and globals
{
    my $test = curr_test();
    my $this = "ok";
    our $that = $test;
    format LEX =
@<<@|
$this,$that
.
    open(LEX, ">&STDOUT") or die;
    write LEX;
    $that = ++$test;
    write LEX;
    close LEX or die "Could not close: $!";
    curr_test($test + 1);
}
# LEX_INTERPNORMAL test
my %e = ( a => 1 );
format OUT4 =
@<<<<<<
"$e{a}"
.
open   OUT4, ">Op_write.tmp" or die "Can't create Op_write.tmp";
write (OUT4);
close  OUT4 or die "Could not close: $!";
is cat('Op_write.tmp'), "1\n" and unlink_all "Op_write.tmp";

# More LEX_INTERPNORMAL
format OUT4a=
@<<<<<<<<<<<<<<<
"${; use
     strict; \'Nasdaq dropping like flies'}"
.
open   OUT4a, ">Op_write.tmp" or die "Can't create Op_write.tmp";
write (OUT4a);
close  OUT4a or die "Could not close: $!";
is cat('Op_write.tmp'), "Nasdaq dropping\n", 'skipspace inside "${...}"'
    and unlink_all "Op_write.tmp";

eval <<'EOFORMAT';
format OUT10 =
@####.## @0###.##
$test1, $test1
.
EOFORMAT

open(OUT10, '>Op_write.tmp') || die "Can't create Op_write.tmp";

use vars '$test1';
$test1 = 12.95;
write(OUT10);
close OUT10 or die "Could not close: $!";

$right = "   12.95 00012.95\n";
is cat('Op_write.tmp'), $right and unlink_all 'Op_write.tmp';

eval <<'EOFORMAT';
format OUT11 =
@0###.## 
$test1
@ 0#
$test1
@0 # 
$test1
.
EOFORMAT

open(OUT11, '>Op_write.tmp') || die "Can't create Op_write.tmp";

$test1 = 12.95;
write(OUT11);
close OUT11 or die "Could not close: $!";

$right = 
"00012.95
1 0#
10 #\n";
is cat('Op_write.tmp'), $right and unlink_all 'Op_write.tmp';

{
    my $test = curr_test();
    my $el;
    format OUT12 =
ok ^<<<<<<<<<<<<<<~~ # sv_chop() naze
$el
.
    my %hash = ($test => 3);
    open(OUT12, '>Op_write.tmp') || die "Can't create Op_write.tmp";

    for $el (keys %hash) {
	write(OUT12);
    }
    close OUT12 or die "Could not close: $!";
    print cat('Op_write.tmp');
    curr_test($test + 1);
}

{
    my $test = curr_test();
    # Bug report and testcase by Alexey Tourbin
    use Tie::Scalar;
    my $v;
    tie $v, 'Tie::StdScalar';
    $v = $test;
    format OUT13 =
ok ^<<<<<<<<< ~~
$v
.
    open(OUT13, '>Op_write.tmp') || die "Can't create Op_write.tmp";
    write(OUT13);
    close OUT13 or die "Could not close: $!";
    print cat('Op_write.tmp');
    curr_test($test + 1);
}

{   # test 14
    # Bug #24774 format without trailing \n failed assertion, but this
    # must fail since we have a trailing ; in the eval'ed string (WL)
    my @v = ('k');
    eval "format OUT14 = \n@\n\@v";
    like $@, qr/Format not terminated/;
}

{   # test 15
    # text lost in ^<<< field with \r in value (WL)
    my $txt = "line 1\rline 2";
    format OUT15 =
^<<<<<<<<<<<<<<<<<<
$txt
^<<<<<<<<<<<<<<<<<<
$txt
.
    open(OUT15, '>Op_write.tmp') || die "Can't create Op_write.tmp";
    write(OUT15);
    close OUT15 or die "Could not close: $!";
    my $res = cat('Op_write.tmp');
    is $res, "line 1\nline 2\n";
}

{   # test 16: multiple use of a variable in same line with ^<
    my $txt = "this_is_block_1 this_is_block_2 this_is_block_3 this_is_block_4";
    format OUT16 =
^<<<<<<<<<<<<<<<< ^<<<<<<<<<<<<<<<<
$txt,             $txt
^<<<<<<<<<<<<<<<< ^<<<<<<<<<<<<<<<<
$txt,             $txt
.
    open(OUT16, '>Op_write.tmp') || die "Can't create Op_write.tmp";
    write(OUT16);
    close OUT16 or die "Could not close: $!";
    my $res = cat('Op_write.tmp');
    is $res, <<EOD;
this_is_block_1   this_is_block_2
this_is_block_3   this_is_block_4
EOD
}

{   # test 17: @* "should be on a line of its own", but it should work
    # cleanly with literals before and after. (WL)

    my $txt = "This is line 1.\nThis is the second line.\nThird and last.\n";
    format OUT17 =
Here we go: @* That's all, folks!
            $txt
.
    open(OUT17, '>Op_write.tmp') || die "Can't create Op_write.tmp";
    write(OUT17);
    close OUT17 or die "Could not close: $!";
    my $res = cat('Op_write.tmp');
    chomp( $txt );
    my $exp = <<EOD;
Here we go: $txt That's all, folks!
EOD
    is $res, $exp;
}

{   # test 18: @# and ~~ would cause runaway format, but we now
    # catch this while compiling (WL)

    format OUT18 =
@######## ~~
10
.
    open(OUT18, '>Op_write.tmp') || die "Can't create Op_write.tmp";
    eval { write(OUT18); };
    like $@,  qr/Repeated format line will never terminate/;
    close OUT18 or die "Could not close: $!";
}

{   # test 19: \0 in an evel'ed format, doesn't cause empty lines (WL)
    my $v = 'gaga';
    eval "format OUT19 = \n" .
         '@<<<' . "\0\n" .
         '$v' .   "\n" .
         '@<<<' . "\0\n" .
         '$v' . "\n.\n";
    open(OUT19, '>Op_write.tmp') || die "Can't create Op_write.tmp";
    write(OUT19);
    close OUT19 or die "Could not close: $!";
    my $res = cat('Op_write.tmp');
    is $res, <<EOD;
gaga\0
gaga\0
EOD
}

{   # test 20: hash accesses; single '}' must not terminate format '}' (WL)
    my %h = ( xkey => 'xval', ykey => 'yval' );
    format OUT20 =
@>>>> @<<<< ~~
each %h
@>>>> @<<<<
$h{xkey}, $h{ykey}
@>>>> @<<<<
{ $h{xkey}, $h{ykey}
}
}
.
    my $exp = '';
    while( my( $k, $v ) = each( %h ) ){
	$exp .= sprintf( "%5s %s\n", $k, $v );
    }
    $exp .= sprintf( "%5s %s\n", $h{xkey}, $h{ykey} );
    $exp .= sprintf( "%5s %s\n", $h{xkey}, $h{ykey} );
    $exp .= "}\n";
    open(OUT20, '>Op_write.tmp') || die "Can't create Op_write.tmp";
    write(OUT20);
    close OUT20 or die "Could not close: $!";
    my $res = cat('Op_write.tmp');
    is $res, $exp;
}


#####################
## Section 2
## numeric formatting
#####################

curr_test($bas_tests + 1);

for my $tref ( @NumTests ){
    my $writefmt = shift( @$tref );
    while (@$tref) {
	my $val      = shift @$tref;
	my $expected = shift @$tref;
        my $writeres = swrite( $writefmt, $val );
	if (ref $expected) {
	    like $writeres, $expected, $writefmt;
	} else {
	    is $writeres, $expected, $writefmt;
	}	
    }
}


#####################################
## Section 3
## Easiest to add new tests just here
#####################################

# DAPM. Exercise a couple of error codepaths

{
    local $~ = '';
    eval { write };
    like $@, qr/Undefined format ""/, 'format with 0-length name';

    $~ = "\0foo";
    eval { write };
    like $@, qr/Undefined format "\0foo"/,
	'no such format beginning with null';

    $~ = "NOSUCHFORMAT";
    eval { write };
    like $@, qr/Undefined format "NOSUCHFORMAT"/, 'no such format';
}

select +(select(OUT21), do {
    open(OUT21, '>Op_write.tmp') || die "Can't create Op_write.tmp";

    format OUT21 =
@<<
$_
.

    local $^ = '';
    local $= = 1;
    $_ = "aataaaaaaaaaaaaaa"; eval { write(OUT21) };
    like $@, qr/Undefined top format ""/, 'top format with 0-length name';

    $^ = "\0foo";
    # For some reason, we have to do this twice to get the error again.
    $_ = "aataaaaaaaaaaaaaa"; eval { write(OUT21) };
    $_ = "aataaaaaaaaaaaaaa"; eval { write(OUT21) };
    like $@, qr/Undefined top format "\0foo"/,
	'no such top format beginning with null';

    $^ = "NOSUCHFORMAT";
    $_ = "aataaaaaaaaaaaaaa"; eval { write(OUT21) };
    $_ = "aataaaaaaaaaaaaaa"; eval { write(OUT21) };
    like $@, qr/Undefined top format "NOSUCHFORMAT"/, 'no such top format';

    # reset things;
    eval { write(OUT21) };
    undef $^A;

    close OUT21 or die "Could not close: $!";
})[0];

{
  package Count;

  sub TIESCALAR {
    my $class = shift;
    bless [shift, 0, 0], $class;
  }

  sub FETCH {
    my $self = shift;
    ++$self->[1];
    $self->[0];
  }

  sub STORE {
    my $self = shift;
    ++$self->[2];
    $self->[0] = shift;
  }
}

{
  my ($pound_utf8, $pm_utf8) = map { my $a = "$_\x{100}"; chop $a; $a}
    my ($pound, $pm) = ("\xA3", "\xB1");

  foreach my $first ('N', $pound, $pound_utf8) {
    foreach my $base ('N', $pm, $pm_utf8) {
      foreach my $second ($base, "$base\n", "$base\nMoo!", "$base\nMoo!\n",
			  "$base\nMoo!\n",) {
	foreach (['^*', qr/(.+)/], ['@*', qr/(.*?)$/s]) {
	  my ($format, $re) = @$_;
	  $format = "1^*2 3${format}4";
	  foreach my $class ('', 'Count') {
	    my $name = qq{swrite("$format", "$first", "$second") class="$class"};
	    $name =~ s/\n/\\n/g;
	    $name =~ s{(.)}{
			ord($1) > 126 ? sprintf("\\x{%x}",ord($1)) : $1
		    }ge;

	    $first =~ /(.+)/ or die $first;
	    my $expect = "1${1}2";
	    $second =~ $re or die $second;
	    $expect .= " 3${1}4";

	    if ($class) {
	      my $copy1 = $first;
	      my $copy2;
	      tie $copy2, $class, $second;
	      is swrite("$format", $copy1, $copy2), $expect, $name;
	      my $obj = tied $copy2;
	      is $obj->[1], 1, 'value read exactly once';
	    } else {
	      my ($copy1, $copy2) = ($first, $second);
	      is swrite("$format", $copy1, $copy2), $expect, $name;
	    }
	  }
	}
      }
    }
  }
}

{
  # This will fail an assertion in 5.10.0 built with -DDEBUGGING (because
  # pp_formline attempts to set SvCUR() on an SVt_RV). I suspect that it will
  # be doing something similarly out of bounds on everything from 5.000
  my $ref = [];
  is swrite('>^*<', $ref), ">$ref<";
  is swrite('>@*<', $ref), ">$ref<";
}

format EMPTY =
.

my $test = curr_test();

format Comment =
ok @<<<<<
$test
.


# RT #8698 format bug with undefined _TOP

open STDOUT_DUP, ">&STDOUT";
my $oldfh = select STDOUT_DUP;
$= = 10;
{
  local $~ = "Comment";
  write;
  curr_test($test + 1);
  is $-, 9;
  is $^, "STDOUT_DUP_TOP";
}
select $oldfh;
close STDOUT_DUP;

*CmT =  *{$::{Comment}}{FORMAT};
ok  defined *{$::{CmT}}{FORMAT}, "glob assign";


# RT #91032: Check that "non-real" strings like tie and overload work,
# especially that they re-compile the pattern on each FETCH, and that
# they don't overrun the buffer


{
    package RT91032;

    sub TIESCALAR { bless [] }
    my $i = 0;
    sub FETCH { $i++; "A$i @> Z\n" }

    use overload '""' => \&FETCH;

    tie my $f, 'RT91032';

    formline $f, "a";
    formline $f, "bc";
    ::is $^A, "A1  a Z\nA2 bc Z\n", "RT 91032: tied";
    $^A = '';

    my $g = bless []; # has overloaded stringify
    formline $g, "de";
    formline $g, "f";
    ::is $^A, "A3 de Z\nA4  f Z\n", "RT 91032: overloaded";
    $^A = '';

    my $h = [];
    formline $h, "junk1";
    formline $h, "junk2";
    ::is ref($h), 'ARRAY', "RT 91032: array ref still a ref";
    ::like "$h", qr/^ARRAY\(0x[0-9a-f]+\)$/, "RT 91032: array stringifies ok";
    ::is $^A, "$h$h","RT 91032: stringified array";
    $^A = '';

    # used to overwrite the ~~ in the *original SV with spaces. Naughty!

    my $orig = my $format = "^<<<<< ~~\n";
    my $abc = "abc";
    formline $format, $abc;
    $^A ='';
    ::is $format, $orig, "RT91032: don't overwrite orig format string";

    # check that ~ and ~~ are displayed correctly as whitespace,
    # under the influence of various different types of border

    for my $n (1,2) {
	for my $lhs (' ', 'Y', '^<<<', '^|||', '^>>>') {
	    for my $rhs ('', ' ', 'Z', '^<<<', '^|||', '^>>>') {
		my $fmt = "^<B$lhs" . ('~' x $n) . "$rhs\n";
		my $sfmt = ($fmt =~ s/~/ /gr);
		my ($a, $bc, $stop);
		($a, $bc, $stop) = ('a', 'bc', 's');
		# $stop is to stop '~~' deleting the whole line
		formline $sfmt, $stop, $a, $bc;
		my $exp = $^A;
		$^A = '';
		($a, $bc, $stop) = ('a', 'bc', 's');
		formline $fmt, $stop, $a, $bc;
		my $got = $^A;
		$^A = '';
		$fmt =~ s/\n/\\n/;
		::is($got, $exp, "chop munging: [$fmt]");
	    }
	}
    }
}

# check that '~  (delete current line if empty) works when
# the target gets upgraded to uft8 (and re-allocated) midstream.

{
    my $format = "\x{100}@~\n"; # format is utf8
    # this target is not utf8, but will expand (and get reallocated)
    # when upgraded to utf8.
    my $orig = "\x80\x81\x82";
    local $^A = $orig;
    my $empty = "";
    formline $format, $empty;
    is $^A , $orig, "~ and realloc";

    # check similarly that trailing blank removal works ok

    $format = "@<\n\x{100}"; # format is utf8
    chop $format;
    $orig = "   ";
    $^A = $orig;
    formline $format, "  ";
    is $^A, "$orig\n", "end-of-line blanks and realloc";

    # and check this doesn't overflow the buffer

    local $^A = '';
    $format = "@* @####\n";
    $orig = "x" x 100 . "\n";
    formline $format, $orig, 12345;
    is $^A, ("x" x 100) . " 12345\n", "\@* doesn't overflow";

    # make sure it can cope with formats > 64k

    $format = 'x' x 65537;
    $^A = '';
    formline $format;
    # don't use 'is' here, as the diag output will be too long!
    ok $^A eq $format, ">64K";
}


SKIP: {
    skip_if_miniperl('miniperl does not support scalario');
    my $buf = "";
    open my $fh, ">", \$buf;
    my $old_fh = select $fh;
    local $~ = "CmT";
    write;
    select $old_fh;
    close $fh;
    is $buf, "ok $test\n", "write to duplicated format";
}

format caret_A_test_TOP =
T
.

format caret_A_test =
L1
L2
L3
L4
.

SKIP: {
    skip_if_miniperl('miniperl does not support scalario');
    my $buf = "";
    open my $fh, ">", \$buf;
    my $old_fh = select $fh;
    local $^ = "caret_A_test_TOP";
    local $~ = "caret_A_test";
    local $= = 3;
    local $^A = "A1\nA2\nA3\nA4\n";
    write;
    select $old_fh;
    close $fh;
    is $buf, "T\nA1\nA2\n\fT\nA3\nA4\n\fT\nL1\nL2\n\fT\nL3\nL4\n",
		    "assign to ^A sets FmLINES";
}

fresh_perl_like(<<'EOP', qr/^Format STDOUT redefined at/, {stderr => 1}, '#64562 - Segmentation fault with redefined formats and warnings');
#!./perl

use strict;
use warnings; # crashes!

format =
.

write;

format =
.

write;
EOP

fresh_perl_is(<<'EOP', ">ARRAY<\ncrunch_eth\n", {stderr => 1}, '#79532 - formline coerces its arguments');
use strict;
use warnings;
my $zamm = ['crunch_eth'];
formline $zamm;
printf ">%s<\n", ref $zamm;
print "$zamm->[0]\n";
EOP

# [perl #73690]

select +(select(RT73690), do {
    open(RT73690, '>Op_write.tmp') || die "Can't create Op_write.tmp";
    format RT73690 =
@<< @<<
11, 22
.

    my @ret;

    @ret = write;
    is(scalar(@ret), 1);
    ok($ret[0]);
    @ret = scalar(write);
    is(scalar(@ret), 1);
    ok($ret[0]);
    @ret = write(RT73690);
    is(scalar(@ret), 1);
    ok($ret[0]);
    @ret = scalar(write(RT73690));
    is(scalar(@ret), 1);
    ok($ret[0]);

    @ret = ('a', write, 'z');
    is(scalar(@ret), 3);
    is($ret[0], 'a');
    ok($ret[1]);
    is($ret[2], 'z');
    @ret = ('b', scalar(write), 'y');
    is(scalar(@ret), 3);
    is($ret[0], 'b');
    ok($ret[1]);
    is($ret[2], 'y');
    @ret = ('c', write(RT73690), 'x');
    is(scalar(@ret), 3);
    is($ret[0], 'c');
    ok($ret[1]);
    is($ret[2], 'x');
    @ret = ('d', scalar(write(RT73690)), 'w');
    is(scalar(@ret), 3);
    is($ret[0], 'd');
    ok($ret[1]);
    is($ret[2], 'w');

    @ret = do { write; 'foo' };
    is(scalar(@ret), 1);
    is($ret[0], 'foo');
    @ret = do { scalar(write); 'bar' };
    is(scalar(@ret), 1);
    is($ret[0], 'bar');
    @ret = do { write(RT73690); 'baz' };
    is(scalar(@ret), 1);
    is($ret[0], 'baz');
    @ret = do { scalar(write(RT73690)); 'quux' };
    is(scalar(@ret), 1);
    is($ret[0], 'quux');

    @ret = ('a', do { write; 'foo' }, 'z');
    is(scalar(@ret), 3);
    is($ret[0], 'a');
    is($ret[1], 'foo');
    is($ret[2], 'z');
    @ret = ('b', do { scalar(write); 'bar' }, 'y');
    is(scalar(@ret), 3);
    is($ret[0], 'b');
    is($ret[1], 'bar');
    is($ret[2], 'y');
    @ret = ('c', do { write(RT73690); 'baz' }, 'x');
    is(scalar(@ret), 3);
    is($ret[0], 'c');
    is($ret[1], 'baz');
    is($ret[2], 'x');
    @ret = ('d', do { scalar(write(RT73690)); 'quux' }, 'w');
    is(scalar(@ret), 3);
    is($ret[0], 'd');
    is($ret[1], 'quux');
    is($ret[2], 'w');

    close RT73690 or die "Could not close: $!";
})[0];

select +(select(RT73690_2), do {
    open(RT73690_2, '>Op_write.tmp') || die "Can't create Op_write.tmp";
    format RT73690_2 =
@<< @<<
return
.

    my @ret;

    @ret = write;
    is(scalar(@ret), 1);
    ok(!$ret[0]);
    @ret = scalar(write);
    is(scalar(@ret), 1);
    ok(!$ret[0]);
    @ret = write(RT73690_2);
    is(scalar(@ret), 1);
    ok(!$ret[0]);
    @ret = scalar(write(RT73690_2));
    is(scalar(@ret), 1);
    ok(!$ret[0]);

    @ret = ('a', write, 'z');
    is(scalar(@ret), 3);
    is($ret[0], 'a');
    ok(!$ret[1]);
    is($ret[2], 'z');
    @ret = ('b', scalar(write), 'y');
    is(scalar(@ret), 3);
    is($ret[0], 'b');
    ok(!$ret[1]);
    is($ret[2], 'y');
    @ret = ('c', write(RT73690_2), 'x');
    is(scalar(@ret), 3);
    is($ret[0], 'c');
    ok(!$ret[1]);
    is($ret[2], 'x');
    @ret = ('d', scalar(write(RT73690_2)), 'w');
    is(scalar(@ret), 3);
    is($ret[0], 'd');
    ok(!$ret[1]);
    is($ret[2], 'w');

    @ret = do { write; 'foo' };
    is(scalar(@ret), 1);
    is($ret[0], 'foo');
    @ret = do { scalar(write); 'bar' };
    is(scalar(@ret), 1);
    is($ret[0], 'bar');
    @ret = do { write(RT73690_2); 'baz' };
    is(scalar(@ret), 1);
    is($ret[0], 'baz');
    @ret = do { scalar(write(RT73690_2)); 'quux' };
    is(scalar(@ret), 1);
    is($ret[0], 'quux');

    @ret = ('a', do { write; 'foo' }, 'z');
    is(scalar(@ret), 3);
    is($ret[0], 'a');
    is($ret[1], 'foo');
    is($ret[2], 'z');
    @ret = ('b', do { scalar(write); 'bar' }, 'y');
    is(scalar(@ret), 3);
    is($ret[0], 'b');
    is($ret[1], 'bar');
    is($ret[2], 'y');
    @ret = ('c', do { write(RT73690_2); 'baz' }, 'x');
    is(scalar(@ret), 3);
    is($ret[0], 'c');
    is($ret[1], 'baz');
    is($ret[2], 'x');
    @ret = ('d', do { scalar(write(RT73690_2)); 'quux' }, 'w');
    is(scalar(@ret), 3);
    is($ret[0], 'd');
    is($ret[1], 'quux');
    is($ret[2], 'w');

    close RT73690_2 or die "Could not close: $!";
})[0];

open(UNDEF, '>Op_write.tmp') || die "Can't create Op_write.tmp";
select +(select(UNDEF), $~ = "UNDEFFORMAT")[0];
format UNDEFFORMAT =
@
undef *UNDEFFORMAT
.
write UNDEF;
pass "active format cannot be freed";

select +(select(UNDEF), $~ = "UNDEFFORMAT2")[0];
format UNDEFFORMAT2 =
@
close UNDEF or die "Could not close: $!"; undef *UNDEF
.
write UNDEF;
pass "freeing current handle in format";
undef $^A;

ok !eval q|
format foo {
@<<<
$a
}
;1
|, 'format foo { ... } is not allowed';

ok !eval q|
format =
@<<<
}
;1
|, 'format = ... } is not allowed';

open(NEST, '>Op_write.tmp') || die "Can't create Op_write.tmp";
format NEST =
@<<<
{
    my $birds = "birds";
    local *NEST = *BIRDS{FORMAT};
    write NEST;
    format BIRDS =
@<<<<<
$birds;
.
    "nest"
}
.
write NEST;
close NEST or die "Could not close: $!";
is cat('Op_write.tmp'), "birds\nnest\n", 'nested formats';

# A compilation error should not create a format
eval q|
format ERROR =
@
@_ =~ s///
.
|;
eval { write ERROR };
like $@, qr'Undefined format',
    'formats with compilation errors are not created';

# This syntax error used to cause a crash, double free, or a least
# a bad read.
# See the long-winded explanation at:
#   https://rt.perl.org/rt3/Ticket/Display.html?id=43425#txn-1144500
eval q|
format =
@
use;format
strict
.
|;
pass('no crash with invalid use/format inside format');


# Low-precedence operators on argument line
format AND =
@
0 and die
.
$- = $=;
ok eval { local $~ = "AND"; print "# "; write; 1 },
    "low-prec ops on arg line" or diag $@;

# Anonymous hashes
open(HASH, '>Op_write.tmp') || die "Can't create Op_write.tmp";
format HASH =
@<<<
${{qw[ Sun 0 Mon 1 Tue 2 Wed 3 Thu 4 Fri 5 Sat 6 ]}}{"Wed"}
.
write HASH;
close HASH or die "Could not close: $!";
is cat('Op_write.tmp'), "3\n", 'anonymous hashes';

# pragmata inside argument line
open(STRICT, '>Op_write.tmp') || die "Can't create Op_write.tmp";
format STRICT =
@<<<
no strict; $foo
.
$::foo = 'oof::$';
write STRICT;
close STRICT or die "Could not close: $!";
is cat('Op_write.tmp'), "oof:\n", 'pragmata on format line';

SKIP: {
   skip "no weak refs" unless eval { require Scalar::Util };
   sub Potshriggley {
format Potshriggley =
.
   }
   Scalar::Util::weaken(my $x = *Potshriggley{FORMAT});
   undef *Potshriggley;
   is $x, undef, 'formats in subs do not leak';
}


#############################
## Section 4
## Add new tests *above* here
#############################

# scary format testing from H.Merijn Brand

# Just a complete test for format, including top-, left- and bottom marging
# and format detection through glob entries

if ($^O eq 'VMS' || $^O eq 'MSWin32' || $^O eq 'dos' ||
    ($^O eq 'os2' and not eval '$OS2::can_fork')) {
  $test = curr_test();
 SKIP: {
      skip "'|-' and '-|' not supported", $tests - $test + 1;
  }
  exit(0);
}


$^  = "STDOUT_TOP";
$=  =  7;		# Page length
$-  =  0;		# Lines left
my $ps = $^L; $^L = "";	# Catch the page separator
my $tm =  1;		# Top margin (empty lines before first output)
my $bm =  2;		# Bottom marging (empty lines between last text and footer)
my $lm =  4;		# Left margin (indent in spaces)

# -----------------------------------------------------------------------
#
# execute the rest of the script in a child process. The parent reads the
# output from the child and compares it with <DATA>.

my @data = <DATA>;

select ((select (STDOUT), $| = 1)[0]); # flush STDOUT

my $opened = open FROM_CHILD, "-|";
unless (defined $opened) {
    fail "open gave $!";
    exit 0;
}

if ($opened) {
    # in parent here

    pass 'open';
    my $s = " " x $lm;
    while (<FROM_CHILD>) {
	unless (@data) {
	    fail 'too much output';
	    exit;
	}
	s/^/$s/;
	my $exp = shift @data;
	is $_, $exp;
    }
    close FROM_CHILD;
    is "@data", "", "correct length of output";
    exit;
}

# in child here
$::NO_ENDING = 1;

    select ((select (STDOUT), $| = 1)[0]);
$tm = "\n" x $tm;
$= -= $bm + 1; # count one for the trailing "----"
my $lastmin = 0;

my @E;

sub wryte
{
    $lastmin = $-;
    write;
    } # wryte;

sub footer
{
    $% == 1 and return "";

    $lastmin < $= and print "\n" x $lastmin;
    print "\n" x $bm, "----\n", $ps;
    $lastmin = $-;
    "";
    } # footer

# Yes, this is sick ;-)
format TOP =
@* ~
@{[footer]}
@* ~
$tm
.

format ENTRY =
@ @<<<<~~
@{(shift @E)||["",""]}
.

format EOR =
- -----
.

sub has_format ($)
{
    my $fmt = shift;
    exists $::{$fmt} or return 0;
    $^O eq "MSWin32" or return defined *{$::{$fmt}}{FORMAT};
    open my $null, "> /dev/null" or die;
    my $fh = select $null;
    local $~ = $fmt;
    eval "write";
    select $fh;
    $@?0:1;
    } # has_format

$^ = has_format ("TOP") ? "TOP" : "EMPTY";
has_format ("ENTRY") or die "No format defined for ENTRY";
foreach my $e ( [ map { [ $_, "Test$_"   ] } 1 .. 7 ],
		[ map { [ $_, "${_}tseT" ] } 1 .. 5 ]) {
    @E = @$e;
    local $~ = "ENTRY";
    wryte;
    has_format ("EOR") or next;
    local $~ = "EOR";
    wryte;
    }
if (has_format ("EOF")) {
    local $~ = "EOF";
    wryte;
    }

close STDOUT;

# That was test 48.

__END__
    
    1 Test1
    2 Test2
    3 Test3
    
    
    ----
    
    4 Test4
    5 Test5
    6 Test6
    
    
    ----
    
    7 Test7
    - -----
    
    
    
    ----
    
    1 1tseT
    2 2tseT
    3 3tseT
    
    
    ----
    
    4 4tseT
    5 5tseT
    - -----
