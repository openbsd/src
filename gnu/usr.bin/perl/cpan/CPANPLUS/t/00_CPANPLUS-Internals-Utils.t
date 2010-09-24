### make sure we can find our conf.pl file
BEGIN { 
    use FindBin; 
    require "$FindBin::Bin/inc/conf.pl";
}

use strict;

### make sure to keep the plan -- this is the only test
### supported for 'older' T::H (pre 2.28) -- see Makefile.PL for details
use Test::More tests => 40;

use Cwd;
use Data::Dumper;
use File::Spec;
use File::Basename;

use CPANPLUS::Error;
use CPANPLUS::Internals::Utils;

# File::Spec and Cwd might return different values for a
# symlinked directory, so we need to be careful.
sub paths_are_same {
    my($have, $want, $name) = @_;

    $have = _resolve_symlinks($have);
    $want = _resolve_symlinks($want);

    my $builder = Test::More->builder;
    return $builder->like( $have, qr/\Q$want/i, $name );
}

# Resolve any symlinks in a path
sub _resolve_symlinks {
    my $path = shift;
    my($vol, $dirs, $file) = File::Spec->splitpath($path);

    my $resolved = File::Spec->catpath( $vol, "", "" );

    for my $dir (File::Spec->splitdir($dirs)) {
        # Resolve the next part of the path
        my $next = File::Spec->catdir( $resolved, $dir );
        $next = eval { readlink $next } || $next;

        # If its absolute, use it.
        # Otherwise tack it onto the end of the previous path.
        $resolved = File::Spec->file_name_is_absolute($next)
                       ? $next
                       : File::Spec->catdir( $resolved, $next );
    }

    return File::Spec->catfile($resolved, $file);
}

my $Cwd     = File::Spec->rel2abs(cwd());
my $Class   = 'CPANPLUS::Internals::Utils';
my $Dir     = 'foo';
my $Move    = 'bar';
my $File    = 'zot';

rmdir $Move if -d $Move;
rmdir $Dir  if -d $Dir;

### test _mdkir ###
{   ok( $Class->_mkdir( dir => $Dir),   "Created dir '$Dir'" );
    ok( -d $Dir,                        "   '$Dir' is a dir" );
}

### test _chdir ###
{   ok( $Class->_chdir( dir => $Dir),   "Chdir to '$Dir'" );    

    my $abs = File::Spec->rel2abs(File::Spec->catdir($Cwd,$Dir));
    paths_are_same( File::Spec->rel2abs(cwd()), $abs,
                                        "   Cwd() is '$Dir'");  

    ok( $Class->_chdir( dir => $Cwd),   "Chdir back to '$Cwd'" );
    paths_are_same( File::Spec->rel2abs(cwd()), $Cwd,
                                        "   Cwd() is '$Cwd'" );
}

### test _move ###
{   ok( $Class->_move( file => $Dir, to => $Move ),
                                        "Move from '$Dir' to '$Move'" );
    ok(  -d $Move,                      "   Dir '$Move' exists" );
    ok( !-d $Dir,                       "   Dir '$Dir' no longer exists" );
    
    
    {   local $CPANPLUS::Error::ERROR_FH = output_handle();
    
        ### now try to move it somewhere it can't ###
        ok( !$Class->_move( file => $Move, to => 'inc' ),
                                        "   Impossible move detected" );
        like( CPANPLUS::Error->stack_as_string, qr/Failed to move/,
                                        "   Expected error found" );
    }
}                                                                                   
            
### test _rmdir ###
{   ok( -d $Move,                       "Dir '$Move' exists" );
    ok( $Class->_rmdir( dir => $Move ), "   Deleted dir '$Move'" );
    ok(!-d $Move,                       "   Dir '$Move' no longer exists" );
}

### _get_file_contents tests ###
{   my $contents = $Class->_get_file_contents( file => basename($0) );
    ok( $contents,                      "Got file contents" );
    like( $contents, qr/BEGIN/,         "   Proper contents found" );
    like( $contents, qr/CPANPLUS/,      "   Proper contents found" );
}
    
### _perl_version tests ###
{   my $version = $Class->_perl_version( perl => $^X );
    ok( $version,                       "Perl version found" );
    like( $version, qr/\d.\d+.\d+/,     "   Looks like a proper version" );
}    
        
### _version_to_number tests ###
{   my $map = {
        '1'     => '1',
        '1.2'   => '1.2',
        '.2'    => '.2',
        'foo'   => '0.0',
        'a.1'   => '0.0',
    };        

    while( my($try,$expect) = each %$map ) {
        my $ver = $Class->_version_to_number( version => $try );
        ok( $ver,                       "Version returned" );
        is( $ver, $expect,              "   Value as expected" );
    }         
}

### _whoami tests ###
{   sub foo { 
        my $me = $Class->_whoami; 
        ok( $me,                        "_whoami returned a result" );
        is( $me, 'foo',                 "   Value as expected" ); 
    } 

    foo();
}
        
### _mode_plus_w tests ###
{   open my $fh, ">$File" or die "Could not open $File for writing: $!";
    close $fh;
    
    ### remove perms
    ok( -e $File,               "File '$File' created" );
    ok( chmod( 000, $File ),    "   File permissions set to 000" );
    
    ok( $Class->_mode_plus_w( file => $File ),
                                "   File permissions set to +w" );
    ok( -w $File,               "   File is writable" );

    1 while unlink $File;
    
    ok( !-e $File,              "   File removed" );
}

### uri encode/decode tests    
{   my $org = 'file://foo/bar';

    my $enc = $Class->_uri_encode( uri => $org );
    
    ok( $enc,                   "String '$org' encoded" );
    like( $enc, qr/%/,          "   Contents as expected" );
    
    my $dec = $Class->_uri_decode( uri => $enc );
    ok( $dec,                   "String '$enc' decoded" );
    is( $dec, $org,             "   Decoded properly" );
}    

        
        
# Local variables:
# c-indentation-style: bsd
# c-basic-offset: 4
# indent-tabs-mode: nil
# End:
# vim: expandtab shiftwidth=4:

