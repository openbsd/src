package CGI::Push;

# See the bottom of this file for the POD documentation.  Search for the
# string '=head'.

# You can run this file through either pod2man or pod2html to produce pretty
# documentation in manual or html file format (these utilities are part of the
# Perl 5 distribution).

# Copyright 1995,1996, Lincoln D. Stein.  All rights reserved.
# It may be used and modified freely, but I do request that this copyright
# notice remain attached to the file.  You may modify this module as you 
# wish, but if you redistribute a modified version, please attach a note
# listing the modifications you have made.

# The most recent version and complete docs are available at:
#   http://www.genome.wi.mit.edu/ftp/pub/software/WWW/cgi_docs.html
#   ftp://ftp-genome.wi.mit.edu/pub/software/WWW/

$CGI::Push::VERSION='1.00';
use CGI;
@ISA = ('CGI');

# add do_push() to exported tags
push(@{$CGI::EXPORT_TAGS{':standard'}},'do_push');

sub do_push {
    my ($self,@p) = CGI::self_or_CGI(@_);

    # unbuffer output
    $| = 1;
    srand;
    my ($random) = rand()*1E16;
    my ($boundary) = "----------------------------------$random";

    my (@header);
    my ($type,$callback,$delay,$last_page,$cookie,$target,$expires,@other) =
	$self->rearrange([TYPE,NEXT_PAGE,DELAY,LAST_PAGE,[COOKIE,COOKIES],TARGET,EXPIRES],@p);
    $type = 'text/html' unless $type;
    $callback = \&simple_counter unless $callback && ref($callback) eq 'CODE';
    $delay = 1 unless defined($delay);

    my(@o);
    foreach (@other) { push(@o,split("=")); }
    push(@o,'-Target'=>$target) if defined($target);
    push(@o,'-Cookie'=>$cookie) if defined($cookie);
    push(@o,'-Type'=>"multipart/x-mixed-replace; boundary=$boundary");
    push(@o,'-Server'=>"CGI.pm Push Module");
    push(@o,'-Status'=>'200 OK');
    push(@o,'-nph'=>1);
    print $self->header(@o);
    print "${boundary}$CGI::CRLF";
    
    # now we enter a little loop
    my @contents;
    while (1) {
	last unless (@contents = &$callback($self,++$COUNTER)) && defined($contents[0]);
	print "Content-type: ${type}$CGI::CRLF$CGI::CRLF";
	print @contents,"$CGI::CRLF";
	print "${boundary}$CGI::CRLF";
	do_sleep($delay) if $delay;
    }
    print "Content-type: ${type}$CGI::CRLF$CGI::CRLF",
          &$last_page($self,++$COUNTER),
          "$CGI::CRLF${boundary}$CGI::CRLF"
	      if $last_page && ref($last_page) eq 'CODE';
}

sub simple_counter {
    my ($self,$count) = @_;
    return (
	    CGI->start_html("CGI::Push Default Counter"),
	    CGI->h1("CGI::Push Default Counter"),
	    "This page has been updated ",CGI->strong($count)," times.",
	    CGI->hr(),
	    CGI->a({'-href'=>'http://www.genome.wi.mit.edu/ftp/pub/software/WWW/cgi_docs.html'},'CGI.pm home page'),
	    CGI->end_html
	    );
}

sub do_sleep {
    my $delay = shift;
    if ( ($delay >= 1) && ($delay!~/\./) ){
	sleep($delay);
    } else {
	select(undef,undef,undef,$delay);
    }
}

1;

=head1 NAME

CGI::Push - Simple Interface to Server Push

=head1 SYNOPSIS

    use CGI::Push qw(:standard);

    do_push(-next_page=>\&next_page,
            -last_page=>\&last_page,
            -delay=>0.5);

    sub next_page {
        my($q,$counter) = @_;
        return undef if $counter >= 10;
        return start_html('Test'),
  	       h1('Visible'),"\n",
               "This page has been called ", strong($counter)," times",
               end_html();
      }

     sub last_page {
	 my($q,$counter) = @_;
         return start_html('Done'),
                h1('Finished'),
                strong($counter),' iterations.',
                end_html;
     }

=head1 DESCRIPTION

CGI::Push is a subclass of the CGI object created by CGI.pm.  It is
specialized for server push operations, which allow you to create
animated pages whose content changes at regular intervals.

You provide CGI::Push with a pointer to a subroutine that will draw
one page.  Every time your subroutine is called, it generates a new
page.  The contents of the page will be transmitted to the browser
in such a way that it will replace what was there beforehand.  The
technique will work with HTML pages as well as with graphics files, 
allowing you to create animated GIFs.

=head1 USING CGI::Push

CGI::Push adds one new method to the standard CGI suite, do_push().
When you call this method, you pass it a reference to a subroutine
that is responsible for drawing each new page, an interval delay, and
an optional subroutine for drawing the last page.  Other optional
parameters include most of those recognized by the CGI header()
method.

You may call do_push() in the object oriented manner or not, as you
prefer:

    use CGI::Push;
    $q = new CGI::Push;
    $q->do_push(-next_page=>\&draw_a_page);

        -or-

    use CGI::Push qw(:standard);
    do_push(-next_page=>\&draw_a_page);

Parameters are as follows:

=over 4

=item -next_page

    do_push(-next_page=>\&my_draw_routine);

This required parameter points to a reference to a subroutine responsible for
drawing each new page.  The subroutine should expect two parameters
consisting of the CGI object and a counter indicating the number
of times the subroutine has been called.  It should return the
contents of the page as an B<array> of one or more items to print.  
It can return a false value (or an empty array) in order to abort the
redrawing loop and print out the final page (if any)

    sub my_draw_routine {
        my($q,$counter) = @_;
        return undef if $counter > 100;
        return start_html('testing'),
               h1('testing'),
	       "This page called $counter times";
    }

=item -last_page

This optional parameter points to a reference to the subroutine
responsible for drawing the last page of the series.  It is called
after the -next_page routine returns a false value.  The subroutine
itself should have exactly the same calling conventions as the
-next_page routine.

=item -type

This optional parameter indicates the content type of each page.  It
defaults to "text/html".  Currently, server push of heterogeneous
document types is not supported.

=item -delay

This indicates the delay, in seconds, between frames.  Smaller delays
refresh the page faster.  Fractional values are allowed.

B<If not specified, -delay will default to 1 second>

=item -cookie, -target, -expires

These have the same meaning as the like-named parameters in
CGI::header().

=back

=head1 INSTALLING CGI::Push SCRIPTS

Server push scripts B<must> be installed as no-parsed-header (NPH)
scripts in order to work correctly.  On Unix systems, this is most
often accomplished by prefixing the script's name with "nph-".  
Recognition of NPH scripts happens automatically with WebSTAR and 
Microsoft IIS.  Users of other servers should see their documentation
for help.

=head1 CAVEATS

This is a new module.  It hasn't been extensively tested.

=head1 AUTHOR INFORMATION

be used and modified freely, but I do request that this copyright
notice remain attached to the file.  You may modify this module as you
wish, but if you redistribute a modified version, please attach a note
listing the modifications you have made.

Address bug reports and comments to:
lstein@genome.wi.mit.edu

=head1 BUGS

This section intentionally left blank.

=head1 SEE ALSO

L<CGI::Carp>, L<CGI>

=cut

