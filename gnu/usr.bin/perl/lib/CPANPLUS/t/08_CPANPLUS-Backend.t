### make sure we can find our conf.pl file
BEGIN { 
    use FindBin; 
    require "$FindBin::Bin/inc/conf.pl";
}

use strict;
use Test::More      'no_plan';
use File::Basename  'dirname';

use Data::Dumper;
use CPANPLUS::Error;
use CPANPLUS::Internals::Constants;

my $conf = gimme_conf();

my $Class = 'CPANPLUS::Backend';
### D::C has troubles with the 'use_ok' -- it finds the wrong paths.
### for now, do a 'use' instead
#use_ok( $Class ) or diag "$Class not found";
use CPANPLUS::Backend;

my $cb = $Class->new( $conf );
isa_ok( $cb, $Class );

my $mt = $cb->module_tree;
my $at = $cb->author_tree;
ok( scalar keys %$mt,       "Module tree has entries" ); 
ok( scalar keys %$at,       "Author tree has entries" ); 

### module_tree tests ###
my $Name = TEST_CONF_MODULE;
my $mod  = $cb->module_tree($Name);

### XXX SOURCEFILES FIX
{   my @mods = $cb->module_tree($Name,$Name);
    my $none = $cb->module_tree( TEST_CONF_INVALID_MODULE );
    
    ok( IS_MODOBJ->(mod => $mod),           "Module object found" );
    is( scalar(@mods), 2,                   "   Module list found" );
    ok( IS_MODOBJ->(mod => $mods[0]),       "   ISA module object" );
    ok( !IS_MODOBJ->(mod => $none),         "   Bogus module detected");
}

### author_tree tests ###
{   my @auths = $cb->author_tree( $mod->author->cpanid,
                                  $mod->author->cpanid );
    my $none  = $cb->author_tree( 'fnurk' );
    
    ok( IS_AUTHOBJ->(auth => $mod->author), "Author object found" );
    is( scalar(@auths), 2,                  "   Author list found" );
    ok( IS_AUTHOBJ->( author => $auths[0] ),"   ISA author object" );
    is( $mod->author, $auths[0],            "   Objects are identical" );
    ok( !IS_AUTHOBJ->( author => $none ),   "   Bogus author detected" );
}

my $conf_obj = $cb->configure_object;
ok( IS_CONFOBJ->(conf => $conf_obj),    "Configure object found" );


