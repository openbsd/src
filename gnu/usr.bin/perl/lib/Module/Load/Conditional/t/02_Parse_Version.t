BEGIN { chdir 't' if -d 't' }

use strict;
use lib         qw[../lib];
use Test::More  'no_plan';

my $Class   = 'Module::Load::Conditional';
my $Meth    = '_parse_version';
my $Verbose = @ARGV ? 1 : 0;

use_ok( $Class );

### versions that should parse
{   for my $str ( __PACKAGE__->_succeed ) {
        my $res = $Class->$Meth( $str, $Verbose );
        ok( defined $res,       "String '$str' identified as version string" );
        
        ### XXX version.pm 0.69 pure perl fails tests under 5.6.2.
        ### XXX version.pm <= 0.69 do not have a complete overload 
        ### implementation, which causes the following error:
        ### $ perl -Mversion -le'qv(1)+0'
        ### Operation "+": no method found,
        ###        left argument in overloaded package version,
        ###        right argument has no overloaded magic at -e line 1
        ### so we do the comparison ourselves, and then feed it to
        ### the Test::More::ok().
        ###
        ### Mailed jpeacock and p5p about both issues on 25-1-2007:
        ###     http://xrl.us/uem7
        ###     (http://www.xray.mpe.mpg.de/mailing-lists/
        ###         perl5-porters/2007-01/msg00805.html)

        ### Quell "Argument isn't numeric in gt" warnings...
        my $bool = do { local $^W; $res > 0 };
        
        ok( $bool,              "   Version is '$res'" );
        isnt( $res, '0.0',      "   Not the default value" );
    }             
}

### version that should fail
{   for my $str ( __PACKAGE__->_fail ) {
        my $res = $Class->$Meth( $str, $Verbose );
        ok( ! defined $res,     "String '$str' is not a version string" );
    }
}    


################################
###
### VERSION declarations to test
###
################################

sub _succeed {
    return grep { /\S/ } map { s/^\s*//; $_ } split "\n", q[
        $VERSION = 1;
        *VERSION = \'1.01';
        use version; $VERSION = qv('0.0.2');
        use version; $VERSION = qv('3.0.14');
        ($VERSION) = '$Revision: 2.03 $' =~ /\s(\d+\.\d+)\s/; 
        ( $VERSION ) = sprintf "%d.%02d", q$Revision: 1.23 $ =~ m/ (\d+) \. (\d+) /gx;
        ($GD::Graph::area::VERSION) = '$Revision: 1.16.2.3 $' =~ /\s([\d.]+)/;
        ($GD::Graph::axestype::VERSION) = '$Revision: 1.44.2.14 $' =~ /\s([\d.]+)/;
        ($GD::Graph::colour::VERSION) = '$Revision: 1.10 $' =~ /\s([\d.]+)/;
        ($GD::Graph::pie::VERSION) = '$Revision: 1.20.2.4 $' =~ /\s([\d.]+)/;
        ($GD::Text::Align::VERSION) = '$Revision: 1.18 $' =~ /\s([\d.]+)/;
        $VERSION = qv('0.0.1');
        use version; $VERSION = qv('0.0.3');
        $VERSION = do { my @r = ( ( $v = q<Version value="0.20.1"> ) =~ /\d+/g ); sprintf "%d.%02d", $r[0], int( $r[1] / 10 ) };
        ($VERSION) = sprintf '%i.%03i', split(/\./,('$Revision: 2.0 $' =~ /Revision: (\S+)\s/)[0]); # $Date: 2005/11/16 02:16:00 $
        ( $VERSION = q($Id: Tidy.pm,v 1.56 2006/07/19 23:13:33 perltidy Exp $) ) =~ s/^.*\s+(\d+)\/(\d+)\/(\d+).*$/$1$2$3/; # all one line for MakeMaker
        ($VERSION) = q $Revision: 2.120 $ =~ /([\d.]+)/;
        ($VERSION) = q$Revision: 1.00 $ =~ /([\d.]+)/;
        $VERSION = "3.0.8";
        $VERSION = '1.0.5';
    ];
}

sub _fail {
    return grep { /\S/ } map { s/^\s*//; $_ } split "\n", q[
        use vars qw($VERSION $AUTOLOAD %ERROR $ERROR $Warn $Die);
        sub version { $GD::Graph::colour::VERSION }
        my $VERS = qr{ $HWS VERSION $HWS \n }xms;
        diag( "Testing $main_module \$${main_module}::VERSION" );
        our ( $VERSION, $v, $_VERSION );
        my $seen = { q{::} => { 'VERSION' => 1 } }; # avoid multiple scans
        eval "$module->VERSION"
        'VERSION' => '1.030' # Variable and Value
        'VERSION' => '2.121_020'
        'VERSION' => '0.050', # Standard variable $VERSION
        use vars qw( $VERSION $seq @FontDirs );
        $VERSION
        # *VERSION = \'1.01';
        # ( $VERSION ) = '$Revision: 1.56 $ ' =~ /\$Revision:\s+([^\s]+)/;
        #$VERSION = sprintf("%d.%s", map {s/_//g; $_} q$Name: $ =~ /-(\d+)_([\d_]+)/);
        #$VERSION = sprintf("%d.%s", map {s/_//g; $_} q$Name: $ =~ /-(\d+)_([\d_]+)/);
    ];
}
