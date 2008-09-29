
use strict;
use warnings;
use bytes;

use Test::More ;
use CompTestUtils;

BEGIN 
{ 
    plan skip_all => "Encode is not available"
        if $] < 5.006 ;

    eval { require Encode; Encode->import(); };

    plan skip_all => "Encode is not available"
        if $@ ;
    
    # use Test::NoWarnings, if available
    my $extra = 0 ;

    my $st = eval { require Test::NoWarnings ;  import Test::NoWarnings; 1; };
    $extra = 1
        if $st ;

    plan(tests => 7 + $extra) ;
}

sub run
{
    my $CompressClass   = identify();
    my $UncompressClass = getInverse($CompressClass);
    my $Error           = getErrorRef($CompressClass);
    my $UnError         = getErrorRef($UncompressClass);


    my $string = "\x{df}\x{100}"; 
    my $encString = Encode::encode_utf8($string);
    my $buffer = $encString;

    #for my $from ( qw(filename filehandle buffer) )
    {
#        my $input ;
#        my $lex = new LexFile my $name ;
#
#        
#        if ($from eq 'buffer')
#          { $input = \$buffer }
#        elsif ($from eq 'filename')
#        {
#            $input = $name ;
#            writeFile($name, $buffer);
#        }
#        elsif ($from eq 'filehandle')
#        {
#            $input = new IO::File "<$name" ;
#        }

        for my $to ( qw(filehandle buffer))
        {
            title "OO Mode: To $to, Encode by hand";

            my $lex2 = new LexFile my $name2 ;
            my $output;
            my $buffer;

            if ($to eq 'buffer')
              { $output = \$buffer }
            elsif ($to eq 'filename')
            {
                $output = $name2 ;
            }
            elsif ($to eq 'filehandle')
            {
                $output = new IO::File ">$name2" ;
            }


            my $out ;
            my $cs = new $CompressClass($output, AutoClose =>1);
            $cs->print($encString);
            $cs->close();

            my $input;
            if ($to eq 'buffer')
              { $input = \$buffer }
            else 
            {
                $input = $name2 ;
            }

            my $ucs = new $UncompressClass($input, Append => 1);
            my $got;
            1 while $ucs->read($got) > 0 ;
            my $decode = Encode::decode_utf8($got);


            is $string, $decode, "  Expected output";


        }
    }

    {
        title "Catch wide characters";

        my $out;
        my $cs = new $CompressClass(\$out);
        my $a = "a\xFF\x{100}";
        eval { $cs->syswrite($a) };
        like($@, qr/Wide character in ${CompressClass}::write/, 
                 "  wide characters in ${CompressClass}::write");
        eval { syswrite($cs, $a) };
        like($@, qr/Wide character in ${CompressClass}::write/, 
                 "  wide characters in ${CompressClass}::write");
    }

}


 
1;

