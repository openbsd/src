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
use File::Spec ;
use CompTestUtils;

BEGIN {
    plan(skip_all => "oneshot needs Perl 5.005 or better - you have Perl $]" )
        if $] < 5.005 ;


    # use Test::NoWarnings, if available
    my $extra = 0 ;
    $extra = 1
        if eval { require Test::NoWarnings ;  import Test::NoWarnings; 1 };

    plan tests => 216 + $extra ;

    #use_ok('IO::Compress::Zip', qw(zip $ZipError :zip_method)) ;
    use_ok('IO::Compress::Zip', qw(:all)) ;
    use_ok('IO::Uncompress::Unzip', qw(unzip $UnzipError)) ;
}


sub zipGetHeader
{
    my $in = shift;
    my $content = shift ;
    my %opts = @_ ;

    my $out ;
    my $got ;

    ok zip($in, \$out, %opts), "  zip ok" ;
    ok unzip(\$out, \$got), "  unzip ok" 
        or diag $UnzipError ;
    is $got, $content, "  got expected content" ;

    my $gunz = new IO::Uncompress::Unzip \$out, Strict => 0
        or diag "UnzipError is $IO::Uncompress::Unzip::UnzipError" ;
    ok $gunz, "  Created IO::Uncompress::Unzip object";
    my $hdr = $gunz->getHeaderInfo();
    ok $hdr, "  got Header info";
    my $uncomp ;
    ok $gunz->read($uncomp), " read ok" ;
    is $uncomp, $content, "  got expected content";
    ok $gunz->close, "  closed ok" ;

    return $hdr ;
    
}

{
    title "Check zip header default NAME & MTIME settings" ;

    my $lex = new LexFile my $file1;

    my $content = "hello ";
    my $hdr ;
    my $mtime ;

    writeFile($file1, $content);
    $mtime = (stat($file1))[9];
    # make sure that the zip file isn't created in the same
    # second as the input file
    sleep 3 ; 
    $hdr = zipGetHeader($file1, $content);

    is $hdr->{Name}, $file1, "  Name is '$file1'";
    is $hdr->{Time}>>1, $mtime>>1, "  Time is ok";

    title "Override Name" ;

    writeFile($file1, $content);
    $mtime = (stat($file1))[9];
    sleep 3 ; 
    $hdr = zipGetHeader($file1, $content, Name => "abcde");

    is $hdr->{Name}, "abcde", "  Name is 'abcde'" ;
    is $hdr->{Time} >> 1, $mtime >> 1, "  Time is ok";

    title "Override Time" ;

    writeFile($file1, $content);
    my $useTime = time + 2000 ;
    $hdr = zipGetHeader($file1, $content, Time => $useTime);

    is $hdr->{Name}, $file1, "  Name is '$file1'" ;
    is $hdr->{Time} >> 1 , $useTime >> 1 ,  "  Time is $useTime";

    title "Override Name and Time" ;

    $useTime = time + 5000 ;
    writeFile($file1, $content);
    $hdr = zipGetHeader($file1, $content, Time => $useTime, Name => "abcde");

    is $hdr->{Name}, "abcde", "  Name is 'abcde'" ;
    is $hdr->{Time} >> 1 , $useTime >> 1 , "  Time is $useTime";

    title "Filehandle doesn't have default Name or Time" ;
    my $fh = new IO::File "< $file1"
        or diag "Cannot open '$file1': $!\n" ;
    sleep 3 ; 
    my $before = time ;
    $hdr = zipGetHeader($fh, $content);
    my $after = time ;

    ok ! defined $hdr->{Name}, "  Name is undef";
    cmp_ok $hdr->{Time} >> 1, '>=', $before >> 1, "  Time is ok";
    cmp_ok $hdr->{Time} >> 1, '<=', $after >> 1, "  Time is ok";

    $fh->close;

    title "Buffer doesn't have default Name or Time" ;
    my $buffer = $content;
    $before = time ;
    $hdr = zipGetHeader(\$buffer, $content);
    $after = time ;

    ok ! defined $hdr->{Name}, "  Name is undef";
    cmp_ok $hdr->{Time} >> 1, '>=', $before >> 1, "  Time is ok";
    cmp_ok $hdr->{Time} >> 1, '<=', $after >> 1, "  Time is ok";
}

