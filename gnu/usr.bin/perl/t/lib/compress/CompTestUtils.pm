package CompTestUtils;

package main ;

use strict ;
use warnings;
use bytes;

#use lib qw(t t/compress);

use Carp ;
#use Test::More ; 



sub title
{
    #diag "" ; 
    ok 1, $_[0] ;
    #diag "" ;
}

sub like_eval
{
    like $@, @_ ;
}

{
    package LexFile ;

    our ($index);
    $index = '00000';
    
    sub new
    {
        my $self = shift ;
        foreach (@_)
        {
            # autogenerate the name unless if none supplied
            $_ = "tst" . $index ++ . ".tmp"
                unless defined $_;
        }
        chmod 0777, @_;
        for (@_) { 1 while unlink $_ } ;
        bless [ @_ ], $self ;
    }

    sub DESTROY
    {
        my $self = shift ;
        chmod 0777, @{ $self } ;
        for (@$self) { 1 while unlink $_ } ;
    }

}

{
    package LexDir ;

    use File::Path;
    sub new
    {
        my $self = shift ;
        foreach (@_) { rmtree $_ }
        bless [ @_ ], $self ;
    }

    sub DESTROY
    {
        my $self = shift ;
        foreach (@$self) { rmtree $_ }
    }
}
sub readFile
{
    my $f = shift ;

    my @strings ;

    if (IO::Compress::Base::Common::isaFilehandle($f))
    {
        my $pos = tell($f);
        seek($f, 0,0);
        @strings = <$f> ;	
        seek($f, 0, $pos);
    }
    else
    {
        open (F, "<$f") 
            or croak "Cannot open $f: $!\n" ;
        binmode F;
        @strings = <F> ;	
        close F ;
    }

    return @strings if wantarray ;
    return join "", @strings ;
}

sub touch
{
    foreach (@_) { writeFile($_, '') }
}

sub writeFile
{
    my($filename, @strings) = @_ ;
    1 while unlink $filename ;
    open (F, ">$filename") 
        or croak "Cannot open $filename: $!\n" ;
    binmode F;
    foreach (@strings) {
        no warnings ;
        print F $_ ;
    }
    close F ;
}

sub GZreadFile
{
    my ($filename) = shift ;

    my ($uncomp) = "" ;
    my $line = "" ;
    my $fil = gzopen($filename, "rb") 
        or croak "Cannopt open '$filename': $Compress::Zlib::gzerrno" ;

    $uncomp .= $line 
        while $fil->gzread($line) > 0;

    $fil->gzclose ;
    return $uncomp ;
}

sub hexDump
{
    my $d = shift ;

    if (IO::Compress::Base::Common::isaFilehandle($d))
    {
        $d = readFile($d);
    }
    elsif (IO::Compress::Base::Common::isaFilename($d))
    {
        $d = readFile($d);
    }
    else
    {
        $d = $$d ;
    }

    my $offset = 0 ;

    $d = '' unless defined $d ;
    #while (read(STDIN, $data, 16)) {
    while (my $data = substr($d, 0, 16)) {
        substr($d, 0, 16) = '' ;
        printf "# %8.8lx    ", $offset;
        $offset += 16;

        my @array = unpack('C*', $data);
        foreach (@array) {
            printf('%2.2x ', $_);
        }
        print "   " x (16 - @array)
            if @array < 16 ;
        $data =~ tr/\0-\37\177-\377/./;
        print "  $data\n";
    }

}

sub readHeaderInfo
{
    my $name = shift ;
    my %opts = @_ ;

    my $string = <<EOM;
some text
EOM

    ok my $x = new IO::Compress::Gzip $name, %opts 
        or diag "GzipError is $IO::Compress::Gzip::GzipError" ;
    ok $x->write($string) ;
    ok $x->close ;

    #is GZreadFile($name), $string ;

    ok my $gunz = new IO::Uncompress::Gunzip $name, Strict => 0
        or diag "GunzipError is $IO::Uncompress::Gunzip::GunzipError" ;
    ok my $hdr = $gunz->getHeaderInfo();
    my $uncomp ;
    ok $gunz->read($uncomp) ;
    ok $uncomp eq $string;
    ok $gunz->close ;

    return $hdr ;
}

