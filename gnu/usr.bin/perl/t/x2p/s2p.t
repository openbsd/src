#!./perl

=head1 NAME

s2p.t - test suite for s2p/psed

=head1 NOTES

The general idea is to

  (a) run psed with a sed script and input data to obtain some output
  (b) run s2p with a sed script creating a Perl program and then run the
      Perl program with the input data, again producing output

Both final outputs should be identical to the expected output.

A $testcase{<name>} contains entries (after the comment ### <name> ###):

  - script: the sed script
  - input:  the key of the input data, stored in $input{<input>}
  - expect: the expected output
  - datfil: an additional file [ <path>, <data> ] (if required)

Temporary files are created in the working directory (embedding $$
in the name), and removed after the test.

Except for bin2dec (which indeed converts binary to decimal) none of the
sed scripts is doing something useful.

Author: Wolfgang Laun.

=cut

BEGIN {
    chdir 't' if -d 't';
    @INC = ( '../lib' );
}

use File::Copy;
use File::Spec;
require './test.pl';

# BRE extensions
$ENV{PSEDEXTBRE} = '<>wW';

our %input = (
   bins => <<'[TheEnd]',
0
111
1000
10001
[TheEnd]

   text => <<'[TheEnd]',
line 1
line 2
line 3
line 4
line 5
line 6
line 7
line 8
[TheEnd]

   adr1 => <<'[TheEnd]',
#no autoprint
# This script should be run on itself
/^#__DATA__$/,${
   /^#A$/p
   s/^# *[0-9]* *//
   /^#\*$/p
   /^#\.$/p
   /^#\(..\)\(..\)\2\1*$/p
   /^#[abc]\{1,\}[def]\{1,\}$/p
}
#__DATA__
#A
#*
#.
#abxyxy
#abxyxyab
#abxyxyabab
#ad
#abcdef
[TheEnd]
);


