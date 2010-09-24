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

print "1..17\n";

$hello = <<EOM ;
hello world
this is a test
EOM

ok(1, $file = IO::Zlib->new($name, "wb"));
ok(2, $file->print($hello));
ok(3, $file->opened());
ok(4, $file->close());
ok(5, !$file->opened());

ok(6, $file = IO::Zlib->new());
ok(7, $file->open($name, "rb"));
ok(8, !$file->eof());
ok(9, $file->read($uncomp, 1024) == length($hello));
ok(10, $uncomp eq $hello);
ok(11, $file->eof());
ok(12, $file->opened());
ok(13, $file->close());
ok(14, !$file->opened());

$file = IO::Zlib->new($name, "rb");
ok(15, $file->read($uncomp, 1024, length($uncomp)) == length($hello));
ok(16, $uncomp eq $hello . $hello);
$file->close();

unlink($name);

ok(17, !defined(IO::Zlib->new($name, "rb")));