sub cmpFile
{
    my ($filename, $uue) = @_ ;
    return readFile($filename) eq unpack("u", $uue) ;
}

sub isRawFormat
{
    my $class = shift;
    my %raw = map { $_ => 1 } qw( RawDeflate );

    return defined $raw{$class};
}

sub uncompressBuffer
{
    my $compWith = shift ;
    my $buffer = shift ;

    my %mapping = ( 'IO::Compress::Gzip'                     => 'IO::Uncompress::Gunzip',
                    'IO::Compress::Gzip::gzip'               => 'IO::Uncompress::Gunzip',
                    'IO::Compress::Deflate'                  => 'IO::Uncompress::Inflate',
                    'IO::Compress::Deflate::deflate'         => 'IO::Uncompress::Inflate',
                    'IO::Compress::RawDeflate'               => 'IO::Uncompress::RawInflate',
                    'IO::Compress::RawDeflate::rawdeflate'   => 'IO::Uncompress::RawInflate',
                    'IO::Compress::Bzip2'                    => 'IO::Uncompress::Bunzip2',
                    'IO::Compress::Bzip2::bzip2'             => 'IO::Uncompress::Bunzip2',
                    'IO::Compress::Zip'                      => 'IO::Uncompress::Unzip',
                    'IO::Compress::Zip::zip'                 => 'IO::Uncompress::Unzip',
                    'IO::Compress::Lzop'                     => 'IO::Uncompress::UnLzop',
                    'IO::Compress::Lzop::lzop'               => 'IO::Uncompress::UnLzop',
                    'IO::Compress::Lzf'                      => 'IO::Uncompress::UnLzf' ,
                    'IO::Compress::Lzf::lzf'                 => 'IO::Uncompress::UnLzf',
                    'IO::Compress::PPMd'                     => 'IO::Uncompress::UnPPMd' ,
                    'IO::Compress::PPMd::ppmd'               => 'IO::Uncompress::UnPPMd',
                    'IO::Compress::DummyComp'                => 'IO::Uncompress::DummyUncomp',
                    'IO::Compress::DummyComp::dummycomp'     => 'IO::Uncompress::DummyUncomp',
                );

    my $out ;
    my $obj = $mapping{$compWith}->new( \$buffer, -Append => 1);
    1 while $obj->read($out) > 0 ;
    return $out ;

}