our %testcase = (

### bin2dec ###
'bin2dec' => {
  script => <<'[TheEnd]',
# binary -> decimal
s/^[ 	]*\([01]\{1,\}\)[ 	]*/\1/
t go
i\
is not a binary number
d

# expand binary to Xs
: go
s/^0*//
s/^1/X/
: expand
s/^\(X\{1,\}\)0/\1\1/
s/^\(X\{1,\}\)1/\1\1X/
t expand

# count Xs in decimal
: count
s/^X/1/
s/0X/1/
s/1X/2/
s/2X/3/
s/3X/4/
s/4X/5/
s/5X/6/
s/6X/7/
s/7X/8/
s/8X/9/
s/9X/X0/
t count
s/^$/0/
[TheEnd]
  input  => 'bins',
  expect => <<'[TheEnd]',
0
7
8
17
[TheEnd]
},


### = ###
'=' => {
  script => <<'[TheEnd]',
1=
$=
[TheEnd]
  input  => 'text',
  expect => <<'[TheEnd]',
1
line 1
line 2
line 3
line 4
line 5
line 6
line 7
8
line 8
[TheEnd]
},

### D ###
'D' => {
  script => <<'[TheEnd]',
#no autoprint
/1/{
N
N
N
D
}
p
/2/D
=
p
[TheEnd]
  input  => 'text',
  expect => <<'[TheEnd]',
line 2
line 3
line 4
line 3
line 4
4
line 3
line 4
line 5
5
line 5
line 6
6
line 6
line 7
7
line 7
line 8
8
line 8
[TheEnd]
},

### H ###
'H' => {
  script => <<'[TheEnd]',
#no autoprint
1,$H
$g
$=
$p
[TheEnd]
  input  => 'text',
  expect => <<'[TheEnd]',
8

line 1
line 2
line 3
line 4
line 5
line 6
line 7
line 8
[TheEnd]
},

### N ###
'N' => {
  script => <<'[TheEnd]',
3a\
added line
4a\
added line
5a\
added line
3,5N
=
d
[TheEnd]
  input  => 'text',
  expect => <<'[TheEnd]',
1
2
added line
4
added line
6
7
8
[TheEnd]
},

### P ###
'P' => {
  script => <<'[TheEnd]',
1N
2N
3N
4=
4P
4,$d
[TheEnd]
  input  => 'text',
  expect => <<'[TheEnd]',
4
line 1
[TheEnd]
},

### a ###
'a' => {
  script => <<'[TheEnd]',
1a\
added line 1.1\
added line 1.2

3a\
added line 3.1
3a\
added line 3.2

[TheEnd]
  input  => 'text',
  expect => <<'[TheEnd]',
line 1
added line 1.1
added line 1.2
line 2
line 3
added line 3.1
added line 3.2
line 4
line 5
line 6
line 7
line 8
[TheEnd]
},

### b ###
'b' => {
  script => <<'[TheEnd]',
#no autoprint
2 b eos
4 b eos
p
: eos
[TheEnd]
  input  => 'text',
  expect => <<'[TheEnd]',
line 1
line 3
line 5
line 6
line 7
line 8
[TheEnd]
},

### block ###
'block' => {
  script => "#no autoprint\n1,3{\n=\np\n}",
  input  => 'text',
  expect => <<'[TheEnd]',
1
line 1
2
line 2
3
line 3
[TheEnd]
},

### c ###
'c' => {
  script => <<'[TheEnd]',
2=

2,4c\
change 2,4 line 1\
change 2,4 line 2

2=

3,5c\
change 3,5 line 1\
change 3,5 line 2

3=

[TheEnd]
  input  => 'text',
  expect => <<'[TheEnd]',
line 1
2
change 2,4 line 1
change 2,4 line 2
line 5
line 6
line 7
line 8
[TheEnd]
},

### c1 ###
'c1' => {
  script => <<'[TheEnd]',
1c\
replaces line 1

2,3c\
replaces lines 2-3

/5/,/6/c\
replaces lines 3-4

8,10c\
replaces lines 6-10
[TheEnd]
  input  => 'text',
  expect => <<'[TheEnd]',
replaces line 1
replaces lines 2-3
line 4
replaces lines 3-4
line 7
[TheEnd]
},

### c2 ###
'c2' => {
  script => <<'[TheEnd]',
3!c\
replace all except line 3

[TheEnd]
  input  => 'text',
  expect => <<'[TheEnd]',
replace all except line 3
replace all except line 3
line 3
replace all except line 3
replace all except line 3
replace all except line 3
replace all except line 3
replace all except line 3
[TheEnd]
},

### c3 ###
'c3' => {
  script => <<'[TheEnd]',
1,4!c\
replace all except 1-4

/5/,/8/!c\
replace all except 5-8
[TheEnd]
  input  => 'text',
  expect => <<'[TheEnd]',
replace all except 5-8
replace all except 5-8
replace all except 5-8
replace all except 5-8
replace all except 1-4
replace all except 1-4
replace all except 1-4
replace all except 1-4
[TheEnd]
},

### d ###
'd' => {
  script => <<'[TheEnd]',
# d delete pattern space, start next cycle
2,4 d
5 d
[TheEnd]
  input  => 'text',
  expect => <<'[TheEnd]',
line 1
line 6
line 7
line 8
[TheEnd]
},

### gh ###
'gh' => {
  script => <<'[TheEnd]',
1h
2g
3h
4g
5q
[TheEnd]
  input  => 'text',
  expect => <<'[TheEnd]',
line 1
line 1
line 3
line 3
line 5
[TheEnd]
},

### i ###
'i' => {
  script => <<'[TheEnd]',
1i\
inserted line 1.1\
inserted line 1.2

3i\
inserted line 3.1
3i\
inserted line 3.2
[TheEnd]
  input  => 'text',
  expect => <<'[TheEnd]',
inserted line 1.1
inserted line 1.2
line 1
line 2
inserted line 3.1
inserted line 3.2
line 3
line 4
line 5
line 6
line 7
line 8
[TheEnd]
},

### n ###
'n' => {
  script => <<'[TheEnd]',
3a\
added line
4a\
added line
5a\
added line
3,5n
=
d
[TheEnd]
  input  => 'text',
  expect => <<'[TheEnd]',
1
2
line 3
added line
4
line 5
added line
6
7
8
[TheEnd]
},

### o ###
'o' => {
  script => <<'[TheEnd]',
/abc/,/def/ s//XXX/
// i\
cheers
[TheEnd]
  input  => 'text',
  expect => <<'[TheEnd]',
line 1
line 2
line 3
line 4
line 5
line 6
line 7
line 8
[TheEnd]
},

### q ###
'q' => {
  script => <<'[TheEnd]',
2a\
append to line 2
3a\
append to line 3 - should not appear in output
3q
[TheEnd]
  input  => 'text',
  expect => <<'[TheEnd]',
line 1
line 2
append to line 2
line 3
[TheEnd]
},

### r ###
'r' => {
  datfil => [ 'r.txt', "r.txt line 1\nr.txt line 2\nr.txt line 3\n" ],
  script => <<'[TheEnd]',
2r%r.txt%
4r %r.txt%
[TheEnd]
  input  => 'text',
  expect => <<'[TheEnd]',
line 1
line 2
r.txt line 1
r.txt line 2
r.txt line 3
line 3
line 4
r.txt line 1
r.txt line 2
r.txt line 3
line 5
line 6
line 7
line 8
[TheEnd]
},

### s ###
's' => {
  script => <<'[TheEnd]',
# enclose any '(a)'.. '(c)' in '-'
s/([a-z])/-\1-/g

s/\([abc]\)/-\1-/g
[TheEnd]
  input  => 'text',
  expect => <<'[TheEnd]',
line 1
line 2
line 3
line 4
line 5
line 6
line 7
line 8
[TheEnd]
},

### s1 ###
's1' => {
  script => <<'[TheEnd]',
s/\w/@1/
s/\y/@2/

s/\n/@3/

# this is literal { }
s/a{3}/@4/

# proper repetition
s/a\{3\}/a rep 3/
[TheEnd]
  input  => 'text',
  expect => <<'[TheEnd]',
@1ine 1
@1ine 2
@1ine 3
@1ine 4
@1ine 5
@1ine 6
@1ine 7
@1ine 8
[TheEnd]
},

### s2 ### RT #115156
's2' => {
  todo   => 'RT #115156',
  script => 's/1*$/x/g',
  input  => 'bins',
  expect => <<'[TheEnd]',
0x
x
1000x
1000x
[TheEnd]
},

### t ###
't' => {
  script => join( "\n",
   '#no autoprint', 's/./X/p', 's/foo/bar/p', 't bye', '=', 'p', ':bye' ),
  input  => 'text',
  expect => <<'[TheEnd]',
Xine 1
Xine 2
Xine 3
Xine 4
Xine 5
Xine 6
Xine 7
Xine 8
[TheEnd]
},

### w ###
'w' => {
  datfil => [ 'w.txt', '' ],
  script => <<'[TheEnd]',
w %w.txt%
[TheEnd]
  input  => 'text',
  expect => <<'[TheEnd]',
line 1
line 2
line 3
line 4
line 5
line 6
line 7
line 8
[TheEnd]
},

### x ###
'x' => {
  script => <<'[TheEnd]',
1h
1d
2x
2,$G
[TheEnd]
  input  => 'text',
  expect => <<'[TheEnd]',
line 1
line 2
line 3
line 2
line 4
line 2
line 5
line 2
line 6
line 2
line 7
line 2
line 8
line 2
[TheEnd]
},

### y ###
'y' => {
  script => <<'[TheEnd]',
y/abcdefghijklmnopqrstuvwxyz/ABCDEFGHIJKLMNOPQRSTUVWXYZ/
y/|/\
/ 
[TheEnd]
  input  => 'text',
  expect => <<'[TheEnd]',
LINE 1
LINE 2
LINE 3
LINE 4
LINE 5
LINE 6
LINE 7
LINE 8
[TheEnd]
},

### cnt ###
'cnt' => {
  script => <<'[TheEnd]',
#no autoprint

# delete line, append NL to hold space
s/.*//
H
$!b

# last line only: get hold
g
s/./X/g
t count
: count
s/^X/1/
s/0X/1/
s/1X/2/
s/2X/3/
s/3X/4/
s/4X/5/
s/5X/6/
s/6X/7/
s/7X/8/
s/8X/9/
s/9X/X0/
t count
p
[TheEnd]
  input  => 'text',
  expect => <<'[TheEnd]',
8
[TheEnd]
},

### adr1 ###
'adr1' => {
  script => <<'[TheEnd]',
#no autoprint
# This script should be run on itself
/^#__DATA__$/,${
   /^#A$/p
   s/^# *[0-9]* *//
   /^#\*$/p
   /^#\.$/p
   /^#\(..\)\(..\)\2\1*$/p
   /^#[abc]\{1,\}[def]\{1,\}$/p
}
#__DATA__
#A
#*
#.
#abxyxy
#abxyxyab
#abxyxyabab
#ad
#abcdef
[TheEnd]
  input  => 'adr1',
  expect => <<'[TheEnd]',
#A
[TheEnd]
},

);

