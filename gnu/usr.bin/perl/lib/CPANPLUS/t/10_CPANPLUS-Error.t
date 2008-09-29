### make sure we can find our conf.pl file
BEGIN { 
    use FindBin; 
    require "$FindBin::Bin/inc/conf.pl";
}

use strict;
use Test::More 'no_plan';
use Data::Dumper;
use FileHandle;
use CPANPLUS::Error;

my $conf = gimme_conf();

my $map = {
    cp_msg      => ["This is just a test message"],
    msg         => ["This is just a test message"],
    cp_error    => ["This is just a test error"],
    error       => ["This is just a test error"],
};

### check if CPANPLUS::Error can do what we expect 
{   for my $name ( keys %$map ) {
        can_ok('CPANPLUS::Error',   $name);
        can_ok('main',              $name);     # did it get exported?
    }
}

### make sure we start with an empty stack
{   CPANPLUS::Error->flush;
    is( scalar(()=CPANPLUS::Error->stack), 0,  
                        "Starting with empty stack" );        
}

### global variables test ###
{   my $file = output_file();

    ### this *has* to be set, as we're testing the contents of the file
    ### to see if it matches what's stored in the buffer.
    local $CPANPLUS::Error::MSG_FH   = output_handle();    
    local $CPANPLUS::Error::ERROR_FH = output_handle();
    
    ok( -e $file,           "Output redirect file exists" );
    ok( !-s $file,          "   Output file is empty" );

    ### print a msg & error ###
    for my $name ( keys %$map ) {
        my $sub = __PACKAGE__->can( $name );

        $sub->( $map->{$name}->[0], 1 );
    }

    ### must close it for Win32 tests!
    close output_handle;           

    ok( -s $file,           "   Output file now has size" );
    
    my $fh = FileHandle->new( $file );
    ok( $fh,                "Opened output file for reading " );
    
    my $contents = do { local $/; <$fh> };
    my $string   = CPANPLUS::Error->stack_as_string;
    my $trace    = CPANPLUS::Error->stack_as_string(1);
    
    ok( $contents,          "   Got the file contents" );
    ok( $string,            "Got the error stack as string" );
    
    
    for my $type ( keys %$map ) {
        my $tag = $type; $tag =~ s/.+?_//g;
    
        for my $str (@{ $map->{$type} } ) {
            like( $contents, qr/\U\Q$tag/,
                            "   Contents matches for '$type'" ); 
            like( $contents, qr/\Q$str/,
                            "   Contents matches for '$type'" ); 
                            
            like( $string, qr/\U\Q$tag/,
                            "   String matches for '$type'" );                
            like( $string, qr/\Q$str/,
                            "   String matches for '$type'" );

            like( $trace, qr/\U\Q$tag/,
                            "   Trace matches for '$type'" );                
            like( $trace, qr/\Q$str/,
                            "   Trace matches for '$type'" );
    
            ### extra trace tests ###
            like( $trace,   qr/\Q$str\E.*?\Q$str/s,
                                "   Trace holds proper traceback" );
            like( $trace,   qr/\Q$0/,
                                "   Trace holds program name" );
            like( $trace,   qr/line/,
                                "   Trace holds line number information" );
        }      
    }

    ### check the stack, flush it, check again ###
    is( scalar(()=CPANPLUS::Error->stack), scalar(keys(%$map)),  
                        "All items on stack" );
    is( scalar(()=CPANPLUS::Error->flush), scalar(keys(%$map)),
                        "All items flushed" );
    is( scalar(()=CPANPLUS::Error->stack), 0,  
                        "No items on stack" );                        
    
}


# Local variables:
# c-indentation-style: bsd
# c-basic-offset: 4
# indent-tabs-mode: nil
# End:
# vim: expandtab shiftwidth=4:
