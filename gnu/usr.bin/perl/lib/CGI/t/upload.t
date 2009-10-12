#!/usr/local/bin/perl -w

#################################################################
#  Emanuele Zeppieri, Mark Stosberg                             #
#  Shamelessly stolen from Data::FormValidator and CGI::Upload  #
#################################################################

# Due to a bug in older versions of MakeMaker & Test::Harness, we must
# ensure the blib's are in @INC, else we might use the core CGI.pm

my $test_file;
if($ENV{PERL_CORE}) {
   chdir 't';
   @INC = '../lib';
   use File::Spec ();
   $test_file = File::Spec->catfile(qw(.. lib CGI t), "upload_post_text.txt");
} else {
   use lib qw(. ./blib/lib ./blib/arch);
   $test_file = "t/upload_post_text.txt";
}

use strict;

use Test::More 'no_plan';

use CGI;

#-----------------------------------------------------------------------------
# %ENV setup.
#-----------------------------------------------------------------------------

my %myenv;

BEGIN {
    %myenv = (
        'SCRIPT_NAME'       => '/test.cgi',
        'SERVER_NAME'       => 'perl.org',
        'HTTP_CONNECTION'   => 'TE, close',
        'REQUEST_METHOD'    => 'POST',
        'SCRIPT_URI'        => 'http://www.perl.org/test.cgi',
        'CONTENT_LENGTH'    => 3285,
        'SCRIPT_FILENAME'   => '/home/usr/test.cgi',
        'SERVER_SOFTWARE'   => 'Apache/1.3.27 (Unix) ',
        'HTTP_TE'           => 'deflate,gzip;q=0.3',
        'QUERY_STRING'      => '',
        'REMOTE_PORT'       => '1855',
        'HTTP_USER_AGENT'   => 'Mozilla/5.0 (compatible; Konqueror/2.1.1; X11)',
        'SERVER_PORT'       => '80',
        'REMOTE_ADDR'       => '127.0.0.1',
        'CONTENT_TYPE'      => 'multipart/form-data; boundary=xYzZY',
        'SERVER_PROTOCOL'   => 'HTTP/1.1',
        'PATH'              => '/usr/local/bin:/usr/bin:/bin',
        'REQUEST_URI'       => '/test.cgi',
        'GATEWAY_INTERFACE' => 'CGI/1.1',
        'SCRIPT_URL'        => '/test.cgi',
        'SERVER_ADDR'       => '127.0.0.1',
        'DOCUMENT_ROOT'     => '/home/develop',
        'HTTP_HOST'         => 'www.perl.org'
    );

    for my $key (keys %myenv) {
        $ENV{$key} = $myenv{$key};
    }
}

END {
    for my $key (keys %myenv) {
        delete $ENV{$key};
    }
}

#-----------------------------------------------------------------------------
# Simulate the upload (really, multiple uploads contained in a single stream).
#-----------------------------------------------------------------------------

my $q;

{
    local *STDIN;
    open STDIN, "< $test_file"
        or die 'missing test file t/upload_post_text.txt';
    binmode STDIN;
    $q = CGI->new;
}

#-----------------------------------------------------------------------------
# Check that the file names retrieved by CGI are correct.
#-----------------------------------------------------------------------------

is( $q->param('does_not_exist_gif'), 'does_not_exist.gif', 'filename_2' );
is( $q->param('100;100_gif')       , '100;100.gif'       , 'filename_3' );
is( $q->param('300x300_gif')       , '300x300.gif'       , 'filename_4' );

{ 
    my $test = "multiple file names are handled right with same-named upload fields";
    my @hello_names = $q->param('hello_world');
    is ($hello_names[0],'goodbye_world.txt',$test. "...first file");
    is ($hello_names[1],'hello_world.txt',$test. "...second file");
}

#-----------------------------------------------------------------------------
# Now check that the upload method works.
#-----------------------------------------------------------------------------

ok( defined $q->upload('does_not_exist_gif'), 'upload_basic_2' );
ok( defined $q->upload('100;100_gif')       , 'upload_basic_3' );
ok( defined $q->upload('300x300_gif')       , 'upload_basic_4' );

{
    my $test = "file handles have expected length for multi-valued field. ";
    my ($goodbye_fh,$hello_fh) = $q->upload('hello_world');

        # Go to end of file;
        seek($goodbye_fh,0,2);
        # How long is the file?
        is(tell($goodbye_fh), 15, "$test..first file");

        # Go to end of file;
        seek($hello_fh,0,2);
        # How long is the file?
        is(tell($hello_fh), 13, "$test..second file");

}



{
    my $test = "300x300_gif has expected length";
    my $fh1 = $q->upload('300x300_gif');
    is(tell($fh1), 0, "First object: filehandle starts with position set at zero");

    # Go to end of file;
    seek($fh1,0,2);
    # How long is the file?
    is(tell($fh1), 1656, $test);
}

my $q2 = CGI->new;

{
    my $test = "Upload filehandles still work after calling CGI->new a second time";
    $q->param('new','zoo');

    is($q2->param('new'),undef, 
        "Reality Check: params set in one object instance don't appear in another instance");

    my $fh2 = $q2->upload('300x300_gif');
        is(tell($fh2), 0, "...so the state of a file handle shouldn't be carried to a new object instance, either.");
        # Go to end of file;
        seek($fh2,0,2);
        # How long is the file?
        is(tell($fh2), 1656, $test);
}

{
    my $test = "multi-valued uploads are reset properly";
    my ($dont_care, $hello_fh2) = $q2->upload('hello_world');
    is(tell($hello_fh2), 0, $test);
}

# vim: nospell
