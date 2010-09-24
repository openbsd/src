use Test::More 'no_plan';
use strict;

BEGIN { 
    chdir 't' if -d 't';
    use File::Spec;
    use lib File::Spec->catdir( qw[.. lib] );
}

my $Class   = 'Term::UI::History';
my $Func    = 'history';
my $Verbose = 0;            # print to STDOUT?

### test load & exports
{   use_ok( $Class );

    for my $pkg ( $Class, __PACKAGE__ ) {
        can_ok( $pkg, $Func );
    }    
}

### test string recording
{   history( $$, $Verbose );   

    my $str = $Class->history_as_string;

    ok( $str,                   "Message recorded" );
    is( $str, $$,               "   With appropriate content" );
    
    $Class->flush;
    ok( !$Class->history_as_string,
                                "   Stack flushed" );
}

### test filehandle printing 
SKIP: {   
    my $file = "$$.tmp";
    
    {   open my $fh, ">$file" or skip "Could not open $file: $!", 6;
    
        ### declare twice for 'used only once' warning
        local $Term::UI::History::HISTORY_FH = $fh;
        local $Term::UI::History::HISTORY_FH = $fh;    
        
        history( $$ );

        close $fh;
    }    

    my $str = $Class->history_as_string;
    ok( $str,                   "Message recorded" );
    is( $str, $$,               "   With appropriate content" );
    
    ### check file contents
    {   ok( -e $file,           "File $file exists" );
        ok( -s $file,           "   File has size" );
    
        open my $fh, $file or skip "Could not open $file: $!", 2;
        my $cont = do { local $/; <$fh> };
        chomp $cont;
        
        is( $cont, $str,        "   File has same content" );
    }        

    $Class->flush;
    
    ### for VMS etc
    1 while unlink $file;
    
    ok( ! -e $file,             "   File $file removed" );
}
