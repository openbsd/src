#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

print "1..49\n";

my $CAT = ($^O eq 'MSWin32' || $^O eq 'NetWare' || $^O eq 'VMS') ? 'type'
	: ($^O eq 'MacOS') ? 'catenate'
        : 'cat';

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

if (`$CAT Op_write.tmp` eq $right)
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

if (`$CAT Op_write.tmp` eq $right)
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

if (`$CAT Op_write.tmp` eq $right)
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

if (`$CAT Op_write.tmp` eq $right)
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
if (`$CAT Op_write.tmp` eq "1\n") {
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
if (`$CAT Op_write.tmp` eq $right)
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
if (`$CAT Op_write.tmp` eq $right)
    { print "ok 11\n"; 1 while unlink 'Op_write.tmp'; }
else
    { print "not ok 11\n"; }

{
    our $el;
    format STDOUT =
ok ^<<<<<<<<<<<<<<~~ # sv_chop() naze
$el
.
    my %hash = (12 => 3);
    for $el (keys %hash) {
	write;
    }
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
    print `$CAT Op_write.tmp`;
}

#######################################
# Easiest to add new tests above here #
#######################################

# 14..49: scary format testing from Merijn H. Brand

my $test = 14;
my $tests = 49;

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

$= = 10;

# [ID 20020227.005] format bug with undefined _TOP
{   local $~ = "Comment";
    write;
    $test++;
    print $- == 9
	? "ok $test\n" : "not ok $test # TODO \$- = $- instead of 9\n";
    $test++;
    print $^ ne "Comment_TOP"
	? "ok $test\n" : "not ok $test # TODO \$^ = $^ instead of 'STDOUT_TOP'\n";
    $test++;
    }

   $^  = "STDOUT_TOP";
   $=  =  7;		# Page length
   $-  =  0;		# Lines left
my $ps = $^L; $^L = "";	# Catch the page separator
my $tm =  1;		# Top margin (empty lines before first output)
my $bm =  2;		# Bottom marging (empty lines between last text and footer)
my $lm =  4;		# Left margin (indent in spaces)

select ((select (STDOUT), $| = 1)[0]);
if ($lm > 0 and !open STDOUT, "|-") {	# Left margin (in this test ALWAYS set)
    select ((select (STDOUT), $| = 1)[0]);
    my $s = " " x $lm;
    while (<STDIN>) {
	s/^/$s/;
	print + ($_ eq <DATA> ? "" : "not "), "ok ", $test++, "\n";
	}
    close STDIN;
    print + (<DATA>?"not ":""), "ok ", $test++, "\n";
    close STDOUT;
    exit;
    }
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
