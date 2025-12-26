use strict;
use warnings;
use Test::More;

use File::Spec;
use FindBin '$Bin';
use Archive::Tar;

# File names
my $tartest = File::Spec->catfile("t", "ptar");
my $foo = File::Spec->catfile("t", "ptar", "foo");
my $tarfile = File::Spec->catfile("t", "ptar.tar");
my $ptar = File::Spec->catfile($Bin, "..", "bin", "ptar");
my $cmd = "$^X $ptar";

plan tests => 11;
my $out;

# Create directory/files
mkdir $tartest;
open my $fh, ">", $foo or die $!;
print $fh "file foo\n";
close $fh;

# Create archive, dashless options
$out = qx{$cmd cvf $tarfile $foo};
is($?, 0, "create ok");
cmp_ok($out, '=~', qr{foo}, "added foo to archive");

# List contents, -f option first
$out = qx{$cmd -f $tarfile -t};
is($?, 0, "list ok");
cmp_ok($out, '=~', qr{foo}, "foo is in archive");

# Extract contents, no space after -f option
unlink $foo or die $!;
$out = qx{$cmd -x -f$tarfile};
is($?, 0, "extract ok");
is($out, "", "extract silent");
ok(-e $foo, "extracted foo from archive");

# Create archive with --format=ustar, bundled options
$out = qx{$cmd --format=ustar -cf $tarfile $foo};
is($?, 0, "--format=ustar ignored ok");
is($out, "", "--format=ustar ignored silently");

# Create archive with --format ustar
$out = qx{$cmd -c -f $tarfile --format ustar $foo};
is($?, 0, "--format ustar ignored ok");
is($out, "", "--format ustar ignored silently");

# Cleanup
END {
    unlink $tarfile or die $!;
    unlink $foo or die $!;
    rmdir $tartest or die $!;
}
