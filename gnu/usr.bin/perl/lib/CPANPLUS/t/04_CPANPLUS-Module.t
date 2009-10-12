### make sure we can find our conf.pl file
BEGIN { 
    use FindBin; 
    require "$FindBin::Bin/inc/conf.pl";
}

use strict;

use CPANPLUS::Configure;
use CPANPLUS::Backend;
use CPANPLUS::Module::Fake;
use CPANPLUS::Module::Author::Fake;
use CPANPLUS::Internals::Constants;

use Test::More 'no_plan';
use Data::Dumper;
use File::Spec;
use File::Path ();

my $Conf    = gimme_conf();
my $CB      = CPANPLUS::Backend->new( $Conf );

### start with fresh sources ###
ok( $CB->reload_indices( update_source => 0 ),  "Rebuilding trees" );  

my $AuthName    = TEST_CONF_AUTHOR;
my $Auth        = $CB->author_tree( $AuthName );
my $ModName     = TEST_CONF_MODULE;
my $Mod         = $CB->module_tree( $ModName );
my $CoreName    = TEST_CONF_PREREQ;
my $CoreMod     = $CB->module_tree( $CoreName );

isa_ok( $Auth,                  'CPANPLUS::Module::Author' );
isa_ok( $Mod,                   'CPANPLUS::Module' );
isa_ok( $CoreMod,               'CPANPLUS::Module' );

### author accessors ###
is( $Auth->author, 'ExtUtils::MakeMaker No XS Code',
                                "Author name: "     . $Auth->author );
is( $Auth->cpanid, $AuthName,   "Author CPANID: "   . $Auth->cpanid );
is( $Auth->email, DEFAULT_EMAIL,"Author email: "    . $Auth->email );
isa_ok( $Auth->parent,          'CPANPLUS::Backend' );

### module accessors ###
{   my %map = (
        ### method      ### result
        module      =>  $ModName,
        name        =>  $ModName,
        comment     =>  undef,
        package     =>  'Foo-Bar-0.01.tar.gz',
        path        =>  'authors/id/EUNOXS',      
        version     =>  '0.01',
        dslip       =>  'cdpO ',
        description =>  'CPANPLUS Test Package', 
        mtime       =>  '',
        author      =>  $Auth,
    );        

    my @acc = $Mod->accessors;
    ok( scalar(@acc),           "Retrieved module accessors" );
    
    ### remove private accessors
    is_deeply( [ sort keys %map ], [ sort grep { $_ !~ /^_/ } @acc ],
                                "   About to test all accessors" );

    ### check all the accessors
    while( my($meth,$res) = each %map ) {
        is( $Mod->$meth, $res,  "   Mod->$meth: " . ($res || '<empty>') );
    }

    ### check accessor objects ###
    isa_ok( $Mod->parent,       'CPANPLUS::Backend' );
    isa_ok( $Mod->author,       'CPANPLUS::Module::Author' );
    is( $Mod->author->author, $Auth->author,            
                                "Module eq Author" );
}

### convenience methods ###
{   ok( 1,                                          "Convenience functions" );
    is( $Mod->package_name,      'Foo-Bar',         "   Package name");
    is( $Mod->package_version,   '0.01',            "   Package version");
    is( $Mod->package_extension, 'tar.gz',          "   Package extension");
    ok( !$Mod->package_is_perl_core,                "   Package not core");
    ok( !$Mod->module_is_supplied_with_perl_core,   "   Module not core" );
    ok( !$Mod->is_bundle,                           "   Package not bundle");
}

### clone & status tests
{   my $clone = $Mod->clone;
    ok( $clone,                 "Module cloned" );
    isa_ok( $clone,             'CPANPLUS::Module' );
    
    for my $acc ( $Mod->accessors ) {
        is( $clone->$acc, $Mod->$acc,
                                "   Clone->$acc matches Mod->$acc " );
    }
    
    ### XXX whitebox test 
    ok( !$clone->_status,      "Status object empty on start" );
    
    my $status = $clone->status;
    ok( $status,                "   Status object defined after query" );
    is( $status, $clone->_status, 
                                "   Object stored as expected" );
    isa_ok( $status,            'Object::Accessor' );
}

{   ### extract + error test ###
    ok( !$Mod->extract(),   "Cannot extract unfetched file" );
    like( CPANPLUS::Error->stack_as_string, qr/You have not fetched/,
                            "   Error properly logged" );
}      

{   ### fetch tests ###
    ### enable signature checks for checksums ###
    my $old = $Conf->get_conf('signature');
    $Conf->set_conf(signature => 1);  
    
    my $where = $Mod->fetch( force => 1 );
    ok( $where,             "Module fetched" );
    ok( -f $where,          "   Module is a file" );
    ok( -s $where,          "   Module has size" );
    
    $Conf->set_conf( signature => $old );
}

{   ### extract tests ###
    my $dir = $Mod->extract( force => 1 );
    ok( $dir,               "Module extracted" );
    ok( -d $dir,            "   Dir exsits" );
}


{   ### readme tests ###
    my $readme = $Mod->readme;
    ok( length $readme,     "Readme found" );
    is( $readme, $Mod->status->readme,
                            "   Readme stored in module object" );
}

{   ### checksums tests ###
    SKIP: {
        skip(q[You chose not to enable checksum verification], 5)
            unless $Conf->get_conf('md5');
    
        my $cksum_file = $Mod->checksums;
        ok( $cksum_file,    "Checksum file found" );
        is( $cksum_file, $Mod->status->checksums,
                            "   File stored in module object" );
        ok( -e $cksum_file, "   File exists" );
        ok( -s $cksum_file, "   File has size" );
    
        ### XXX test checksum_value if there's digest::md5 + config wants it
        ok( $Mod->status->checksum_ok,
                            "   Checksum is ok" );
                            
        ### check ttl code for checksums; fetching it now means the cache 
        ### should kick in
        {   CPANPLUS::Error->flush;
            ok( $Mod->checksums,       
                            "   Checksums re-fetched" );
            like( CPANPLUS::Error->stack_as_string, qr/Using cached file/,
                            "       Cached file used" );
        }                            
    }
}


{   ### installer type tests ###
    my $installer  = $Mod->get_installer_type;
    ok( $installer,         "Installer found" );
    is( $installer, INSTALLER_MM,
                            "   Proper installer found" );
}

{   ### check signature tests ###
    SKIP: {
        skip(q[You chose not to enable signature checks], 1)
            unless $Conf->get_conf('signature');
            
        ok( $Mod->check_signature,
                            "Signature check OK" );
    }
}

### dslip & related
{   my $dslip = $Mod->dslip;   
    ok( $dslip,             "Got dslip information from $ModName ($dslip)" );

    ### now find it for a submodule
    {   my $submod = $CB->module_tree( TEST_CONF_MODULE_SUB );
        ok( $submod,        "   Found submodule " . $submod->name );
        ok( $submod->dslip, "   Got dslip info (".$submod->dslip.")" );
        is( $submod->dslip, $dslip,
                            "   It's identical to $ModName" );
    }                            
}

{   ### details() test ###   
    my $href = {
        'Support Level'     => 'Developer',
        'Package'           => $Mod->package,
        'Description'       => $Mod->description,
        'Development Stage' => 
                'under construction but pre-alpha (not yet released)',
        'Author'            => sprintf("%s (%s)", $Auth->author, $Auth->email),
        'Version on CPAN'   => $Mod->version,
        'Language Used'     => 
                'Perl-only, no compiler needed, should be platform independent',
        'Interface Style'   => 
                'Object oriented using blessed references and/or inheritance',
        'Public License'    => 'Unknown',                
        ### XXX we can't really know what you have installed ###
        #'Version Installed' => '0.06',
    };   

    my $res = $Mod->details;
    
    ### delete they key of which we don't know the value ###
    delete $res->{'Version Installed'};
    
    is_deeply( $res, $href, "Details OK" );        
}

{   ### contians() test ###
    ### XXX ->contains works based on package name. in our sourcefiles
    ### we use 4x the same package name for different modules. So use
    ### the only unique package name here, which is the one for the core mod
    my @list = $CoreMod->contains;
    
    ok( scalar(@list),          "Found modules contained in this one" );
    is_deeply( \@list, [$CoreMod],  
                                "   Found all modules expected" );
}

{   ### testing distributions() ###
    my @mdists = $Mod->distributions;
    is( scalar @mdists, 1, "Distributions found via module" );

    my @adists = $Auth->distributions;
    is( scalar @adists, 3,  "Distributions found via author" );
}