my %ErrorMap = (    'IO::Compress::Gzip'                => \$IO::Compress::Gzip::GzipError,
                    'IO::Compress::Gzip::gzip'          => \$IO::Compress::Gzip::GzipError,
                    'IO::Uncompress::Gunzip'            => \$IO::Uncompress::Gunzip::GunzipError,
                    'IO::Uncompress::Gunzip::gunzip'    => \$IO::Uncompress::Gunzip::GunzipError,
                    'IO::Uncompress::Inflate'           => \$IO::Uncompress::Inflate::InflateError,
                    'IO::Uncompress::Inflate::inflate'  => \$IO::Uncompress::Inflate::InflateError,
                    'IO::Compress::Deflate'             => \$IO::Compress::Deflate::DeflateError,
                    'IO::Compress::Deflate::deflate'    => \$IO::Compress::Deflate::DeflateError,
                    'IO::Uncompress::RawInflate'        => \$IO::Uncompress::RawInflate::RawInflateError,
                    'IO::Uncompress::RawInflate::rawinflate'  => \$IO::Uncompress::RawInflate::RawInflateError,
                    'IO::Uncompress::AnyInflate'        => \$IO::Uncompress::AnyInflate::AnyInflateError,
                    'IO::Uncompress::AnyInflate::anyinflate'  => \$IO::Uncompress::AnyInflate::AnyInflateError,
                    'IO::Uncompress::AnyUncompress'        => \$IO::Uncompress::AnyUncompress::AnyUncompressError,
                    'IO::Uncompress::AnyUncompress::anyUncompress'  => \$IO::Uncompress::AnyUncompress::AnyUncompressError,
                    'IO::Compress::RawDeflate'          => \$IO::Compress::RawDeflate::RawDeflateError,
                    'IO::Compress::RawDeflate::rawdeflate'  => \$IO::Compress::RawDeflate::RawDeflateError,
                    'IO::Compress::Bzip2'               => \$IO::Compress::Bzip2::Bzip2Error,
                    'IO::Compress::Bzip2::bzip2'        => \$IO::Compress::Bzip2::Bzip2Error,
                    'IO::Uncompress::Bunzip2'           => \$IO::Uncompress::Bunzip2::Bunzip2Error,
                    'IO::Uncompress::Bunzip2::bunzip2'  => \$IO::Uncompress::Bunzip2::Bunzip2Error,
                    'IO::Compress::Zip'                 => \$IO::Compress::Zip::ZipError,
                    'IO::Compress::Zip::zip'            => \$IO::Compress::Zip::ZipError,
                    'IO::Uncompress::Unzip'             => \$IO::Uncompress::Unzip::UnzipError,
                    'IO::Uncompress::Unzip::unzip'      => \$IO::Uncompress::Unzip::UnzipError,
                    'IO::Compress::Lzop'                => \$IO::Compress::Lzop::LzopError,
                    'IO::Compress::Lzop::lzop'          => \$IO::Compress::Lzop::LzopError,
                    'IO::Uncompress::UnLzop'            => \$IO::Uncompress::UnLzop::UnLzopError,
                    'IO::Uncompress::UnLzop::unlzop'    => \$IO::Uncompress::UnLzop::UnLzopError,
                    'IO::Compress::Lzf'                 => \$IO::Compress::Lzf::LzfError,
                    'IO::Compress::Lzf::lzf'            => \$IO::Compress::Lzf::LzfError,
                    'IO::Uncompress::UnLzf'             => \$IO::Uncompress::UnLzf::UnLzfError,
                    'IO::Uncompress::UnLzf::unlzf'      => \$IO::Uncompress::UnLzf::UnLzfError,
                    'IO::Compress::PPMd'                 => \$IO::Compress::PPMd::PPMdError,
                    'IO::Compress::PPMd::ppmd'            => \$IO::Compress::PPMd::PPMdError,
                    'IO::Uncompress::UnPPMd'             => \$IO::Uncompress::UnPPMd::UnPPMdError,
                    'IO::Uncompress::UnPPMd::unppmd'      => \$IO::Uncompress::UnPPMd::UnPPMdError,

                    'IO::Compress::DummyComp'           => \$IO::Compress::DummyComp::DummyCompError,
                    'IO::Compress::DummyComp::dummycomp'=> \$IO::Compress::DummyComp::DummyCompError,
                    'IO::Uncompress::DummyUncomp'       => \$IO::Uncompress::DummyUncomp::DummyUncompError,
                    'IO::Uncompress::DummyUncomp::dummyuncomp' => \$IO::Uncompress::DummyUncomp::DummyUncompError,
               );

