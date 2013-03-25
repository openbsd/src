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

my @out = (
	"ungWv48Bz+pBQUDeXa4iI7ADYaOWF3qctBD/YfIAFa0",
	"248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1",
);

my $numtests = 8 + scalar @out;
print "1..$numtests\n";

	# attempt to use an invalid algorithm, and check for failure

my $testnum = 1;
my $NSA = "SHA-42";	# No Such Algorithm
print "not " if $MODULE->new($NSA);
print "ok ", $testnum++, "\n";

my $tempfile = "methods.tmp";
END { 1 while unlink $tempfile }

	# test OO methods using first two SHA-256 vectors from NIST

my $fh = FileHandle->new($tempfile, "w");
binmode($fh);
print $fh "bcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
$fh->close;

my $sha = $MODULE->new()->reset("SHA-256")->new();
$sha->add_bits("a", 5)->add_bits("001");

my $rsp = shift(@out);
print "not " unless $sha->clone->add("b", "c")->b64digest eq $rsp;
print "ok ", $testnum++, "\n";

$rsp = shift(@out);

	# test addfile with bareword filehandle

open(FILE, "<$tempfile");
binmode(FILE);
print "not " unless
	$sha->clone->addfile(*FILE)->hexdigest eq $rsp;
print "ok ", $testnum++, "\n";
close(FILE);

	# test addfile with indirect filehandle

$fh = FileHandle->new($tempfile, "r");
binmode($fh);
print "not " unless $sha->clone->addfile($fh)->hexdigest eq $rsp;
print "ok ", $testnum++, "\n";
$fh->close;

	# test addfile using file name instead of handle

print "not " unless $sha->addfile($tempfile, "b")->hexdigest eq $rsp;
print "ok ", $testnum++, "\n";

	# test addfile portable mode

$fh = FileHandle->new($tempfile, "w");
binmode($fh);
print $fh "abc\012" x 2048;		# using UNIX newline
$fh->close;

print "not " unless $sha->new(1)->addfile($tempfile, "p")->hexdigest eq
	"d449e19c1b0b0c191294c8dc9fa2e4a6ff77fc51";
print "ok ", $testnum++, "\n";

$fh = FileHandle->new($tempfile, "w");
binmode($fh);
print $fh "abc\015\012" x 2048;		# using DOS/Windows newline
$fh->close;

print "not " unless $sha->new(1)->addfile($tempfile, "p")->hexdigest eq
	"d449e19c1b0b0c191294c8dc9fa2e4a6ff77fc51";
print "ok ", $testnum++, "\n";

$fh = FileHandle->new($tempfile, "w");
binmode($fh);
print $fh "abc\015" x 2048;		# using early-Mac newline
$fh->close;

print "not " unless $sha->new(1)->addfile($tempfile, "p")->hexdigest eq
	"d449e19c1b0b0c191294c8dc9fa2e4a6ff77fc51";
print "ok ", $testnum++, "\n";

	# test addfile BITS mode

$fh = FileHandle->new($tempfile, "w");
print $fh "0100010";			# using NIST 7-bit test vector
$fh->close;

print "not " unless $sha->new(1)->addfile($tempfile, "0")->hexdigest eq
	"04f31807151181ad0db278a1660526b0aeef64c2";
print "ok ", $testnum++, "\n";

$fh = FileHandle->new($tempfile, "w");
binmode($fh);
print $fh map(chr, (0..127));		# this is actually NIST 2-bit test
$fh->close;				# vector "01" (other chars ignored)

print "not " unless $sha->new(1)->addfile($tempfile, "0")->hexdigest eq
	"ec6b39952e1a3ec3ab3507185cf756181c84bbe2";
print "ok ", $testnum++, "\n";
