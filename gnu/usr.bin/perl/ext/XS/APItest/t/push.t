BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    push @INC, "::lib:$MacPerl::Architecture:" if $^O eq 'MacOS';
    require Config; import Config;
    if ($Config{'extensions'} !~ /\bXS\/APItest\b/) {
        print "1..0 # Skip: XS::APItest was not built\n";
        exit 0;
    }
}

use Test::More tests => 9;

BEGIN { use_ok('XS::APItest') };

#########################

my @mpushp = mpushp();
my @mpushn = mpushn();
my @mpushi = mpushi();
my @mpushu = mpushu();
ok(eq_array(\@mpushp, [qw(one two three)]), 'mPUSHp()');
ok(eq_array(\@mpushn, [0.5, -0.25, 0.125]), 'mPUSHn()');
ok(eq_array(\@mpushi, [-1, 2, -3]),         'mPUSHi()');
ok(eq_array(\@mpushu, [1, 2, 3]),           'mPUSHu()');

my @mxpushp = mxpushp();
my @mxpushn = mxpushn();
my @mxpushi = mxpushi();
my @mxpushu = mxpushu();
ok(eq_array(\@mxpushp, [qw(one two three)]), 'mXPUSHp()');
ok(eq_array(\@mxpushn, [0.5, -0.25, 0.125]), 'mXPUSHn()');
ok(eq_array(\@mxpushi, [-1, 2, -3]),         'mXPUSHi()');
ok(eq_array(\@mxpushu, [1, 2, 3]),           'mXPUSHu()');
