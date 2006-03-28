#!perl -w

use Test qw(plan ok);
plan tests => 5;

{
   package Digest::Foo;
   require Digest::base;
   use vars qw(@ISA $VERSION);
   @ISA = qw(Digest::base);

   sub new {
	my $class = shift;
	my $str = "";
	bless \$str, $class;
   }

   sub add {
	my $self = shift;
	$$self .= join("", @_);
	return $self;
   }

   sub digest {
	my $self = shift;
	return sprintf "%04d", length($$self);
   }
}

use Digest::file qw(digest_file digest_file_hex digest_file_base64);

my $file = "test-$$";
die if -f $file;
open(F, ">$file") || die "Can't create '$file': $!";
binmode(F);
print F "foo\0\n";
close(F) || die "Can't write '$file': $!";

ok(digest_file($file, "Foo"), "0005");
ok(digest_file_hex($file, "Foo"), "30303035");
ok(digest_file_base64($file, "Foo"), "MDAwNQ");

unlink($file) || warn "Can't unlink '$file': $!";

ok(eval { digest_file("not-there.txt", "Foo") }, undef);
ok($@);
