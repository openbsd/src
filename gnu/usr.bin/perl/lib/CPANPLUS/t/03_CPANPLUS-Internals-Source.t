### make sure we can find our conf.pl file
BEGIN { 
    use FindBin; 
    require "$FindBin::Bin/inc/conf.pl";
}

use strict;

use Module::Load;
use Test::More eval { 
            load $ENV{CPANPLUS_SOURCE_ENGINE} if $ENV{CPANPLUS_SOURCE_ENGINE}; 1 
        } ? 'no_plan'
          : (skip_all => "SQLite engine not available");

use CPANPLUS::Error;
use CPANPLUS::Backend;
use CPANPLUS::Internals::Constants;

use Data::Dumper;
use File::Basename qw[dirname];

my $conf = gimme_conf();
my $cb   = CPANPLUS::Backend->new( $conf );

### XXX temp
# $conf->set_conf( verbose => 1 );

isa_ok($cb, "CPANPLUS::Internals" );

my $modname = TEST_CONF_MODULE;

### test lookups
{   my $mt      = $cb->_module_tree;
    my $at      = $cb->_author_tree;

    ### source files should be copied from the 'server' now
    for my $name (qw[auth mod dslip] ) {
        my $file = File::Spec->catfile( 
                            $conf->get_conf('base'),
                            $conf->_get_source($name)
                    );            
        ok( (-e $file && -f _ && -s _), "$file exists" );
    }    

    ok( $at,                    "Authortree loaded successfully" );
    ok( scalar keys %$at,       "   Authortree has items in it" );
    ok( $mt,                    "Moduletree loaded successfully" );
    ok( scalar keys %$mt,       "   Moduletree has items in it" );

    my $auth    = $at->{'EUNOXS'};
    my $mod     = $mt->{$modname};

    isa_ok( $auth,              'CPANPLUS::Module::Author' );
    isa_ok( $mod,               'CPANPLUS::Module' );
}

### save state tests
SKIP: {   
    skip "Save state tests for custom engine $ENV{CPANPLUS_SOURCE_ENGINE}", 7
        if $ENV{CPANPLUS_SOURCE_ENGINE};

    ok( 1,                      "Testing save state functionality" );


    ### check we dont have a status set yet
    {   my $mod     = $cb->_module_tree->{$modname};
        ok( !$mod->_status,     "   No status set yet in module object" );
        ok( $mod->status,       "       Status now set" );
    }

    ### now save this to disk
    {   CPANPLUS::Error->flush;

        my $rv = $cb->save_state;
        ok( $rv,                "   State information saved" );
        
        like( CPANPLUS::Error->stack_as_string, qr/Writing compiled source/,    
                                "       Diagnostics confirmed" );
    }
    
    ### now we rebuild the trees from disk and
    ### check if the module object has a status saved with it
    {   CPANPLUS::Error->flush;
        ok( $cb->_build_trees( uptodate => 1, use_stored => 1),
                                "   Trees are rebuilt" );

        like( CPANPLUS::Error->stack_as_string, qr/Retrieving/,    
                                "       Diagnostics confirmed" );

    
        my $mod     = $cb->_module_tree->{$modname};
        ok( $mod->status,       "       Status now set in module object" );
    }  
}