my %TopFuncMap = (  'IO::Compress::Gzip'          => 'IO::Compress::Gzip::gzip',
                    'IO::Uncompress::Gunzip'      => 'IO::Uncompress::Gunzip::gunzip',

                    'IO::Compress::Deflate'       => 'IO::Compress::Deflate::deflate',
                    'IO::Uncompress::Inflate'     => 'IO::Uncompress::Inflate::inflate',

                    'IO::Compress::RawDeflate'    => 'IO::Compress::RawDeflate::rawdeflate',
                    'IO::Uncompress::RawInflate'  => 'IO::Uncompress::RawInflate::rawinflate',

                    'IO::Uncompress::AnyInflate'  => 'IO::Uncompress::AnyInflate::anyinflate',
                    'IO::Uncompress::AnyUncompress'  => 'IO::Uncompress::AnyUncompress::anyuncompress',

                    'IO::Compress::Bzip2'         => 'IO::Compress::Bzip2::bzip2',
                    'IO::Uncompress::Bunzip2'     => 'IO::Uncompress::Bunzip2::bunzip2',

                    'IO::Compress::Zip'           => 'IO::Compress::Zip::zip',
                    'IO::Uncompress::Unzip'       => 'IO::Uncompress::Unzip::unzip',
                    'IO::Compress::Lzop'          => 'IO::Compress::Lzop::lzop',
                    'IO::Uncompress::UnLzop'      => 'IO::Uncompress::UnLzop::unlzop',
                    'IO::Compress::Lzf'           => 'IO::Compress::Lzf::lzf',
                    'IO::Uncompress::UnLzf'       => 'IO::Uncompress::UnLzf::unlzf',
                    'IO::Compress::PPMd'           => 'IO::Compress::PPMd::ppmd',
                    'IO::Uncompress::UnPPMd'       => 'IO::Uncompress::UnPPMd::unppmd',
                    'IO::Compress::DummyComp'     => 'IO::Compress::DummyComp::dummyuncomp',
                    'IO::Uncompress::DummyUncomp' => 'IO::Uncompress::DummyUncomp::dummyuncomp',
                 );

   %TopFuncMap = map { ($_              => $TopFuncMap{$_}, 
                        $TopFuncMap{$_} => $TopFuncMap{$_}) } 
                 keys %TopFuncMap ;

 #%TopFuncMap = map { ($_              => \&{ $TopFuncMap{$_} ) } 
                 #keys %TopFuncMap ;


my %inverse  = ( 'IO::Compress::Gzip'                    => 'IO::Uncompress::Gunzip',
                 'IO::Compress::Gzip::gzip'              => 'IO::Uncompress::Gunzip::gunzip',
                 'IO::Compress::Deflate'                 => 'IO::Uncompress::Inflate',
                 'IO::Compress::Deflate::deflate'        => 'IO::Uncompress::Inflate::inflate',
                 'IO::Compress::RawDeflate'              => 'IO::Uncompress::RawInflate',
                 'IO::Compress::RawDeflate::rawdeflate'  => 'IO::Uncompress::RawInflate::rawinflate',
                 'IO::Compress::Bzip2::bzip2'            => 'IO::Uncompress::Bunzip2::bunzip2',
                 'IO::Compress::Bzip2'                   => 'IO::Uncompress::Bunzip2',
                 'IO::Compress::Zip::zip'                => 'IO::Uncompress::Unzip::unzip',
                 'IO::Compress::Zip'                     => 'IO::Uncompress::Unzip',
                 'IO::Compress::Lzop::lzop'              => 'IO::Uncompress::UnLzop::unlzop',
                 'IO::Compress::Lzop'                    => 'IO::Uncompress::UnLzop',
                 'IO::Compress::Lzf::lzf'                => 'IO::Uncompress::UnLzf::unlzf',
                 'IO::Compress::Lzf'                     => 'IO::Uncompress::UnLzf',
                 'IO::Compress::PPMd::ppmd'              => 'IO::Uncompress::UnPPMd::unppmd',
                 'IO::Compress::PPMd'                    => 'IO::Uncompress::UnPPMd',
                 'IO::Compress::DummyComp::dummycomp'    => 'IO::Uncompress::DummyUncomp::dummyuncomp',
                 'IO::Compress::DummyComp'               => 'IO::Uncompress::DummyUncomp',
             );

%inverse  = map { ($_ => $inverse{$_}, $inverse{$_} => $_) } keys %inverse;

sub getInverse
{
    my $class = shift ;

    return $inverse{$class} ;
}

sub getErrorRef
{
    my $class = shift ;

    return $ErrorMap{$class} ;
}

sub getTopFuncRef
{
    my $class = shift ;

    return \&{ $TopFuncMap{$class} } ;
}

sub getTopFuncName
{
    my $class = shift ;

    return $TopFuncMap{$class}  ;
}

