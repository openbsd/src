#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

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
my $hmb_tests = 37;

printf "1..%d\n", $bas_tests + $num_tests + $hmb_tests;

############
## Section 1
############

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

$right =
"the quick brown fox
jumped
forescore
and
seven years
when in
the course
of huma...
now is the time for all good men to come to\n";

if (cat('Op_write.tmp') eq $right)
    { print "ok 1\n"; 1 while unlink 'Op_write.tmp'; }
else
    { print "not ok 1\n"; }

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

if (cat('Op_write.tmp') eq $right)
    { print "ok 2\n"; 1 while unlink 'Op_write.tmp'; }
else
    { print "not ok 2\n"; }

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

if (cat('Op_write.tmp') eq $right)
    { print "ok 3\n"; 1 while unlink 'Op_write.tmp'; }
else
    { print "not ok 3\n"; }

# formline tests

$mustbe = <<EOT;
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

$was1 = $was2 = '';
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
print $was1 eq $mustbe ? "ok 4\n" : "not ok 4\n";
print $was2 eq $mustbe ? "ok 5\n" : "not ok 5\n";

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

if (cat('Op_write.tmp') eq $right)
    { print "ok 6\n"; 1 while unlink 'Op_write.tmp'; }
else
    { print "not ok 6\n"; }

# test lexicals and globals
{
    my $this = "ok";
    our $that = 7;
    format LEX =
@<<@|
$this,$that
.
    open(LEX, ">&STDOUT") or die;
    write LEX;
    $that = 8;
    write LEX;
    close LEX or die "Could not close: $!";
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
if (cat('Op_write.tmp') eq "1\n") {
    print "ok 9\n";
    1 while unlink "Op_write.tmp";
    }
else {
    print "not ok 9\n";
    }

eval <<'EOFORMAT';
format OUT10 =
@####.## @0###.##
$test1, $test1
.
EOFORMAT

open(OUT10, '>Op_write.tmp') || die "Can't create Op_write.tmp";

$test1 = 12.95;
write(OUT10);
close OUT10 or die "Could not close: $!";

$right = "   12.95 00012.95\n";
if (cat('Op_write.tmp') eq $right)
    { print "ok 10\n"; 1 while unlink 'Op_write.tmp'; }
else
    { print "not ok 10\n"; }

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
if (cat('Op_write.tmp') eq $right)
    { print "ok 11\n"; 1 while unlink 'Op_write.tmp'; }
else
    { print "not ok 11\n"; }

{
    our $el;
    format OUT12 =
ok ^<<<<<<<<<<<<<<~~ # sv_chop() naze
$el
.
    my %hash = (12 => 3);
    open(OUT12, '>Op_write.tmp') || die "Can't create Op_write.tmp";

    for $el (keys %hash) {
	write(OUT12);
    }
    close OUT12 or die "Could not close: $!";
    print cat('Op_write.tmp');

}

{
    # Bug report and testcase by Alexey Tourbin
    use Tie::Scalar;
    my $v;
    tie $v, 'Tie::StdScalar';
    $v = 13;
    format OUT13 =
ok ^<<<<<<<<< ~~
$v
.
    open(OUT13, '>Op_write.tmp') || die "Can't create Op_write.tmp";
    write(OUT13);
    close OUT13 or die "Could not close: $!";
    print cat('Op_write.tmp');
}

{   # test 14
    # Bug #24774 format without trailing \n failed assertion, but this
    # must fail since we have a trailing ; in the eval'ed string (WL)
    my @v = ('k');
    eval "format OUT14 = \n@\n\@v";
    print +($@ && $@ =~ /Format not terminated/)
      ? "ok 14\n" : "not ok 14 $@\n";

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
    print $res eq "line 1\nline 2\n" ? "ok 15\n" : "not ok 15\n";
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
    print $res eq <<EOD ? "ok 16\n" : "not ok 16\n";
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
    print $res eq $exp ? "ok 17\n" : "not ok 17\n";
}

{   # test 18: @# and ~~ would cause runaway format, but we now
    # catch this while compiling (WL)

    format OUT18 =
@######## ~~
10
.
    open(OUT18, '>Op_write.tmp') || die "Can't create Op_write.tmp";
    eval { write(OUT18); };
    print +($@ && $@ =~ /Repeated format line will never terminate/)
      ? "ok 18\n" : "not ok 18: $@\n";
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
    print $res eq <<EOD ? "ok 19\n" : "not ok 19\n";
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
    print $res eq $exp ? "ok 20\n" : "not ok 20 res=[$res]exp=[$exp]\n";
}


#####################
## Section 2
## numeric formatting
#####################

my $nt = $bas_tests;
for my $tref ( @NumTests ){
    my $writefmt = shift( @$tref );
    while (@$tref) {
	my $val      = shift @$tref;
	my $expected = shift @$tref;
        my $writeres = swrite( $writefmt, $val );
        $nt++;
	my $ok = ref($expected)
		 ? $writeres =~ $expected
		 : $writeres eq $expected;
	
        print $ok
	    ? "ok $nt - $writefmt\n"
	    : "not ok $nt\n# f=[$writefmt] exp=[$expected] got=[$writeres]\n";
    }
}


#####################################
## Section 3
## Easiest to add new tests above here
#######################################

# scary format testing from H.Merijn Brand

my $test = $bas_tests + $num_tests + 1;
my $tests = $bas_tests + $num_tests + $hmb_tests;

if ($^O eq 'VMS' || $^O eq 'MSWin32' || $^O eq 'dos' || $^O eq 'MacOS' ||
    ($^O eq 'os2' and not eval '$OS2::can_fork')) {
  foreach ($test..$tests) {
      print "ok $_ # skipped: '|-' and '-|' not supported\n";
  }
  exit(0);
}


use strict;	# Amazed that this hackery can be made strict ...

# Just a complete test for format, including top-, left- and bottom marging
# and format detection through glob entries

format EMPTY =
.

format Comment =
ok @<<<<<
$test
.


# [ID 20020227.005] format bug with undefined _TOP

open STDOUT_DUP, ">&STDOUT";
my $oldfh = select STDOUT_DUP;
$= = 10;
{   local $~ = "Comment";
    write;
    $test++;
    print $- == 9
	? "ok $test # TODO\n" : "not ok $test # TODO \$- = $- instead of 9\n";
    $test++;
    print $^ eq "STDOUT_DUP_TOP"
	? "ok $test\n" : "not ok $test\n# \$^ = $^ instead of 'STDOUT_DUP_TOP'\n";
    $test++;
}
select $oldfh;
close STDOUT_DUP;

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
    print "not ok $test - open gave $!\n"; exit 0;
}

if ($opened) {
    # in parent here

    print "ok $test - open\n"; $test++;
    my $s = " " x $lm;
    while (<FROM_CHILD>) {
	unless (@data) {
	    print "not ok $test - too much output\n";
	    exit;
	}
	s/^/$s/;
	my $exp = shift @data;
	print + ($_ eq $exp ? "" : "not "), "ok ", $test++, " \n";
	if ($_ ne $exp) {
	    s/\n/\\n/g for $_, $exp;
	    print "#expected: $exp\n#got:      $_\n";
	}
    }
    close FROM_CHILD;
    print + (@data?"not ":""), "ok ", $test++, " - too litle output\n";
    exit;
}

# in child here

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
