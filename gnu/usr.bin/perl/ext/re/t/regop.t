#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require Config;
    if (($Config::Config{'extensions'} !~ /\bre\b/) ){
	print "1..0 # Skip -- Perl configured without re module\n";
	exit 0;
    }
}

use strict;
BEGIN { require "./test.pl"; }
our $NUM_SECTS;
chomp(my @strs= grep { !/^\s*\#/ } <DATA>);
my $out = runperl(progfile => "../ext/re/t/regop.pl", stderr => 1 );
# VMS currently embeds linefeeds in the output.
$out =~ s/\cJ//g if $^O = 'VMS';
my @tests = grep { /\S/ } split /(?=Compiling REx)/, $out;
# on debug builds we get an EXECUTING... message in there at the top
shift @tests
    if $tests[0] =~ /EXECUTING.../;

plan( @tests + 2 + ( @strs - grep { !$_ or /^---/ } @strs ));

is( scalar @tests, $NUM_SECTS,
    "Expecting output for $NUM_SECTS patterns" );
ok( defined $out, 'regop.pl returned something defined' );

$out ||= "";
my $test= 1;
foreach my $testout ( @tests ) {
    my ( $pattern )= $testout=~/Compiling REx "([^"]+)"/;
    ok( $pattern, "Pattern for test " . ($test++) );
    my $diaged;
    while (@strs) {
        local $_= shift @strs;
        last if !$_
             or /^---/;
        next if /^\s*#/;
        s/^\s+//;
        s/\s+$//;
        ok( $testout=~/\Q$_\E/, "$_: /$pattern/" )
            or do {
                !$diaged++ and diag("$_: /$pattern/\n$testout");
            };
    }
}

# The format below is simple. Each line is an exact
# string that must be found in the output.
# Lines starting the # are comments.
# Lines starting with --- are seperators indicating
# that the tests for this result set are finished.
# If you add a test make sure you update $NUM_SECTS
# the commented output is just for legacy/debugging purposes
BEGIN{ $NUM_SECTS= 6 }

__END__
#Compiling REx "X(A|[B]Q||C|D)Y"
#size 34
#first at 1
#   1: EXACT <X>(3)
#   3: OPEN1(5)
#   5:   TRIE-EXACT(21)
#        [Words:5 Chars:5 Unique:5 States:6 Start-Class:A-D]
#          <A>
#          <BQ>
#          <>
#          <C>
#          <D>
#  21: CLOSE1(23)
#  23: EXACT <Y>(25)
#  25: END(0)
#anchored "X" at 0 floating "Y" at 1..3 (checking floating) minlen 2
#Guessing start of match, REx "X(A|[B]Q||C|D)Y" against "XY"...
#Found floating substr "Y" at offset 1...
#Found anchored substr "X" at offset 0...
#Guessed: match at offset 0
#Matching REx "X(A|[B]Q||C|D)Y" against "XY"
#  Setting an EVAL scope, savestack=140
#   0 <> <XY>              |  1:  EXACT <X>
#   1 <X> <Y>              |  3:  OPEN1
#   1 <X> <Y>              |  5:  TRIE-EXACT
#                                 matched empty string...
#   1 <X> <Y>              | 21:  CLOSE1
#   1 <X> <Y>              | 23:  EXACT <Y>
#   2 <XY> <>              | 25:  END
#Match successful!
#%MATCHED%
#Freeing REx: "X(A|[B]Q||C|D)Y"
Compiling REx "X(A|[B]Q||C|D)Y"
[A-D]
TRIE-EXACT
<BQ>
matched empty string
Match successful!
Found floating substr "Y" at offset 1...
Found anchored substr "X" at offset 0...
Guessed: match at offset 0
checking floating
minlen 2
S:1/6   
W:5
L:0/2
C:5/5
%MATCHED%
---
#Compiling REx "[f][o][o][b][a][r]"
#size 67
#first at 1
#   1: EXACT <foobar>(13)
#  13: END(0)
#anchored "foobar" at 0 (checking anchored isall) minlen 6
#Guessing start of match, REx "[f][o][o][b][a][r]" against "foobar"...
#Found anchored substr "foobar" at offset 0...
#Guessed: match at offset 0
#Freeing REx: "[f][o][o][b][a][r]"
foobar
checking anchored isall
minlen 6
anchored "foobar" at 0
Guessed: match at offset 0
Compiling REx "[f][o][o][b][a][r]"
Freeing REx: "[f][o][o][b][a][r]"
%MATCHED%
---
#Compiling REx ".[XY]."
#size 14
#first at 1
#   1: REG_ANY(2)
#   2: ANYOF[XY](13)
#  13: REG_ANY(14)
#  14: END(0)
#minlen 3
#%FAILED%
#Freeing REx: ".[XY]."
%FAILED%
minlen 3
---
# Compiling REx "(?:ABCP|ABCG|ABCE|ABCB|ABCA|ABCD)"
# Got 164 bytes for offset annotations.
#     TRIE(NATIVE): W:6 C:24 Uq:7 Min:4 Max:4
#       Char : Match Base  Ofs     A   B   C   P   G   E   D
#       State|---------------------------------------------------
#       #   1|       @   7 + 0[    2   .   .   .   .   .   .]
#       #   2|       @   7 + 1[    .   3   .   .   .   .   .]
#       #   3|       @   7 + 2[    .   .   4   .   .   .   .]
#       #   4|       @   A + 0[    9   8   0   5   6   7   A]
#       #   5| W   1 @   0 
#       #   6| W   2 @   0 
#       #   7| W   3 @   0 
#       #   8| W   4 @   0 
#       #   9| W   5 @   0 
#       #   A| W   6 @   0 
# Final program:
#    1: EXACT <ABC>(3)
#    3: TRIEC-EXACT<S:4/10 W:6 L:1/1 C:24/7>[A-EGP](20)
#       <P> 
#       <G> 
#       <E> 
#       <B> 
#       <A> 
#       <D> 
#   20: END(0)
# anchored "ABC" at 0 (checking anchored) minlen 4 
# Offsets: [20]
# 	1:4[3] 3:4[15] 19:32[0] 20:34[0] 
# Guessing start of match in sv for REx "(?:ABCP|ABCG|ABCE|ABCB|ABCA|ABCD)" against "ABCD"
# Found anchored substr "ABC" at offset 0...
# Guessed: match at offset 0
# Matching REx "(?:ABCP|ABCG|ABCE|ABCB|ABCA|ABCD)" against "ABCD"
#    0 <> <ABCD>               |  1:EXACT <ABC>(3)
#    3 <ABC> <D>               |  3:TRIEC-EXACT<S:4/10 W:6 L:1/1 C:24/7>[A-EGP](20)
#    3 <ABC> <D>               |    State:    4 Accepted:    0 Charid:  7 CP:  44 After State:    a
#    4 <ABCD> <>               |    State:    a Accepted:    1 Charid:  6 CP:   0 After State:    0
#                                   got 1 possible matches
#                                   only one match left: #6 <D>
#    4 <ABCD> <>               | 20:END(0)
# Match successful!
# %MATCHED%
# Freeing REx: "(?:ABCP|ABCG|ABCE|ABCB|ABCA|ABCD)"
%MATCHED%
EXACT <ABC>
TRIEC-EXACT
[A-EGP]
only one match left: #6 <D>
S:4/10
W:6
L:1/1
C:24/7
minlen 4
(checking anchored)
anchored "ABC" at 0
---
#Compiling REx "(\.COM|\.EXE|\.BAT|\.CMD|\.VBS|\.VBE|\.JS|\.JSE|\.WSF|\.WSH|\.pyo|\.pyc|\.pyw|\.py)$"
#size 48 nodes first at 3
#first at 3
#rarest char
# at 0
#   1: OPEN1(3)
#   3:   EXACTF <.>(5)
#   5:   TRIE-EXACTF(45)
#        [Start:2 Words:14 Chars:54 Unique:18 States:29 Minlen:2 Maxlen:3 Start-Class:BCEJPVWbcejpvw]
#          <.COM>
#          ...  yada yada ... (dmq)
#          <.py>
#  45: CLOSE1(47)
#  47: EOL(48)
#  48: END(0)
#floating ""$ at 3..4 (checking floating) stclass "EXACTF <.>" minlen 3
#Offsets: [48]
#        1:1[1] 3:2[1] 5:2[81] 45:83[1] 47:84[1] 48:85[0]
#Guessing start of match, REx "(\.COM|\.EXE|\.BAT|\.CMD|\.VBS|\.VBE|\.JS|\.JSE|\.WSF|\.WSH|..." against "D:dev/perl/ver/28321_/perl.exe"...
#Found floating substr ""$ at offset 30...
#Starting position does not contradict /^/m...
#Does not contradict STCLASS...
#Guessed: match at offset 26
#Matching REx "(\.COM|\.EXE|\.BAT|\.CMD|\.VBS|\.VBE|\.JS|\.JSE|\.WSF|\.WSH|\.pyo|\.pyc|\.pyw|\.py)$..." against ".exe"
#Matching stclass "EXACTF <.>" against ".exe"
#  Setting an EVAL scope, savestack=140
#  26 <21_/perl> <.exe>    |  1:  OPEN1
#  26 <21_/perl> <.exe>    |  3:  EXACTF <.>
#  27 <21_/perl.> <exe>    |  5:  TRIE-EXACTF
#                                 only one match : #2 <.EXE>
#  30 <21_/perl.exe> <>    | 45:    CLOSE1
#  30 <21_/perl.exe> <>    | 47:    EOL
#  30 <21_/perl.exe> <>    | 48:    END
#Match successful!
#POP STATE(1)
#%MATCHED%
#Freeing REx: "(\\.COM|\\.EXE|\\.BAT|\\.CMD|\\.VBS|\\.VBE|\\.JS|\\.JSE|\\."......
%MATCHED%
floating ""$ at 3..4 (checking floating)
#1:1[1] 3:2[1] 5:2[64] 45:83[1] 47:84[1] 48:85[0]
#stclass EXACTF <.> minlen 3
#Found floating substr ""$ at offset 30...
#Does not contradict STCLASS...
#Guessed: match at offset 26
#Matching stclass EXACTF <.> against ".exe"
---
#Compiling REx "[q]"
#size 12 nodes Got 100 bytes for offset annotations.
#first at 1
#Final program:
#   1: EXACT <q>(3)
#   3: END(0)
#anchored "q" at 0 (checking anchored isall) minlen 1
#Offsets: [12]
#        1:1[3] 3:4[0]
#Guessing start of match, REx "[q]" against "q"...
#Found anchored substr "q" at offset 0...
#Guessed: match at offset 0
#%MATCHED%
#Freeing REx: "[q]"
Got 100 bytes for offset annotations.
Offsets: [12]
1:1[3] 3:4[0]
%MATCHED%        
Freeing REx: "[q]"