### parse_module tests ###
{   my @map = (                                  
        $Name => [ 
            $mod->author->cpanid,   # author
            $mod->package_name,     # package name
            $mod->version,          # version
        ],
        $mod => [ 
            $mod->author->cpanid,  
            $mod->package_name, 
            $mod->version, 
        ],
        'Foo-Bar-EU-NOXS' => [ 
            $mod->author->cpanid,  
            $mod->package_name, 
            $mod->version,
        ],
        'Foo-Bar-EU-NOXS-0.01' => [ 
            $mod->author->cpanid,  
            $mod->package_name, 
            '0.01',
        ],
        'EUNOXS/Foo-Bar-EU-NOXS' => [ 
            'EUNOXS',
            $mod->package_name, 
            $mod->version,
        ],
        'EUNOXS/Foo-Bar-EU-NOXS-0.01' => [ 
            'EUNOXS',              
            $mod->package_name, 
            '0.01',
        ],
        ### existing module, no extension given
        ### this used to create a modobj with no package extension
        'EUNOXS/Foo-Bar-0.02' => [ 
            'EUNOXS',              
            'Foo-Bar',
            '0.02',
        ],
        'Foo-Bar-EU-NOXS-0.09' => [ 
            $mod->author->cpanid,  
            $mod->package_name, 
            '0.09',
        ],
        'MBXS/Foo-Bar-EU-NOXS-0.01' => [ 
            'MBXS',                
            $mod->package_name, 
            '0.01',
        ],
        'EUNOXS/Foo-Bar-EU-NOXS-0.09' => [ 
            'EUNOXS',
            $mod->package_name, 
            '0.09',
        ],
        'EUNOXS/Foo-Bar-EU-NOXS-0.09.zip' => [ 
            'EUNOXS',
            $mod->package_name, 
            '0.09',
        ],
        'FROO/Flub-Flob-1.1.zip' => [ 
            'FROO',    
            'Flub-Flob',    
            '1.1',  
        ],
        'G/GO/GOYALI/SMS_API_3_01.tar.gz' => [ 
            'GOYALI',  
            'SMS_API',      
            '3_01', 
        ],
        'E/EY/EYCK/Net/Lite/Net-Lite-FTP-0.091' => [ 
            'EYCK',    
            'Net-Lite-FTP', 
            '0.091',
        ],
        'EYCK/Net/Lite/Net-Lite-FTP-0.091' => [ 
            'EYCK',
            'Net-Lite-FTP', 
            '0.091',
        ],
        'M/MA/MAXDB/DBD-MaxDB-7.5.0.24a' => [ 
            'MAXDB',
            'DBD-MaxDB',
            '7.5.0.24a', 
        ],
        'EUNOXS/perl5.005_03.tar.gz' => [ 
            'EUNOXS',  
            'perl',
            '5.005_03',
        ],
        'FROO/Flub-Flub-v1.1.0.tbz' => [ 
            'FROO',    
            'Flub-Flub',       
            'v1.1.0', 
        ],
        'FROO/Flub-Flub-1.1_2.tbz' => [ 
            'FROO',    
            'Flub-Flub',       
            '1.1_2',
        ],   
        'LDS/CGI.pm-3.27.tar.gz' => [ 
            'LDS',
            'CGI',
            '3.27', 
        ],
        'FROO/Text-Tabs+Wrap-2006.1117.tar.gz' => [ 
            'FROO',    
            'Text-Tabs+Wrap',
            '2006.1117',                                                      
        ],   
        'JETTERO/Crypt-PBC-0.7.20.0-0.4.9' => [ 
            'JETTERO',
            'Crypt-PBC',
            '0.7.20.0-0.4.9' ,
        ],
        'GRICHTER/HTML-Embperl-1.2.1.tar.gz' => [ 
            'GRICHTER',            
            'HTML-Embperl', 
            '1.2.1',
        ],
        'KANE/File-Fetch-0.15_03' => [
            'KANE',
            'File-Fetch',
            '0.15_03',
        ],
        'AUSCHUTZ/IO-Stty-.02.tar.gz' => [
            'AUSCHUTZ',
            'IO-Stty',
            '.02',
        ],            
        '.' => [
            'CPANPLUS',
            't',
            '',
        ],            
    );       

    while ( my($guess, $attr) = splice @map, 0, 2 ) {
        my( $author, $pkg_name, $version ) = @$attr;

        ok( $guess,             "Attempting to parse $guess" );

        my $obj = $cb->parse_module( module => $guess );
        
        ok( $obj,               "   Result returned" );
        ok( IS_MODOBJ->( mod => $obj ), 
                                "   parse_module success by '$guess'" );     
        
        is( $obj->version, $version,
                                "   Proper version found: $version" );
        is( $obj->package_version, $version,
                                "       Found in package_version as well" );

        ### VMS doesn't preserve case, so match them after normalizing case
        is( uc($obj->package_name), uc($pkg_name),
                                "   Proper package_name found: $pkg_name" );
        unlike( $obj->package_name, qr/\d/,
                                "       No digits in package name" );
        {   my $ext = $obj->package_extension;
            ok( $ext,           "       Has extension as well: $ext" );
        }
        
        like( $obj->author->cpanid, "/$author/i", 
                                "   Proper author found: $author");
        like( $obj->path,           "/$author/i", 
                                "   Proper path found: " . $obj->path );
    }


    ### test for things that look like real modules, but aren't ###
    {   my @map = (
            [  $Name . $$ => [
                [qr/does not contain an author/,"Missing author part detected"],
                [qr/Cannot find .+? in the module tree/,"Unable to find module"]
            ] ],
            [ {}, => [
                [ qr/module string from reference/,"Unable to parse ref"] 
            ] ],
        );

        for my $entry ( @map ) {
            my($mod,$aref) = @$entry;
            
            my $none = $cb->parse_module( module => $mod );
            ok( !IS_MODOBJ->(mod => $none),     
                                "Non-existant module detected" );
            ok( !IS_FAKE_MODOBJ->(mod => $none),
                                "Non-existant fake module detected" );
        
            my $str = CPANPLUS::Error->stack_as_string;
            for my $pair (@$aref) {
                my($re,$diag) = @$pair;
                like( $str, $re,"   $diag" );
            }
        }    
    }
    
    ### test parsing of arbitrary URI
    for my $guess ( qw[ http://foo/bar.gz
                        http://a/b/c/d/e/f/g/h/i/j
                        flub://floo ]
    ) {
        my $obj = $cb->parse_module( module => $guess );
        ok( IS_FAKE_MODOBJ->(mod => $obj), 
                                "parse_module success by '$guess'" );
        is( $obj->status->_fetch_from, $guess,
                                "   Fetch from set ok" );
    }                                       
}         

### RV tests ###
{   my $method = 'readme';
    my %args   = ( modules => [$Name] );  
    
    my $rv = $cb->$method( %args );
    ok( IS_RVOBJ->( $rv ),              "Got an RV object" );
    ok( $rv->ok,                        "   Overall OK" );
    cmp_ok( $rv, '==', 1,               "   Overload OK" );
    is( $rv->function, $method,         "   Function stored OK" );     
    is_deeply( $rv->args, \%args,       "   Arguments stored OK" );
    is( $rv->rv->{$Name}, $mod->readme, "   RV as expected" );
}

### reload_indices tests ###
{
    my $file = File::Spec->catfile( $conf->get_conf('base'),
                                    $conf->_get_source('mod'),
                                );
  
    ok( $cb->reload_indices( update_source => 0 ),  "Rebuilding trees" );                              
    my $age = -M $file;
    
    ### make sure we are 'newer' on faster machines with a sleep..
    ### apparently Win32's FAT isn't granual enough on intervals
    ### < 2 seconds, so it may give the same answer before and after
    ### the sleep, causing the test to fail. so sleep atleast 2 seconds.
    sleep 2;
    ok( $cb->reload_indices( update_source => 1 ),  
                                    "Rebuilding and refetching trees" );
    cmp_ok( $age, '>', -M $file,    "    Source file '$file' updated" );                                      
}

### flush tests ###
{
    for my $cache( qw[methods hosts modules lib all] ) {
        ok( $cb->flush($cache), "Cache $cache flushed ok" );
    }
}

### installed tests ###
{   ok( scalar($cb->installed), "Found list of installed modules" );
}    
                
### autobudle tests ###
{
    my $where = $cb->autobundle;
    ok( $where,     "Autobundle written" );
    ok( -s $where,  "   File has size" );
}

### local_mirror tests ###
{   ### turn off md5 checks for the 'fake' packages we have 
    my $old_md5 = $conf->get_conf('md5');
    $conf->set_conf( md5 => 0 );

    ### otherwise 'status->fetch' might be undef! ###
    my $rv = $cb->local_mirror( path => 'dummy-localmirror' );
    ok( $rv,                        "Local mirror created" );
    
    for my $mod ( values %{ $cb->module_tree } ) {
        my $name    = $mod->module;
        
        my $cksum   = File::Spec->catfile(
                        dirname($mod->status->fetch),
                        CHECKSUMS );
        ok( -e $mod->status->fetch, "   Module '$name' fetched" );
        ok( -s _,                   "       Module '$name' has size" );
        ok( -e $cksum,              "   Checksum fetched for '$name'" );
        ok( -s _,                   "       Checksum for '$name' has size" );
    }      

    $conf->set_conf( md5 => $old_md5 );
}    

### check ENV variable
{   ### process id
    {   my $name = 'PERL5_CPANPLUS_IS_RUNNING';
        ok( $ENV{$name},            "Env var '$name' set" );
        is( $ENV{$name}, $$,        "   Set to current process id" );
    }

    ### Version    
    {   my $name = 'PERL5_CPANPLUS_IS_VERSION';
        ok( $ENV{$name},            "Env var '$name' set" );

        ### version.pm formats ->VERSION output... *sigh*
        is( $ENV{$name}, $Class->VERSION,        
                                    "   Set to current process version" );
    }
    
}

__END__    
                                          
# Local variables:
# c-indentation-style: bsd
# c-basic-offset: 4
# indent-tabs-mode: nil
# End:
# vim: expandtab shiftwidth=4:                    
                    
