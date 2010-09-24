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
my $bas_tests = 20;

# number of tests in section 3
my $bug_tests = 4 + 3 * 3 * 5 * 2 * 3 + 2 + 1 + 1;

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
END { 1 while unlink 'Op_write.tmp' }

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

is cat('Op_write.tmp'), $right and do { 1 while unlink 'Op_write.tmp'; };

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

is cat('Op_write.tmp'), $right and do { 1 while unlink 'Op_write.tmp'; };

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

is cat('Op_write.tmp'), $right and do { 1 while unlink 'Op_write.tmp' };

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

is cat('Op_write.tmp'), $right and do { 1 while unlink 'Op_write.tmp' };


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
is cat('Op_write.tmp'), "1\n" and do { 1 while unlink "Op_write.tmp" };

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
is cat('Op_write.tmp'), $right and do { 1 while unlink 'Op_write.tmp' };

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
is cat('Op_write.tmp'), $right and do { 1 while unlink 'Op_write.tmp' };

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
    like $@, qr/Not a format reference/, 'format reference';

    $~ = "NOSUCHFORMAT";
    eval { write };
    like $@, qr/Undefined format/, 'no such format';
}

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
	  foreach my $class ('', 'Count') {
	    my $name = "$first, $second $format $class";
	    $name =~ s/\n/\\n/g;

	    $first =~ /(.+)/ or die $first;
	    my $expect = "1${1}2";
	    $second =~ $re or die $second;
	    $expect .= " 3${1}4";

	    if ($class) {
	      my $copy1 = $first;
	      my $copy2;
	      tie $copy2, $class, $second;
	      is swrite("1^*2 3${format}4", $copy1, $copy2), $expect, $name;
	      my $obj = tied $copy2;
	      is $obj->[1], 1, 'value read exactly once';
	    } else {
	      my ($copy1, $copy2) = ($first, $second);
	      is swrite("1^*2 3${format}4", $copy1, $copy2), $expect, $name;
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


# [ID 20020227.005] format bug with undefined _TOP

open STDOUT_DUP, ">&STDOUT";
my $oldfh = select STDOUT_DUP;
$= = 10;
{
  local $~ = "Comment";
  write;
  curr_test($test + 1);
  {
    local $::TODO = '[ID 20020227.005] format bug with undefined _TOP';
    is $-, 9;
  }
  is $^, "STDOUT_DUP_TOP";
}
select $oldfh;
close STDOUT_DUP;

*CmT =  *{$::{Comment}}{FORMAT};
ok  defined *{$::{CmT}}{FORMAT}, "glob assign";

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
