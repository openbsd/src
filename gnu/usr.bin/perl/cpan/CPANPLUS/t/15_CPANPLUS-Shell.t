### the shell prints to STDOUT, so capture that here
### and we can check the output
### make sure we can find our conf.pl file
BEGIN { 
    use FindBin; 
    require "$FindBin::Bin/inc/conf.pl";
}

### this lets us capture output from the default shell
{   no warnings 'redefine';

    my $out;
    *CPANPLUS::Shell::Default::__print = sub {
        my $self = shift;
        $out .= "@_";
    };

    sub _out        { $out }
    sub _reset_out  { $out = '' }
}    

use strict;
use Test::More      'no_plan';
use CPANPLUS::Internals::Constants;

### in some subprocesses, the Term::ReadKey code will go
### balistic and die because it can't figure out terminal
### dimensions. If we add these env vars, it'll use them
### as a default and not die. Thanks to Slaven Rezic for
### reporting this.
local $ENV{'COLUMNS'} = 80 unless $ENV{'COLUMNS'};
local $ENV{'LINES'}   = 40 unless $ENV{'LINES'};

my $Conf    = gimme_conf();
my $Class   = 'CPANPLUS::Shell';
my $Default = SHELL_DEFAULT;
my $TestMod = TEST_CONF_MODULE;
my $TestAuth= TEST_CONF_AUTHOR;

 
### basic load tests
use_ok( $Class, 'Default' );
is( $Class->which,  SHELL_DEFAULT,
                                "Default shell loaded" );

### create an object
my $Shell = $Class->new( $Conf );
ok( $Shell,                     "   New object created" );
isa_ok( $Shell, $Default,       "   Object" );

### method tests
{   
    ### uri to use for /cs tests
    my $cs_path = File::Spec->rel2abs(
                        File::Spec->catfile( 
                            $FindBin::Bin,
                            TEST_CONF_CPAN_DIR,
                        )
                    );
    my $cs_uri = $Shell->backend->_host_to_uri(
                        scheme  => 'file',
                        host    => '',
                        path    => $cs_path,
                    );
     
    my $base = $Conf->get_conf('base');   

    ### XXX have to keep the list ordered, as some methods only work as 
    ### expected *after* others have run
    my @map = (
        'v'                     => qr/CPANPLUS/,
        '! $self->__print($$)'  => qr/$$/,
        '?'                     => qr/\[General\]/,
        'h'                     => qr/\[General\]/,
        's'                     => qr/Unknown type/,
        's conf'                => qr/$Default/,
        's program'             => qr/sudo/,
        's mirrors'             => do { my $re = TEST_CONF_CPAN_DIR; qr/$re/ },
        's selfupdate'          => qr/selfupdate/,
        'b'                     => qr/autobundle/,
        "a $TestAuth"           => qr/$TestAuth/,
        "m $TestMod"            => qr/$TestMod/,
        "w"                     => qr/$TestMod/,
        "r 1"                   => qr/README/,
        "r $TestMod"            => qr/README/,
        "f $TestMod"            => qr/$TestAuth/,
        "d $TestMod"            => qr/$TestMod/,
        ### XXX this one prints to stdout in a subprocess -- skipping this
        ### for now due to possible PERL_CORE issues
        #"t $TestMod"            => qr/$TestMod.*tested successfully/i,
        "l $TestMod"            => qr/$TestMod/,
        '! die $$; p'           => qr/$$/,
        '/plugins'              => qr/Available plugins:/i,
        '/? ?'                  => qr/usage/i,
        
        ### custom source plugin tests
        ### lower case path matching, as on VMS we can't predict case
        "/? cs"                  => qr|/cs|,
        "/cs --add $cs_uri"      => qr/Added remote source/,
        "/cs --list"             => do { my $re = quotemeta($cs_uri); qr/$re/i },
        "/cs --contents $cs_uri" => qr/$TestAuth/i,
        "/cs --update"           => qr/Updated remote sources/,
        "/cs --update $cs_uri"   => qr/Updated remote sources/,

        ### --write leaves a file that we should clean up, so make
        ### sure it's in the path that we clean up already anyway
        "/cs --write $base"      => qr/Wrote remote source index/,
        "/cs --remove $cs_uri"   => qr/Removed remote source/,
    );

    my $meth = 'dispatch_on_input';
    can_ok( $Shell, $meth );
    
    while( my($input,$out_re) = splice(@map, 0, 2) ) {

        ### empty output cache
        __PACKAGE__->_reset_out;
        CPANPLUS::Error->flush;
        
        ok( 1,                  "Testing '$input'" );
        $Shell->$meth( input => $input );
        
        my $out = __PACKAGE__->_out;
        
        ### XXX remove me
        #diag( $out );
        
        ok( $out,               "   Output received" );
        like( $out, $out_re,    "   Output matches '$out_re'" );
    }
}

__END__

#### test seperately, they have side effects     
'q'                     => qr/^$/,          # no output!
's save boxed'          => do { my $re = CONFIG_BOXED;       qr/$re/ },        
### this doens't write any output 
'x --update_source'     => qr/module tree/i,
s edit
s reconfigure
'c'     => '_reports',    
'i'     => '_install',     
'u'     => '_uninstall',
'z'     => '_shell',
### might not have any out of date modules...
'o'     => '_uptodate',

    