{
    title "Check CanonicalName & FilterName";

    my $lex = new LexFile my $file1;

    my $content = "hello" ;
    writeFile($file1, $content);
    my $hdr;

    my $abs = File::Spec->catfile("", "fred", "joe");
    $hdr = zipGetHeader($file1, $content, Name => $abs, CanonicalName => 1) ;
    is $hdr->{Name}, "fred/joe", "  Name is 'fred/joe'" ;

    $hdr = zipGetHeader($file1, $content, Name => $abs, CanonicalName => 0) ;
    is $hdr->{Name}, File::Spec->catfile("", "fred", "joe"), "  Name is '/fred/joe'" ;

    $hdr = zipGetHeader($file1, $content, FilterName => sub {$_ = "abcde"});
    is $hdr->{Name}, "abcde", "  Name is 'abcde'" ;

    $hdr = zipGetHeader($file1, $content, Name => $abs, 
         CanonicalName => 1,
         FilterName => sub { s/joe/jim/ });
    is $hdr->{Name}, "fred/jim", "  Name is 'fred/jim'" ;

    $hdr = zipGetHeader($file1, $content, Name => $abs, 
         CanonicalName => 0,
         FilterName => sub { s/joe/jim/ });
    is $hdr->{Name}, File::Spec->catfile("", "fred", "jim"), "  Name is '/fred/jim'" ;
}

for my $stream (0, 1)
{
    for my $zip64 (0, 1)
    {
        #next if $zip64 && ! $stream;

        for my $method (ZIP_CM_STORE, ZIP_CM_DEFLATE)
        {

            title "Stream $stream, Zip64 $zip64, Method $method";

            my $lex = new LexFile my $file1;

            my $content = "hello ";
            #writeFile($file1, $content);

            my $status = zip(\$content => $file1 , 
                               Method => $method, 
                               Stream => $stream,
                               Zip64  => $zip64);

             ok $status, "  zip ok" 
                or diag $ZipError ;

            my $got ;
            ok unzip($file1 => \$got), "  unzip ok"
                or diag $UnzipError ;

            is $got, $content, "  content ok";

            my $u = new IO::Uncompress::Unzip $file1
                or diag $ZipError ;

            my $hdr = $u->getHeaderInfo();
            ok $hdr, "  got header";

            is $hdr->{Stream}, $stream, "  stream is $stream" ;
            is $hdr->{MethodID}, $method, "  MethodID is $method" ;
            is $hdr->{Zip64}, $zip64, "  Zip64 is $zip64" ;
        }
    }
}

for my $stream (0, 1)
{
    for my $zip64 (0, 1)
    {
        next if $zip64 && ! $stream;
        for my $method (ZIP_CM_STORE, ZIP_CM_DEFLATE)
        {
            title "Stream $stream, Zip64 $zip64, Method $method";

            my $file1;
            my $file2;
            my $zipfile;
            my $lex = new LexFile $file1, $file2, $zipfile;

            my $content1 = "hello ";
            writeFile($file1, $content1);

            my $content2 = "goodbye ";
            writeFile($file2, $content2);

            my %content = ( $file1 => $content1,
                            $file2 => $content2,
                          );

            ok zip([$file1, $file2] => $zipfile , Method => $method, 
                                                  Zip64  => $zip64,
                                                  Stream => $stream), " zip ok" 
                or diag $ZipError ;

            for my $file ($file1, $file2)
            {
                my $got ;
                ok unzip($zipfile => \$got, Name => $file), "  unzip $file ok"
                    or diag $UnzipError ;

                is $got, $content{$file}, "  content ok";
            }
        }
    }
}

# TODO add more error cases

