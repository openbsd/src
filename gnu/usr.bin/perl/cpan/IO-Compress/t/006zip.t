BEGIN {
    if ($ENV{PERL_CORE}) {
	chdir 't' if -d 't';
	@INC = ("../lib", "lib/compress");
    }
}

use lib qw(t t/compress);
use strict;
use warnings;
use bytes;

use Test::More ;
use CompTestUtils;

BEGIN {
    # use Test::NoWarnings, if available
    my $extra = 0 ;
    $extra = 1
        if eval { require Test::NoWarnings ;  import Test::NoWarnings; 1 };

    plan tests => 77 + $extra ;

    use_ok('IO::Compress::Zip', qw(:all)) ;
    use_ok('IO::Uncompress::Unzip', qw(unzip $UnzipError)) ;

    eval { 
           require IO::Compress::Bzip2 ; 
           import  IO::Compress::Bzip2 2.010 ; 
           require IO::Uncompress::Bunzip2 ; 
           import  IO::Uncompress::Bunzip2 2.010 ; 
         } ;

}


sub getContent
{
    my $filename = shift;

    my $u = new IO::Uncompress::Unzip $filename, Append => 1
        or die "Cannot open $filename: $UnzipError";

    isa_ok $u, "IO::Uncompress::Unzip";

    my @content;
    my $status ;

    for ($status = 1; ! $u->eof(); $status = $u->nextStream())
    {
        my $name = $u->getHeaderInfo()->{Name};
        #warn "Processing member $name\n" ;

        my $buff = '';
        1 while ($status = $u->read($buff)) ;

        push @content, $buff;
        last unless $status == 0;
    }

    die "Error processing $filename: $status $!\n"
        if $status < 0 ;    

    return @content;
}


{
    title "Create a simple zip - All Deflate";

    my $lex = new LexFile my $file1;

    my @content = (
                   'hello',
                   '',
                   'goodbye ',
                   );

    my $zip = new IO::Compress::Zip $file1,
                    Name => "one", Method => ZIP_CM_DEFLATE, Stream => 0;
    isa_ok $zip, "IO::Compress::Zip";

    is $zip->write($content[0]), length($content[0]), "write"; 
    $zip->newStream(Name=> "two", Method => ZIP_CM_DEFLATE);
    is $zip->write($content[1]), length($content[1]), "write"; 
    $zip->newStream(Name=> "three", Method => ZIP_CM_DEFLATE);
    is $zip->write($content[2]), length($content[2]), "write"; 
    ok $zip->close(), "closed";                    

    my @got = getContent($file1);

    is $got[0], $content[0], "Got 1st entry";
    is $got[1], $content[1], "Got 2nd entry";
    is $got[2], $content[2], "Got 3nd entry";
}

SKIP:
{
    title "Create a simple zip - All Bzip2";

    skip "IO::Compress::Bzip2 not available", 9
        unless defined $IO::Compress::Bzip2::VERSION;

    my $lex = new LexFile my $file1;

    my @content = (
                   'hello',
                   '',
                   'goodbye ',
                   );

    my $zip = new IO::Compress::Zip $file1,
                    Name => "one", Method => ZIP_CM_BZIP2, Stream => 0;
    isa_ok $zip, "IO::Compress::Zip";

    is $zip->write($content[0]), length($content[0]), "write"; 
    $zip->newStream(Name=> "two", Method => ZIP_CM_BZIP2);
    is $zip->write($content[1]), length($content[1]), "write"; 
    $zip->newStream(Name=> "three", Method => ZIP_CM_BZIP2);
    is $zip->write($content[2]), length($content[2]), "write"; 
    ok $zip->close(), "closed";                    

    my @got = getContent($file1);

    is $got[0], $content[0], "Got 1st entry";
    is $got[1], $content[1], "Got 2nd entry";
    is $got[2], $content[2], "Got 3nd entry";
}

