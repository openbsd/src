BEGIN {
    if( $ENV{PERL_CORE} ) {
	@INC = '../lib';
	chdir 't';
    }
}

use IO::Zlib;

sub ok
{
    my ($no, $ok) = @_ ;

    #++ $total ;
    #++ $totalBad unless $ok ;

    print "ok $no\n" if $ok ;
    print "not ok $no\n" unless $ok ;
}

$name="test.gz";

print "1..10\n";

$text = "abcd";

ok(1, $file = IO::Zlib->new($name, "wb"));
ok(2, $file->print($text));
ok(3, $file->close());

ok(4, $file = IO::Zlib->new($name, "rb"));
ok(5, $file->getc() eq substr($text,0,1));
ok(6, $file->getc() eq substr($text,1,1));
ok(7, $file->getc() eq substr($text,2,1));
ok(8, $file->getc() eq substr($text,3,1));
ok(9, $file->getc() eq "");
ok(10, $file->close());

unlink($name);
