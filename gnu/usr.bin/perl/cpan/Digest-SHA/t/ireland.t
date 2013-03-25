use strict;
use FileHandle;

my $MODULE;

BEGIN {
	$MODULE = (-d "src") ? "Digest::SHA" : "Digest::SHA::PurePerl";
	eval "require $MODULE" || die $@;
	$MODULE->import(qw());
}

BEGIN {
	if ($ENV{PERL_CORE}) {
		chdir 't' if -d 't';
		@INC = '../lib';
	}
}

# David Ireland's test vector - SHA-256 digest of "a" x 536870912

# Adapted from Julius Duque's original script (t/24-ireland.tmp)
#	- modified to use state cache via dump()/load() methods

print "1..1\n";

my $tempfile = "ireland.tmp";
END { 1 while unlink $tempfile }

my $fh = FileHandle->new($tempfile, "w");
while (<DATA>) { print $fh $_ }  close($fh);

my $rsp = "b9045a713caed5dff3d3b783e98d1ce5778d8bc331ee4119d707072312af06a7";

my $sha;
if ($sha = $MODULE->load($tempfile)) {
	$sha->add("aa");
	print "not " unless $sha->hexdigest eq $rsp;
	print "ok 1\n";
}
else { print "not ok 1\n" }

__DATA__
alg:256
H:dd75eb45:02d4f043:06b41193:6fda751d:73064db9:787d54e1:52dc3fe0:48687dfa
block:61:61:61:61:61:61:61:61:61:61:61:61:61:61:61:61:61:61:61:61:61:61:61:61:61:61:61:61:61:61:61:61:61:61:61:61:61:61:61:61:61:61:61:61:61:61:61:61:61:61:61:61:61:61:61:61:61:61:61:61:61:61:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00
blockcnt:496
lenhh:0
lenhl:0
lenlh:0
lenll:4294967280
