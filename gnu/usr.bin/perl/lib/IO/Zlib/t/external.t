BEGIN {
    if( $ENV{PERL_CORE} ) {
	@INC = '../lib';
	chdir 't';
    }
}

# Test this only iff we have an executable /usr/bin/gzip
# AND we have /usr/bin in our PATH
# AND we have a useable /usr/bin directory.
# This limits the testing to UNIX-like
# systems but that should be enough.

my $gzip = "/usr/bin/gzip";

unless( -x $gzip &&
        ":$ENV{PATH}:" =~ m!:/usr/bin:! &&
        -d "/usr/bin" && -x "/usr/bin") {
    print "1..0 # Skip: no $gzip\n";
    exit 0;
}

sub ok
{
    my ($no, $ok) = @_ ;
    print "ok $no\n" if $ok ;
    print "not ok $no\n" unless $ok ;
}

my $hasCompressZlib;

BEGIN {
    eval { require Compress::Zlib };
    $hasCompressZlib = $@ ? 0 : 1;
}

use IO::Zlib;

print "1..33\n";

# Other export functionality (none) is tested in import.t.

ok(1,
   $hasCompressZlib == IO::Zlib::has_Compress_Zlib());

eval "use IO::Zlib qw(:gzip_external)";
ok(2,
   $@ =~ /^IO::Zlib::import: ':gzip_external' requires an argument /);

eval "use IO::Zlib";
ok(3, !$@);

ok(4,
   $hasCompressZlib || IO::Zlib::gzip_used());

ok(5,
   !defined IO::Zlib::gzip_external());

ok(6,
   IO::Zlib::gzip_read_open() eq 'gzip -dc %s |');

ok(7,
   IO::Zlib::gzip_write_open() eq '| gzip > %s');

ok(8,
   ($hasCompressZlib && \&IO::Zlib::gzopen == \&Compress::Zlib::gzopen) ||
   \&IO::Zlib::gzopen == \&IO::Zlib::gzopen_external);

eval "use IO::Zlib qw(:gzip_external 0)";

ok(9,
   !IO::Zlib::gzip_external());

ok(10,
   ($hasCompressZlib && \&IO::Zlib::gzopen == \&Compress::Zlib::gzopen) ||
   (!$hasCompressZlib &&
    $@ =~ /^IO::Zlib::import: no Compress::Zlib and no external gzip /));

eval "use IO::Zlib qw(:gzip_external 1)";

ok(11,
   IO::Zlib::gzip_used());

ok(12,
   IO::Zlib::gzip_external());

ok(13,
   \&IO::Zlib::gzopen == \&IO::Zlib::gzopen_external);

eval 'IO::Zlib->new("foo", "xyz")';
ok(14, $@ =~ /^IO::Zlib::gzopen_external: mode 'xyz' is illegal /);

# The following is a copy of the basic.t, shifted up by 14 tests,
# the difference being that now we should be using the external gzip.

$name="test.gz";

$hello = <<EOM ;
hello world
this is a test
EOM

ok(15, $file = IO::Zlib->new($name, "wb"));
ok(16, $file->print($hello));
ok(17, $file->opened());
ok(18, $file->close());
ok(19, !$file->opened());

ok(20, $file = IO::Zlib->new());
ok(21, $file->open($name, "rb"));
ok(22, !$file->eof());
ok(23, $file->read($uncomp, 1024) == length($hello));
ok(24, $file->eof());
ok(25, $file->opened());
ok(26, $file->close());
ok(27, !$file->opened());

unlink($name);

ok(28, $hello eq $uncomp);

ok(29, !defined(IO::Zlib->new($name, "rb")));

# Then finally test modifying the open commands.

my $new_read = 'gzip.exe /d /c %s |';

eval "use IO::Zlib ':gzip_read_open' => '$new_read'";

ok(30,
   IO::Zlib::gzip_read_open() eq $new_read);

eval "use IO::Zlib ':gzip_read_open' => 'bad'";

ok(31,
   $@ =~ /^IO::Zlib::import: ':gzip_read_open' 'bad' is illegal /);

my $new_write = '| gzip.exe %s';

eval "use IO::Zlib ':gzip_write_open' => '$new_write'";

ok(32,
   IO::Zlib::gzip_write_open() eq $new_write);

eval "use IO::Zlib ':gzip_write_open' => 'bad'";

ok(33,
   $@ =~ /^IO::Zlib::import: ':gzip_write_open' 'bad' is illegal /);

