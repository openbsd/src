print "1..3\n";

use Digest;

{
    package Digest::Dummy;
    use vars qw($VERSION @ISA);
    $VERSION = 1;

    require Digest::base;
    @ISA = qw(Digest::base);

    sub new {
	my $class = shift;
	my $d = shift || "ooo";
	bless { d => $d }, $class;
    }
    sub add {}
    sub digest { shift->{d} }
}

my $d;
$d = Digest->new("Dummy");
print "not " unless $d->digest eq "ooo";
print "ok 1\n";

$d = Digest->Dummy;
print "not " unless $d->digest eq "ooo";
print "ok 2\n";

$Digest::MMAP{"Dummy-24"} = [["NotThere"], "NotThereEither", ["Digest::Dummy", 24]];
$d = Digest->new("Dummy-24");
print "not " unless $d->digest eq "24";
print "ok 3\n";