my @aux = ();
my $ntc = 2 * keys %testcase;
plan( $ntc );

# temporary file names
my $script = "s2pt$$.sed";
my $stdin  = "s2pt$$.in";
my $plsed  = "s2pt$$.pl";

# various command lines for 
my $s2p  = File::Spec->catfile( File::Spec->updir(), 'x2p', 's2p' );
my $psed = File::Spec->catfile( File::Spec->curdir(), 'psed' );
if ($^O eq 'VMS') {
  # default in the .com extension if it's not already there
  $s2p = VMS::Filespec::vmsify($s2p);
  $psed = VMS::Filespec::vmsify($psed);
  # Converting file specs from Unix format to VMS with the extended
  # character set active can result in a trailing '.' added for null
  # extensions.  This must be removed if the intent is to default the
  # extension.
  $s2p =~ s/\.$//;
  $psed =~ s/\.$//;
  $s2p = VMS::Filespec::rmsexpand($s2p, '.com');
  $psed = VMS::Filespec::rmsexpand($psed, '.com');
}
my $sedcmd = [ $psed, '-f', $script, $stdin ];
my $s2pcmd = [ $s2p,  '-f', $script ];
my $plcmd  = [ $plsed, $stdin ];

# psed: we create a local copy as linking may not work on some systems.
copy( $s2p, $psed );
push( @aux, $psed );