SKIP:
{
    title "Create a simple zip - Deflate + Bzip2";

    skip "IO::Compress::Bzip2 not available", 9
        unless $IO::Compress::Bzip2::VERSION;

    my $lex = new LexFile my $file1;

    my @content = (
                   'hello',
                   'and',
                   'goodbye ',
                   );

    my $zip = new IO::Compress::Zip $file1,
                    Name => "one", Method => ZIP_CM_DEFLATE, Stream => 0;
    isa_ok $zip, "IO::Compress::Zip";

    is $zip->write($content[0]), length($content[0]), "write"; 
    $zip->newStream(Name=> "two", Method => ZIP_CM_BZIP2);
    is $zip->write($content[1]), length($content[1]), "write"; 
    $zip->newStream(Name=> "three", Method => ZIP_CM_DEFLATE);
    is $zip->write($content[2]), length($content[2]), "write"; 
    ok $zip->close(), "closed";                    

    my @got = getContent($file1);

    is $got[0], $content[0], "Got 1st entry";
    is $got[1], $content[1], "Got 2nd entry";
    is $got[2], $content[2], "Got 3nd entry";
}

{
    title "Create a simple zip - All STORE";

    my $lex = new LexFile my $file1;

    my @content = (
                   'hello',
                   '',
                   'goodbye ',
                   );

    my $zip = new IO::Compress::Zip $file1,
                    Name => "one", Method => ZIP_CM_STORE, Stream => 0;
    isa_ok $zip, "IO::Compress::Zip";

    is $zip->write($content[0]), length($content[0]), "write"; 
    $zip->newStream(Name=> "two", Method => ZIP_CM_STORE);
    is $zip->write($content[1]), length($content[1]), "write"; 
    $zip->newStream(Name=> "three", Method => ZIP_CM_STORE);
    is $zip->write($content[2]), length($content[2]), "write"; 
    ok $zip->close(), "closed";                    

    my @got = getContent($file1);

    is $got[0], $content[0], "Got 1st entry";
    is $got[1], $content[1], "Got 2nd entry";
    is $got[2], $content[2], "Got 3nd entry";
}

{
    title "Create a simple zip - Deflate + STORE";

    my $lex = new LexFile my $file1;

    my @content = qw(
                   hello 
                       and
                   goodbye 
                   );

    my $zip = new IO::Compress::Zip $file1,
                    Name => "one", Method => ZIP_CM_DEFLATE, Stream => 0;
    isa_ok $zip, "IO::Compress::Zip";

    is $zip->write($content[0]), length($content[0]), "write"; 
    $zip->newStream(Name=> "two", Method => ZIP_CM_STORE);
    is $zip->write($content[1]), length($content[1]), "write"; 
    $zip->newStream(Name=> "three", Method => ZIP_CM_DEFLATE);
    is $zip->write($content[2]), length($content[2]), "write"; 
    ok $zip->close(), "closed";                    

    my @got = getContent($file1);

    is $got[0], $content[0], "Got 1st entry";
    is $got[1], $content[1], "Got 2nd entry";
    is $got[2], $content[2], "Got 3nd entry";
}

{
    title "Create a simple zip - Deflate + zero length STORE";

    my $lex = new LexFile my $file1;

    my @content = (
                   'hello ',
                   '',
                   'goodbye ',
                   );

    my $zip = new IO::Compress::Zip $file1,
                    Name => "one", Method => ZIP_CM_DEFLATE, Stream => 0;
    isa_ok $zip, "IO::Compress::Zip";

    is $zip->write($content[0]), length($content[0]), "write"; 
    $zip->newStream(Name=> "two", Method => ZIP_CM_STORE);
    is $zip->write($content[1]), length($content[1]), "write"; 
    $zip->newStream(Name=> "three", Method => ZIP_CM_DEFLATE);
    is $zip->write($content[2]), length($content[2]), "write"; 
    ok $zip->close(), "closed";                    

    my @got = getContent($file1);

    is $got[0], $content[0], "Got 1st entry";
    ok $got[1] eq $content[1], "Got 2nd entry";
    is $got[2], $content[2], "Got 3nd entry";
}


SKIP:
for my $method (ZIP_CM_DEFLATE, ZIP_CM_STORE, ZIP_CM_BZIP2)
{
    title "Read a line from zip, Method $method";

    skip "IO::Compress::Bzip2 not available", 14
        unless defined $IO::Compress::Bzip2::VERSION;

    my $content = "a single line\n";
    my $zip ;

    my $status = zip \$content => \$zip, 
                    Method => $method, 
                    Stream => 0, 
                    Name => "123";
    is $status, 1, "  Created a zip file";

    my $u = new IO::Uncompress::Unzip \$zip;
    isa_ok $u, "IO::Uncompress::Unzip";

    is $u->getline, $content, "  Read first line ok";
    ok ! $u->getline, "  Second line doesn't exist";


}