sub compressBuffer
{
    my $compWith = shift ;
    my $buffer = shift ;

    my %mapping = ( 'IO::Uncompress::Gunzip'                  => 'IO::Compress::Gzip',
                    'IO::Uncompress::Gunzip::gunzip'          => 'IO::Compress::Gzip',
                    'IO::Uncompress::Inflate'                 => 'IO::Compress::Deflate',
                    'IO::Uncompress::Inflate::inflate'        => 'IO::Compress::Deflate',
                    'IO::Uncompress::RawInflate'              => 'IO::Compress::RawDeflate',
                    'IO::Uncompress::RawInflate::rawinflate'  => 'IO::Compress::RawDeflate',
                    'IO::Uncompress::Bunzip2'                 => 'IO::Compress::Bzip2',
                    'IO::Uncompress::Bunzip2::bunzip2'        => 'IO::Compress::Bzip2',
                    'IO::Uncompress::Unzip'                   => 'IO::Compress::Zip',
                    'IO::Uncompress::Unzip::unzip'            => 'IO::Compress::Zip',
                    'IO::Uncompress::UnLzop'                  => 'IO::Compress::Lzop',
                    'IO::Uncompress::UnLzop::unlzop'          => 'IO::Compress::Lzop',
                    'IO::Uncompress::UnLzp'                   => 'IO::Compress::Lzf',
                    'IO::Uncompress::UnLzf::unlzf'            => 'IO::Compress::Lzf',
                    'IO::Uncompress::UnPPMd'                  => 'IO::Compress::PPMd',
                    'IO::Uncompress::UnPPMd::unppmd'          => 'IO::Compress::PPMd',
                    'IO::Uncompress::AnyInflate'              => 'IO::Compress::Gzip',
                    'IO::Uncompress::AnyInflate::anyinflate'  => 'IO::Compress::Gzip',
                    'IO::Uncompress::AnyUncompress'           => 'IO::Compress::Gzip',
                    'IO::Uncompress::AnyUncompress::anyuncompress'  => 'IO::Compress::Gzip',
                    'IO::Uncompress::DummyUncomp'             => 'IO::Compress::DummyComp',
                    'IO::Uncompress::DummyUncomp::dummyuncomp'=> 'IO::Compress::DummyComp',
                );

    my $out ;
    my $obj = $mapping{$compWith}->new( \$out);
    $obj->write($buffer) ;
    $obj->close();
    return $out ;
}

our ($AnyUncompressError);
BEGIN
{
    eval ' use IO::Uncompress::AnyUncompress qw($AnyUncompressError); ';
}

sub anyUncompress
{
    my $buffer = shift ;
    my $already = shift;

    my @opts = ();
    if (ref $buffer && ref $buffer eq 'ARRAY')
    {
        @opts = @$buffer;
        $buffer = shift @opts;
    }

    if (ref $buffer)
    {
        croak "buffer is undef" unless defined $$buffer;
        croak "buffer is empty" unless length $$buffer;

    }


    my $data ;
    if (IO::Compress::Base::Common::isaFilehandle($buffer))
    {
        $data = readFile($buffer);
    }
    elsif (IO::Compress::Base::Common::isaFilename($buffer))
    {
        $data = readFile($buffer);
    }
    else
    {
        $data = $$buffer ;
    }

    if (defined $already && length $already)
    {

        my $got = substr($data, 0, length($already));
        substr($data, 0, length($already)) = '';

        is $got, $already, '  Already OK' ;
    }

    my $out = '';
    my $o = new IO::Uncompress::AnyUncompress \$data, 
                    Append => 1, 
                    Transparent => 0, 
                    RawInflate => 1,
                    @opts
        or croak "Cannot open buffer/file: $AnyUncompressError" ;

    1 while $o->read($out) > 0 ;

    croak "Error uncompressing -- " . $o->error()
        if $o->error() ;

    return $out ;

}

