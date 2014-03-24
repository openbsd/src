use strict;
use warnings;
use v5.16.0;
use File::Temp 'tempdir';
use File::Spec::Functions;
use Test::More;

BEGIN {
  plan skip_all => "Home-grown glob does not do character classes on $^O" if $^O eq 'VMS';
}

plan tests => 1;

my @md = (1..305);
my @mp = (1000..1205);

my $path = tempdir uc cleanup => 1;

foreach (@md) {
    open(my $f, ">", catfile $path, "md_$_.dat");
    close $f;
}

foreach (@mp) {
    open(my $f, ">", catfile $path, "mp_$_.dat");
    close $f;
}
my @b = glob(qq{$path/mp_[0123456789]*.dat
                $path/md_[0123456789]*.dat});
is scalar(@b), @md+@mp,
    'File::Glob extends the stack when returning a long list';