### check custom sources
### XXX whitebox test
SKIP: {   
    ### first, find a file to serve as a source
    my $mod     = $cb->_module_tree->{$modname};
    my $package = File::Spec->rel2abs(
                        File::Spec->catfile( 
                            $FindBin::Bin,
                            TEST_CONF_CPAN_DIR,
                            $mod->path,
                            $mod->package,
                        )
                    );      
       
    ok( $package,               "Found file for custom source" );
    ok( -e $package,            "   File '$package' exists" );

    ### remote uri    
    my $uri      =  $cb->_host_to_uri(
                        scheme  => 'file',
                        host    => '',
                        path    => File::Spec->catfile( dirname($package) )
                    );

    my $expected_file = $cb->__custom_module_source_index_file( uri => $uri );
    
    ok( $expected_file,         "Sources should be written to '$uri'" );
    
    skip( "Index file size too long (>260 chars). Can't write to disk", 28 )
        if length $expected_file > 260 and ON_WIN32;
            

    ### local file 
    ### 2 tests
    my $src_file = $cb->_add_custom_module_source( uri => $uri );
    ok( $src_file,              "Sources written to '$src_file'" );                     
    ok( -e $src_file,           "   File exists" );                     
                     
    ### and write the file  
    ### 5 tests
    {   my $meth = '__write_custom_module_index';
        can_ok( $cb,    $meth );

        my $rv = $cb->$meth( 
                        path => dirname( $package ),
                        to   => $src_file
                    );

        ok( $rv,                "   Sources written" );
        is( $rv, $src_file,     "       Written to expected file" );
        ok( -e $src_file,       "       Source file exists" );
        ok( -s $src_file,       "       File has non-zero size" );
    }              
    
    ### let's see if we can find our custom files
    ### 3 tests
    {   my $meth = '__list_custom_module_sources';
        can_ok( $cb,    $meth );
        
        my %files = $cb->$meth;
        ok( scalar(keys(%files)),
                                "   Got list of sources" );
        
        ### on VMS, we can't predict the case unfortunately
        ### so grep for it instead;
        my $found = map { 
            my $src_re = quotemeta($src_file);
            $_ =~ /$src_re/i;
        } keys %files;

        ok( $found,             "   Found proper entry for $src_file" );
    }        

    ### now we can have it be loaded in
    ### 6 tests
    {   my $meth = '__create_custom_module_entries';
        can_ok( $cb,    $meth );

        ### now add our own sources
        ok( $cb->$meth,         "Sources file loaded" );

        my $add_name = TEST_CONF_INST_MODULE;
        my $add      = $cb->_module_tree->{$add_name};
        ok( $add,               "   Found added module" );

        ok( $add->status->_fetch_from,  
                                "       Full download path set" );
        is( $add->author->cpanid, CUSTOM_AUTHOR_ID,
                                "       Attributed to custom author" );

        ### since we replaced an existing module, there should be
        ### a message on the stack
        like( CPANPLUS::Error->stack_as_string, qr/overwrite module tree/i,
                                "   Addition message recorded" );
    }

    ### test updating custom sources
    ### 3 tests
    {   my $meth    = '__update_custom_module_sources';
        can_ok( $cb,    $meth );
        
        ### mark what time it is now, sleep 1 second for better measuring
        my $now     = time;        
        sleep 1;
        
        my $ok      = $cb->$meth;

        ok( $ok,                    "Custom sources updated" );
        cmp_ok( [stat $src_file]->[9], '>=', $now,
                                    "   Timestamp on sourcefile updated" );    
    }
    
    ### now update it individually
    ### 3 tests    
    {   my $meth    = '__update_custom_module_source';
        can_ok( $cb,    $meth );
        
        ### mark what time it is now, sleep 1 second for better measuring
        my $now     = time;        
        sleep 1;
        
        my $ok      = $cb->$meth( remote => $uri );

        ok( $ok,                    "Custom source for '$uri' updated" );
        cmp_ok( [stat $src_file]->[9], '>=', $now,
                                    "   Timestamp on sourcefile updated" );    
    }

    ### now update using the higher level API, see if it's part of the update
    ### 3 tests    
    {   CPANPLUS::Error->flush;

        ### mark what time it is now, sleep 1 second for better measuring
        my $now = time;        
        sleep 1;
        
        my $ok  = $cb->_build_trees(
                        uptodate    => 0,
                        use_stored  => 0,
                    );
    
        ok( $ok,                    "All sources updated" );
        cmp_ok( [stat $src_file]->[9], '>=', $now,
                                    "   Timestamp on sourcefile updated" );    

        like( CPANPLUS::Error->stack_as_string, qr/Updating sources from/,
                                    "   Update recorded in the log" );
    }
    
    ### now remove the index file;
    ### 3 tests    
    {   my $meth = '_remove_custom_module_source';
        can_ok( $cb,    $meth );
        
        my $file = $cb->$meth( uri => $uri );
        ok( $file,                  "Index file removed" );
        ok( ! -e $file,             "   File '$file' no longer on disk" );
    }
}

# Local variables:
# c-indentation-style: bsd
# c-basic-offset: 4
# indent-tabs-mode: nil
# End:
# vim: expandtab shiftwidth=4:
