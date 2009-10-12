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

{
    my $test = "uploadInfo: basic test";
    my $fh = $q->upload('300x300_gif');
    is( $q->uploadInfo($fh)->{'Content-Type'}, "image/gif", $test);
}

my $q2 = CGI->new;

{
    my $test = "uploadInfo: works with second object instance";
    my $fh = $q2->upload('300x300_gif');
    is( $q2->uploadInfo($fh)->{'Content-Type'}, "image/gif", $test);
}

