#!/usr/local/bin/perl

use Config;
use File::Basename qw(&basename &dirname);

# List explicitly here the variables you want Configure to
# generate.  Metaconfig only looks for shell variables, so you
# have to mention them as if they were shell variables, not
# %Config entries.  Thus you write
#  $startperl
# to ensure Configure will look for $Config{startperl}.

# This forces PL files to create target in same directory as PL file.
# This is so that make depend always knows where to find PL derivatives.
chdir(dirname($0));
($file = basename($0)) =~ s/\.PL$//;
$file =~ s/\.pl$//
	if ($^O eq 'VMS' or $^O eq 'os2');  # "case-forgiving"

open OUT,">$file" or die "Can't create $file: $!";

print "Extracting $file (with variable substitutions)\n";

# In this section, perl variables will be expanded during extraction.
# You can use $Config{...} to use Configure variables.

print OUT <<"!GROK!THIS!";
$Config{'startperl'}
!GROK!THIS!

# In the following, perl variables are not expanded during extraction.

print OUT <<'!NO!SUBS!';
eval 'exec perl -S $0 ${1+"$@"}'
        if $running_under_some_shell;
#
# pod2html - convert pod format to html
# Version 1.15
# usage: pod2html [podfiles]
# Will read the cwd and parse all files with .pod extension
# if no arguments are given on the command line.
#
# Many helps, suggestions, and fixes from the perl5 porters, and all over.
# Bill Middleton - wjm@metronet.com
#
# Please send patches/fixes/features to me
#
#
# 
*RS = */;
*ERRNO = *!;

################################################################################
# Invoke with various levels of debugging possible
################################################################################
while ($ARGV[0] =~ /^-d(.*)/) {
    shift;
    $Debug{ lc($1 || shift) }++;
}

# ck for podnames on command line
while ($ARGV[0]) {
    push(@Pods,shift);
}

################################################################################
# CONFIGURE
#
# The beginning of the url for the anchors to the other sections.
# Edit $type to suit.  It's configured for relative url's now.
# Other possibilities are:
# $type = '<A HREF="file://localhost/usr/local/htmldir/'; # file url
# $type = '<A HREF="http://www.bozo.com/perl/manual/html/' # server
#
################################################################################

$type = '<A HREF="';		
$dir = ".";             # location of pods

# look in these pods for things not found within the current pod
# be careful tho, namespace collisions cause stupid links

@inclusions = qw[
     perlfunc perlvar perlrun perlop 
];
################################################################################
# END CONFIGURE
################################################################################

$A = {};  # The beginning of all things

unless (@Pods) {
    opendir(DIR,$dir)  or  die "Can't opendir $dir: $ERRNO";
    @Pods = grep(/\.pod$/,readdir(DIR));
    closedir(DIR) or die "Can't closedir $dir: $ERRNO";
}
@Pods or die "aak, expected pods";