sub getHeaders
{
    my $buffer = shift ;
    my $already = shift;

    my @opts = ();
    if (ref $buffer && ref $buffer eq 'ARRAY')
    {
        @opts = @$buffer;
        $buffer = shift @opts;
    }

    if (ref $buffer)
    {
        croak "buffer is undef" unless defined $$buffer;
        croak "buffer is empty" unless length $$buffer;

    }


    my $data ;
    if (IO::Compress::Base::Common::isaFilehandle($buffer))
    {
        $data = readFile($buffer);
    }
    elsif (IO::Compress::Base::Common::isaFilename($buffer))
    {
        $data = readFile($buffer);
    }
    else
    {
        $data = $$buffer ;
    }

    if (defined $already && length $already)
    {

        my $got = substr($data, 0, length($already));
        substr($data, 0, length($already)) = '';

        is $got, $already, '  Already OK' ;
    }

    my $out = '';
    my $o = new IO::Uncompress::AnyUncompress \$data, 
                MultiStream => 1, 
                Append => 1, 
                Transparent => 0, 
                RawInflate => 1,
                @opts
        or croak "Cannot open buffer/file: $AnyUncompressError" ;

    1 while $o->read($out) > 0 ;

    croak "Error uncompressing -- " . $o->error()
        if $o->error() ;

    return ($o->getHeaderInfo()) ;

}

sub mkComplete
{
    my $class = shift ;
    my $data = shift;
    my $Error = getErrorRef($class);

    my $buffer ;
    my %params = ();

    if ($class eq 'IO::Compress::Gzip') {
        %params = (
            Name       => "My name",
            Comment    => "a comment",
            ExtraField => ['ab' => "extra"],
            HeaderCRC  => 1);
    }
    elsif ($class eq 'IO::Compress::Zip'){
        %params = (
            Name              => "My name",
            Comment           => "a comment",
            ZipComment        => "last comment",
            exTime            => [100, 200, 300],
            ExtraFieldLocal   => ["ab" => "extra1"],
            ExtraFieldCentral => ["cd" => "extra2"],
        );
    }

    my $z = new $class( \$buffer, %params)
        or croak "Cannot create $class object: $$Error";
    $z->write($data);
    $z->close();

    my $unc = getInverse($class);
    anyUncompress(\$buffer) eq $data
        or die "bad bad bad";
    my $u = new $unc( \$buffer);
    my $info = $u->getHeaderInfo() ;


    return wantarray ? ($info, $buffer) : $buffer ;
}

sub mkErr
{
    my $string = shift ;
    my ($dummy, $file, $line) = caller ;
    -- $line ;

    $file = quotemeta($file);

    return "/$string\\s+at $file line $line/" if $] >= 5.006 ;
    return "/$string\\s+at /" ;
}

sub mkEvalErr
{
    my $string = shift ;

    return "/$string\\s+at \\(eval /" if $] > 5.006 ;
    return "/$string\\s+at /" ;
}

sub dumpObj
{
    my $obj = shift ;

    my ($dummy, $file, $line) = caller ;

    if (@_)
    {
        print "#\n# dumpOBJ from $file line $line @_\n" ;
    }
    else
    {
        print "#\n# dumpOBJ from $file line $line \n" ;
    }

    my $max = 0 ;;
    foreach my $k (keys %{ *$obj })
    {
        $max = length $k if length $k > $max ;
    }

    foreach my $k (sort keys %{ *$obj })
    {
        my $v = $obj->{$k} ;
        $v = '-undef-' unless defined $v;
        my $pad = ' ' x ($max - length($k) + 2) ;
        print "# $k$pad: [$v]\n";
    }
    print "#\n" ;
}


sub getMultiValues
{
    my $class = shift ;

    return (0,0) if $class =~ /lzf/i;
    return (1,0);
}


sub gotScalarUtilXS
{
    eval ' use Scalar::Util "dualvar" ';
    return $@ ? 0 : 1 ;
}

package CompTestUtils;

1;
__END__
	t/Test/Builder.pm
	t/Test/More.pm
	t/Test/Simple.pm
	t/compress/CompTestUtils.pm
	t/compress/any.pl
	t/compress/anyunc.pl
	t/compress/destroy.pl
	t/compress/generic.pl
	t/compress/merge.pl
	t/compress/multi.pl
	t/compress/newtied.pl
	t/compress/oneshot.pl
	t/compress/prime.pl
	t/compress/tied.pl
	t/compress/truncate.pl
	t/compress/zlib-generic.plParsing config.in...
Building Zlib enabled
Auto Detect Gzip OS Code..
Setting Gzip OS Code to 3 [Unix/Default]
Looks Good.
