#!./perl -w

BEGIN {
    require 'test.pl';
}

plan tests => 2;

use File::Temp;
use Fcntl qw(:mode);

my $tmpfile = File::Temp->new;
my $mode = (stat "$tmpfile")[2];
ok( S_ISREG($mode), " S_ISREG tmpfile");
ok(!S_ISDIR($mode), "!S_ISDIR tmpfile");