# loop twice through the pods, first to learn the links, then to produce html
for $count (0,1) {
    print STTDER "Scanning pods...\n" unless $count;
    foreach $podfh ( @Pods ) {
	($pod = $podfh) =~ s/\.pod$//;
	Debug("files", "opening 2 $podfh" );
	print "Creating $pod.html from $podfh\n" if $count;
	$RS = "\n=";         # grok pods by item (Nonstandard but effecient)
	open($podfh,"<".$podfh)  || die "can't open $podfh: $ERRNO";
	@all = <$podfh>;
	close($podfh);
	$RS = "\n";

	$all[0] =~ s/^=//;
	for (@all) { s/=$// }
	$Podnames{$pod} = 1;
	$in_list = 0;
	$html = $pod.".html";
	if ($count) {              # give us a html and rcs header
	    open(HTML,">$html") || die "can't create $html: $ERRNO";
	    print HTML '<!-- $Id$ -->',"\n",'<HTML><HEAD>',"\n";
	    print HTML "<CENTER>" unless $NO_NS;
	    print HTML "<TITLE>$pod</TITLE>\n</HEAD>\n<BODY>";
	    print HTML "</CENTER>" unless $NO_NS;
	}
	for ($i = 0; $i <= $#all; $i++) {       # decide what to do with each chunk
	    $all[$i] =~ /^(\w+)\s*(.*)\n?([^\0]*)$/ ;
	    ($cmd, $title, $rest) = ($1,$2,$3);
	    if ($cmd eq "item") {
		if ($count ) { # producing html
		    do_list("over",$all[$i],\$in_list,\$depth) unless $depth;
		    do_item($title,$rest,$in_list);
		}
		else {  
		    # scan item
		    scan_thing("item",$title,$pod);
		}
	    }
	    elsif ($cmd =~ /^head([12])/) {
		$num = $1;
		if ($count) { # producing html
		    do_hdr($num,$title,$rest,$depth);
		}
		else {
		    # header scan
		    scan_thing($cmd,$title,$pod); # skip head1
		}
	    }
	    elsif ($cmd =~ /^over/) {
		$count and $depth and do_list("over",$all[$i+1],\$in_list,\$depth);
	    }
	    elsif ($cmd =~ /^back/) {
		if ($count) {  # producing html
		    ($depth) or next; # just skip it
		    do_list("back",$all[$i+1],\$in_list,\$depth);
		    do_rest("$title.$rest");
		}
	    }
	    elsif ($cmd =~ /^cut/) {
		next;
	    }
            elsif ($cmd =~ /^for/) {  # experimental pragma html
                if ($count) {  # producing html
                    if ($title =~ s/^html//) {
                        $in_html =1;
                        do_rest("$title.$rest");
                    }
                }
            }
            elsif ($cmd =~ /^begin/) {  # experimental pragma html
                if ($count) {  # producing html
                    if ($title =~ s/^html//) {
                        print HTML $title,"\n",$rest;
                    }
                    elsif ($title =~ /^end/) {
                        next;
                    }
                }
            }
	    elsif ($Debug{"misc"}) { 
		warn("unrecognized header: $cmd");
	    }
	}
        # close open lists without '=back' stmts
	if ($count) {  # producing html
	    while ($depth) {
		 do_list("back",$all[$i+1],\$in_list,\$depth);
	    }
	    print HTML "\n</BODY>\n</HTML>\n";
	}
    }
}

sub do_list{   # setup a list type, depending on some grok logic
    my($which,$next_one,$list_type,$depth) = @_;
    my($key);
    if ($which eq "over") {
	unless ($next_one =~ /^item\s+(.*)/) {
	    warn "Bad list, $1\n" if $Debug{"misc"};
	}
	$key = $1;

	if      ($key =~ /^1\.?/) {
	    $$list_type = "OL";
	} elsif ($key =~ /\*\s*$/) {
	    $$list_type = "UL";
	} elsif ($key =~ /\*?\s*\w/) {
	    $$list_type = "DL";
	} else {
	    warn "unknown list type for item $key" if $Debug{"misc"};
	}

	print HTML qq{\n};
	print HTML $$list_type eq 'DL' ? qq{<DL COMPACT>} : qq{<$$list_type>};
	$$depth++;
    }
    elsif ($which eq "back") {
	print HTML qq{\n</$$list_type>\n};
	$$depth--;
    }
}

sub do_hdr{   # headers
    my($num,$title,$rest,$depth) = @_;
    print HTML qq{<p><hr>\n} if $num == 1;
    process_thing(\$title,"NAME");
    print HTML qq{\n<H$num> };
    print HTML $title; 
    print HTML qq{</H$num>\n};
    do_rest($rest);
}

sub do_item{  # list items
    my($title,$rest,$list_type) = @_;
    my $bullet_only = $title eq '*' and $list_type eq 'UL';
    process_thing(\$title,"NAME");
    if ($list_type eq "DL") {
	print HTML qq{\n<DT><STRONG>\n};
	print HTML $title; 
	print HTML qq{\n</STRONG>\n};
	print HTML qq{<DD>\n};
    }
    else {
	print HTML qq{\n<LI>};
	unless ($bullet_only or $list_type eq "OL") {
	    print HTML $title,"\n";
	}
    }
    do_rest($rest);
}

sub do_rest{   # the rest of the chunk handled here
    my($rest) = @_;
    my(@lines,$p,$q,$line,,@paras,$inpre);
    @paras = split(/\n\n\n*/,$rest);  
    for ($p = 0; $p <= $#paras; $p++) {
	$paras[$p] =~ s/^\n//mg;
	@lines = split(/\n/,$paras[$p]);
        if ($in_html) {  # handle =for html paragraphs
            print HTML $paras[0];
            $in_html = 0;
            next;
        }
	elsif ($lines[0] =~ /^\s+\w*\t.*/) {  # listing or unordered list
	    print HTML qq{<UL>};
	    foreach $line (@lines) { 
		($line =~ /^\s+(\w*)\t(.*)/) && (($key,$rem) = ($1,$2));
		print HTML defined($Podnames{$key}) 
				?  "<LI>$type$key.html\">$key<\/A>\t$rem</LI>\n" 
				: "<LI>$line</LI>\n";
	    }
	    print HTML qq{</UL>\n};
	}
	elsif ($lines[0] =~ /^\s/) {       # preformatted code
	    if ($paras[$p] =~/>>|<</) {
		print HTML qq{\n<PRE>\n};
		$inpre=1;
	    }
	    else {                         # Still cant beat XMP.  Yes, I know 
		print HTML qq{\n<XMP>\n}; # it's been obsoleted... suggestions?
		$inpre = 0;
	    }
	    while (defined($paras[$p])) {
	        @lines = split(/\n/,$paras[$p]);
		foreach $q (@lines) {      # mind your p's and q's here :-)
		    if ($paras[$p] =~ />>|<</) {
			if ($inpre) {
			    process_thing(\$q,"HTML");
			}
			else {
			    print HTML qq{\n</XMP>\n};
			    print HTML qq{<PRE>\n};
			    $inpre=1;
			    process_thing(\$q,"HTML");
			}
		    }
		    1 while $q =~  s/\t+/' 'x (length($&) * 8 - length($`) % 8)/e;
		    print HTML  $q,"\n";
		}
		last if $paras[$p+1] !~ /^\s/;
		$p++;
	    }
	    print HTML ($inpre==1) ? (qq{\n</PRE>\n}) : (qq{\n</XMP>\n});
	}
	else {                             # other text
	    @lines = split(/\n/,$paras[$p]);
	    foreach $line (@lines) {
                process_thing(\$line,"HTML");
		print HTML qq{$line\n};
	    }
	}
	print HTML qq{<p>};
    }
}

sub process_thing{       # process a chunk, order important
    my($thing,$htype) = @_;
    pre_escapes($thing);
    find_refs($thing,$htype);
    post_escapes($thing);
}

sub scan_thing{           # scan a chunk for later references
    my($cmd,$title,$pod) = @_;
    $_ = $title;
    s/\n$//;
    s/E<(.*?)>/&$1;/g;
    # remove any formatting information for the headers
    s/[SFCBI]<(.*?)>/$1/g;         
    # the "don't format me" thing
    s/Z<>//g;
    if ($cmd eq "item") {
        /^\*/ and  return; 	# skip bullets
        /^\d+\./ and  return; 	# skip numbers
        s/(-[a-z]).*/$1/i;
	trim($_);
        return if defined $A->{$pod}->{"Items"}->{$_};
        $A->{$pod}->{"Items"}->{$_} = gensym($pod, $_);
        $A->{$pod}->{"Items"}->{(split(' ',$_))[0]}=$A->{$pod}->{"Items"}->{$_};
        Debug("items", "item $_");
        if (!/^-\w$/ && /([%\$\@\w]+)/ && $1 ne $_ 
    	    && !defined($A->{$pod}->{"Items"}->{$_}) && ($_ ne $1)) 
        {
    	    $A->{$pod}->{"Items"}->{$1} = $A->{$pod}->{"Items"}->{$_};
    	    Debug("items", "item $1 REF TO $_");
        } 
        if ( m{^(tr|y|s|m|q[qwx])/.*[^/]} ) {
    	    my $pf = $1 . '//';
    	    $pf .= "/" if $1 eq "tr" || $1 eq "y" || $1 eq "s";
    	    if ($pf ne $_) {
    	        $A->{$pod}->{"Items"}->{$pf} = $A->{$pod}->{"Items"}->{$_};
    	        Debug("items", "item $pf REF TO $_");
    	    }
	}
    }
    elsif ($cmd =~ /^head[12]/) {                
        return if defined($A->{$pod}->{"Headers"}->{$_});
        $A->{$pod}->{"Headers"}->{$_} = gensym($pod, $_);
        Debug("headers", "header $_");
    } 
    else {
        warn "unrecognized header: $cmd" if $Debug;
    } 
}


sub picrefs { 
    my($char, $bigkey, $lilkey,$htype) = @_;
    my($key,$ref,$podname);
    for $podname ($pod,@inclusions) {
	for $ref ( "Items", "Headers" ) {
	    if (defined $A->{$podname}->{$ref}->{$bigkey}) {
		$value = $A->{$podname}->{$ref}->{$key = $bigkey};
		Debug("subs", "bigkey is $bigkey, value is $value\n");
	    } 
	    elsif (defined $A->{$podname}->{$ref}->{$lilkey}) {
		$value = $A->{$podname}->{$ref}->{$key = $lilkey};
		return "" if $lilkey eq '';
		Debug("subs", "lilkey is $lilkey, value is $value\n");
	    } 
	} 
	if (length($key)) {
            ($pod2,$num) = split(/_/,$value,2);
	    if ($htype eq "NAME") {  
		return "\n<A NAME=\"".$value."\">\n$bigkey</A>\n"
	    }
	    else {
		return "\n$type$pod2.html\#".$value."\">$bigkey<\/A>\n";
	    }
	} 
    }
    if ($char =~ /[IF]/) {
	return "<EM>$bigkey</EM>";
    } elsif ($char =~ /C/) {
	return "<CODE>$bigkey</CODE>";
    } else {
	return "<STRONG>$bigkey</STRONG>";
    }
} 

sub find_refs { 
    my($thing,$htype) = @_;
    my($orig) = $$thing;
    # LREF: a manpage(3f) we don't know about
    for ($$thing) {
	#s:L<([a-zA-Z][^\s\/]+)(\([^\)]+\))>:the I<$1>$2 manpage:g;
        s@(\S+?://\S*[^.,;!?\s])@noremap(qq{<A HREF="$1">$1</A>})@ge;
        s,([a-z0-9_.-]+\@([a-z0-9_-]+\.)+([a-z0-9_-]+)),noremap(qq{<A HREF="MAILTO:$1">$1</A>}),gie;
	s/L<([^>]*)>/lrefs($1,$htype)/ge;
	s/([CIBF])<(\W*?(-?\w*).*?)>/picrefs($1, $2, $3, $htype)/ge;
	s/(S)<([^\/]\W*?(-?\w*).*?)>/picrefs($1, $2, $3, $htype)/ge;
	s/((\w+)\(\))/picrefs("I", $1, $2,$htype)/ge;
	s/([\$\@%](?!&[gl]t)([\w:]+|\W\b))/varrefs($1,$htype)/ge;
    }
    if ($$thing eq $orig && $htype eq "NAME") { 
	$$thing = picrefs("I", $$thing, "", $htype);
    }

}

sub lrefs {
    my($page, $item) = split(m#/#, $_[0], 2);
    my($htype) = $_[1];
    my($podname);
    my($section) = $page =~ /\((.*)\)/;
    my $selfref;
    if ($page =~ /^[A-Z]/ && $item) {
	$selfref++;
	$item = "$page/$item";
	$page = $pod;
    }  elsif (!$item && $page =~ /[^a-z\-]/ && $page !~ /^\$.$/) {
	$selfref++;
	$item = $page;
	$page = $pod;
    } 
    $item =~ s/\(\)$//;
    if (!$item) {
    	if (!defined $section && defined $Podnames{$page}) {
	    return "\n$type$page.html\">\nthe <EM>$page</EM> manpage<\/A>\n";
	} else {
	    (warn "Bizarre entry $page/$item") if $Debug;
	    return "the <EM>$_[0]</EM>  manpage\n";
	} 
    } 

    if ($item =~ s/"(.*)"/$1/ || ($item =~ /[^\w\/\-]/ && $item !~ /^\$.$/)) {
	$text = "<EM>$item</EM>";
	$ref = "Headers";
    } else {
	$text = "<EM>$item</EM>";
	$ref = "Items";
    } 
    for $podname ($pod, @inclusions) {
	undef $value;
	if ($ref eq "Items") {
	    if (defined($value = $A->{$podname}->{$ref}->{$item})) {
		($pod2,$num) = split(/_/,$value,2);
		return (($pod eq $pod2) && ($htype eq "NAME"))
	    	? "\n<A NAME=\"".$value."\">\n$text</A>\n"
	    	: "\n$type$pod2.html\#".$value."\">$text<\/A>\n";
            }
        } 
	elsif ($ref eq "Headers") {
	    if (defined($value = $A->{$podname}->{$ref}->{$item})) {
		($pod2,$num) = split(/_/,$value,2);
		return (($pod eq $pod2) && ($htype eq "NAME")) 
	    	? "\n<A NAME=\"".$value."\">\n$text</A>\n"
	    	: "\n$type$pod2.html\#".$value."\">$text<\/A>\n";
            }
	}
    }
    warn "No $ref reference for $item (@_)" if $Debug;
    return $text;
} 

sub varrefs {
    my ($var,$htype) = @_;
    for $podname ($pod,@inclusions) {
	if ($value = $A->{$podname}->{"Items"}->{$var}) {
	    ($pod2,$num) = split(/_/,$value,2);
	    Debug("vars", "way cool -- var ref on $var");
	    return (($pod eq $pod2) && ($htype eq "NAME"))  # INHERIT $_, $pod
		? "\n<A NAME=\"".$value."\">\n$var</A>\n"
		: "\n$type$pod2.html\#".$value."\">$var<\/A>\n";
	}
    }
    Debug( "vars", "bummer, $var not a var");
    return "<STRONG>$var</STRONG>";
} 

sub gensym {
    my ($podname, $key) = @_;
    $key =~ s/\s.*//;
    ($key = lc($key)) =~ tr/a-z/_/cs;
    my $name = "${podname}_${key}_0";
    $name =~ s/__/_/g;
    while ($sawsym{$name}++) {
        $name =~ s/_?(\d+)$/'_' . ($1 + 1)/e;
    }
    return $name;
} 

sub pre_escapes {  # twiddle these, and stay up late  :-)
    my($thing) = @_;
    for ($$thing) { 
	s/"(.*?)"/``$1''/gs;
	s/&/noremap("&amp;")/ge;
	s/<</noremap("&lt;&lt;")/eg;
	s/([^ESIBLCF])</$1\&lt\;/g;
	s/E<([^\/][^<>]*)>/\&$1\;/g;              # embedded special
    }
}
sub noremap {   # adding translator for hibit chars soon
    my $hide = $_[0];
    $hide =~ tr/\000-\177/\200-\377/;  
    $hide;
} 


sub post_escapes {
    my($thing) = @_;
    for ($$thing) {
	s/([^GM])>>/$1\&gt\;\&gt\;/g;
	s/([^D][^"MGA])>/$1\&gt\;/g;
	tr/\200-\377/\000-\177/;
    }
}

sub Debug {
    my $level = shift;
    print STDERR @_,"\n" if $Debug{$level};
} 

sub dumptable  {
    my $t = shift;
    print STDERR "TABLE DUMP $t\n";
    foreach $k (sort keys %$t) {
	printf STDERR "%-20s <%s>\n", $t->{$k}, $k;
    } 
} 
sub trim {
    for (@_) {
        s/^\s+//;
        s/\s\n?$//;
    }
}
!NO!SUBS!

close OUT or die "Can't close $file: $!";
chmod 0755, $file or die "Can't reset permissions for $file: $!\n";
exec("$Config{'eunicefix'} $file") if $Config{'eunicefix'} ne ':';