{   ### test status->flush ###
    ok( $Mod->status->mk_flush,
                            "Status flushed" );
    ok(!$Mod->status->fetch,"   Fetch status empty" );
    ok(!$Mod->status->extract,
                            "   Extract status empty" );
    ok(!$Mod->status->checksums,
                            "   Checksums status empty" );
    ok(!$Mod->status->readme,
                            "   Readme status empty" );
}

{   ### testing bundles ###
    my $bundle = $CB->module_tree('Bundle::Foo::Bar');
    isa_ok( $bundle,            'CPANPLUS::Module' );

    ok( $bundle->is_bundle,     "   It's a Bundle:: module" );
    ok( $bundle->fetch,         "   Fetched the bundle" );
    ok( $bundle->extract,       "   Extracted the bundle" );

    my @objs = $bundle->bundle_modules;
    is( scalar(@objs), 5,       "   Found all prerequisites" );
    
    for( @objs ) {
        isa_ok( $_, 'CPANPLUS::Module', 
                                "   Prereq " . $_->module  );
        ok( defined $bundle->status->prereqs->{$_->module},
                                "       Prereq was registered" );
    }
}

{   ### testing autobundles
    my $file    = File::Spec->catfile( 
                        dummy_cpan_dir(), 
                        $Conf->_get_build('autobundle'),
                        'Snapshot.pm' 
                    );
    my $uri     = $CB->_host_to_uri( scheme => 'file', path => $file );
    my $bundle  = $CB->parse_module( module => $uri );
    
    ok( -e $file,               "Creating bundle from '$file'" );
    ok( $bundle,                "   Object created" );
    isa_ok( $bundle, 'CPANPLUS::Module',
                                "   Object" );
    ok( $bundle->is_bundle,     "   Recognized as bundle" );
    ok( $bundle->is_autobundle, "   Recognized as autobundle" );
    
    my $type = $bundle->get_installer_type;
    ok( $type,                  "   Found installer type" );
    is( $type, INSTALLER_AUTOBUNDLE,
                                "       Installer type is $type" );

    my $where = $bundle->fetch;
    ok( $where,                 "   Autobundle fetched" );
    ok( -e $where,              "       File exists" );


    my @list = $bundle->bundle_modules;
    ok( scalar(@list),          "   Prereqs found" );
    is( scalar(@list), 1,       "       Right number of prereqs" );
    isa_ok( $list[0], 'CPANPLUS::Module',
                                "       Object" );
                                
    ### skiptests to make sure we don't get any test header mismatches
    my $rv = $bundle->create( prereq_target => 'create', skiptest => 1 );
    ok( $rv,                    "   Tested prereqs" );

}

### test module from perl core ###
{   isa_ok( $CoreMod, 'CPANPLUS::Module',
                                "Core module " . $CoreName );
    ok( $CoreMod->package_is_perl_core, 
                                "   Package found in perl core" );
    
    ### check if it's core with 5.6.1
    {   local $] = '5.006001';
        ok( $CoreMod->module_is_supplied_with_perl_core,
                                "   Module also found in perl core");
    }
    
    ok( !$CoreMod->install,     "   Package not installed" );
    like( CPANPLUS::Error->stack_as_string, qr/core Perl/,
                                "   Error properly logged" );
}    

### test third-party modules
SKIP: {
    skip "Module::ThirdParty not installed", 10 
        unless eval { require Module::ThirdParty; 1 };

    ok( !$Mod->is_third_party, 
                                "Not a 3rd party module: ". $Mod->name );
    
    my $fake = $CB->parse_module( module => 'LOCAL/SVN-Core-1.0' );
    ok( $fake,                  "Created module object for ". $fake->name );
    ok( $fake->is_third_party,
                                "   It is a 3rd party module" );

    my $info = $fake->third_party_information;
    ok( $info,                  "Got 3rd party package information" );
    isa_ok( $info,              'HASH' );
    
    for my $item ( qw[name url author author_url] ) {
        ok( length($info->{$item}),
                                "   $item field is filled" );
    }        
}

### testing EU::Installed methods in Dist::MM tests ###

# Local variables:
# c-indentation-style: bsd
# c-basic-offset: 4
# indent-tabs-mode: nil
# End:
# vim: expandtab shiftwidth=4:
