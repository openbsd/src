#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

require './test.pl';
plan(tests => 25);

# compile time

is('-' x 5, '-----',    'compile time x');
is('-' x 1, '-',        '  x 1');
is('-' x 0, '',         '  x 0');

is('ab' x 3, 'ababab',  '  more than one char');

# run time

$a = '-';
is($a x 5, '-----',     'run time x');
is($a x 1, '-',         '  x 1');
is($a x 0, '',          '  x 0');

$a = 'ab';
is($a x 3, 'ababab',    '  more than one char');

$a = 'xyz';
$a x= 2;
is($a, 'xyzxyz',        'x=2');
$a x= 1;
is($a, 'xyzxyz',        'x=1');
$a x= 0;
is($a, '',              'x=0');

@x = (1,2,3);

is(join('', @x x 4),        '3333',                 '@x x Y');
is(join('', (@x) x 4),      '123123123123',         '(@x) x Y');
is(join('', (@x,()) x 4),   '123123123123',         '(@x,()) x Y');
is(join('', (@x,1) x 4),    '1231123112311231',     '(@x,1) x Y');
is(join(':', () x 4),       '',                     '() x Y');
is(join(':', (9) x 4),      '9:9:9:9',              '(X) x Y');
is(join(':', (9,9) x 4),    '9:9:9:9:9:9:9:9',      '(X,X) x Y');
is(join('', (split(//,"123")) x 2), '123123',       'split and x');


# This test is actually testing for Digital C compiler optimizer bug,
# present in Dec C versions 5.* and 6.0 (used in Digital UNIX and VMS),
# found in December 1998.  The bug was reported to Digital^WCompaq as
#     DECC 2745 (21-Dec-1998)
# GEM_BUGS 7619 (23-Dec-1998)
# As of April 1999 the bug has been fixed in Tru64 UNIX 5.0 and is planned
# to be fixed also in 4.0G.
#
# The bug was as follows: broken code was produced for util.c:repeatcpy()
# (a utility function for the 'x' operator) in the case *all* these
# four conditions held:
#
# (1) len == 1
# (2) "from" had the 8th bit on in its single character
# (3) count > 7 (the 'x' count > 16)
# (4) the highest optimization level was used in compilation
#     (which is the default when compiling Perl)
#
# The bug looked like this (. being the eight-bit character and ? being \xff):
#
# 16 ................
# 17 .........???????.
# 18 .........???????..
# 19 .........???????...
# 20 .........???????....
# 21 .........???????.....
# 22 .........???????......
# 23 .........???????.......
# 24 .........???????.???????
# 25 .........???????.???????.
#
# The bug was triggered in the "if (len == 1)" branch.  The fix
# was to introduce a new temporary variable.  In diff -u format:
#
#     register char *frombase = from;
# 
#     if (len == 1) {
#-       todo = *from;
#+       register char c = *from;
#        while (count-- > 0)
#-           *to++ = todo;
#+           *to++ = c;
#        return;
#     }
#
# The bug could also be (obscurely) avoided by changing "from" to
# be an unsigned char pointer.
#
# This obscure bug was not found by the then test suite but instead
# by Mark.Martinec@nsc.ijs.si while trying to install Digest-MD5-2.00.
#
# jhi@iki.fi
#
is("\xdd" x 24, "\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd", 'Dec C bug');


# When we use a list repeat in a scalar context, it behaves like
# a scalar repeat. Make sure that works properly, and doesn't leave
# extraneous values on the stack.
#  -- robin@kitsite.com

my ($x, $y) = scalar ((1,2)x2);
is($x, "22",    'list repeat in scalar context');
is($y, undef,   '  no extra values on stack');

# Make sure the stack doesn't get truncated too much - the left
# operand of the eq binop needs to remain!
is(77, scalar ((1,7)x2),    'stack truncation');


# perlbug 20011113.110 works in 5.6.1, broken in 5.7.2
{
    my $x= [("foo") x 2];
    is( join('', @$x), 'foofoo', 'list repeat in anon array ref broken [ID 20011113.110]' );
}

# [ID 20010809.028] x operator not copying elements in 'for' list?
{
    local $TODO = "x operator not copying elements in 'for' list? [ID 20010809.028]";
    my $x = 'abcd';
    my $y = '';
    for (($x =~ /./g) x 2) {
	$y .= chop;
    }
    is($y, 'abcdabcd');
}