# process all testcases
#
my $indat = '';
for my $tc ( sort keys %testcase ){
    my( $psedres, $s2pres );

    local $TODO = $testcase{$tc}{todo};

    # 1st test: run psed
    # prepare the script 
    open( SED, ">$script" ) || goto FAIL_BOTH;
    my $script = $testcase{$tc}{script};

    # additional files for r, w: patch script, inserting temporary names
    if( exists( $testcase{$tc}{datfil} ) ){
        my( $datnam, $datdat ) = @{$testcase{$tc}{datfil}};
        my $datfil = "s2pt$$" . $datnam;
        push( @aux, $datfil );
        open( DAT, ">$datfil" ) || goto FAIL_BOTH;
        print DAT $datdat;
        close( DAT );
        $script =~ s/\%$datnam\%/$datfil/eg;
    }
    print SED $script;
    close( SED ) || goto FAIL_BOTH;

    # prepare input
    #
    if( $indat ne $testcase{$tc}{input} ){
        $indat = $testcase{$tc}{input};
        open( IN, ">$stdin" ) || goto FAIL_BOTH;
        print IN $input{$indat};
        close( IN ) || goto FAIL_BOTH;
    }

    # on VMS, runperl eats blank lines to work around 
    # spurious newlines in pipes
    $testcase{$tc}{expect} =~ s/\n\n/\n/ if $^O eq 'VMS';

    # run and compare
    #
    $psedres = runperl( args => $sedcmd );
    is( $psedres, $testcase{$tc}{expect}, "psed $tc" );

    # 2nd test: run s2p
    # translate the sed script to a Perl program

    my $perlprog = runperl( args => $s2pcmd );
    open( PP, ">$plsed" ) || goto FAIL_S2P;
    print PP $perlprog;
    close( PP ) || goto FAIL_S2P;

    # execute generated Perl program, compare
    $s2pres = runperl( args => $plcmd );
    is( $s2pres, $testcase{$tc}{expect}, "s2p $tc" );
    next;

FAIL_BOTH:
    fail( "psed $tc" );
FAIL_S2P:
    fail( "s2p $tc" );
}

END {
    for my $f ( $script, $stdin, $plsed, @aux ){
        1 while unlink( $f ); # hats off to VMS...
    }
}
