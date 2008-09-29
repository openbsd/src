package Nomad;

# Suboptimal things:
#	ast type info is generally still implicit
#	the combined madness calls are actually losing type information
#	brace madprops tend to be too low in the tree
#	could use about 18 more refactorings...
#	lots of unused cruft left around from previous refactorings

use strict;
use warnings;
use Carp;

use P5AST;
use P5re;

my $deinterpolate;

sub xml_to_p5 {
    my %options = @_;


    my $filename = $options{'input'} or die;
    $deinterpolate = $options{'deinterpolate'};
    my $YAML = $options{'YAML'};

    local $SIG{__DIE__} = sub {
        my $e = shift;
        $e =~ s/\n$/\n    [NODE $filename line $::prevstate->{line}]/ if $::prevstate;
        confess $e;
    };

    # parse file
    use XML::Parser;
    my $p1 = XML::Parser->new(Style => 'Objects', Pkg => 'PLXML');
    $p1->setHandlers('Char' => sub { warn "Chars $_[1]" if $_[1] =~ /\S/; });

    # First slurp XML into tree of objects.

    my $root = $p1->parsefile($filename);

    # Now turn XML tree into something more like an AST.

    PLXML::prepreproc($root->[0]);
    my $ast = P5AST->new('Kids' => [$root->[0]->ast()]);
    #::t($ast);

    if ($YAML) {
        require YAML::Syck;
        return YAML::Syck::Dump($ast);
    }

    # Finally, walk AST to produce new program.

    my $text = $ast->p5text();	# returns encoded, must output raw
    return $text;
}

$::curstate = 0;
$::prevstate = 0;
$::curenc = 1;		# start in iso-8859-1, sigh...

$::H = "HeredocHere000";
%::H = ();

my @enc = (
    'utf-8',
    'iso-8859-1',
);

my %enc = (
    'utf-8' => 0,
    'iso-8859-1' => 1,
);

my %madtype = (
    '$' => 'p5::sigil',
    '@' => 'p5::sigil',
    '%' => 'p5::sigil',
    '&' => 'p5::sigil',
    '*' => 'p5::sigil',
    'o' => 'p5::operator',
    '~' => 'p5::operator',
    '+' => 'p5::punct',
    '?' => 'p5::punct',
    ':' => 'p5::punct',
    ',' => 'p5::punct',
    ';' => 'p5::punct',
    '#' => 'p5::punct',
    '(' => 'p5::opener',
    ')' => 'p5::closer',
    '[' => 'p5::opener',
    ']' => 'p5::closer',
    '{' => 'p5::opener',
    '}' => 'p5::closer',
    '1'	=> 'p5::punct',
    '2'	=> 'p5::punct',
    'a'	=> 'p5::operator',
    'A'	=> 'p5::operator',
    'd' => 'p5::declarator',
    'E'	=> 'p5::text',
    'L' => 'p5::label',
    'm' => 'p5::remod',
#    'n' => 'p5::name',
    'q' => 'p5::openquote',
    'Q' => 'p5::closequote',
    '='	=> 'p5::text',
    'R'	=> 'p5::text',
    's'	=> 'p5::text',
    's'	=> 'p5::declarator',
#    'V' => 'p5::version',
    'X' => 'p5::token',
);

use Data::Dumper;
$Data::Dumper::Indent = 1;
$Data::Dumper::Quotekeys = 0;

sub d {
    my $text = Dumper(@_);
    # doesn't scale well, alas
    1 while $text =~ s/(.*)^([^\n]*)bless\( \{\n(.*?)^(\s*\}), '([^']*)' \)([^\n]*)/$1$2$5 {\n$3$4$6 # $5/ms;
    $text =~ s/PLXML:://g;
    if ($text) {
	my ($package, $filename, $line) = caller;
	my $subroutine = (caller(1))[3];
	$text =~ s/\n?\z/, called from $subroutine, line $line\n/;
	warn $text;
    }
};

{

    my %xmlrepl = (
	'&' => '&amp;',
	"'" => '&apos;',
	'"' => '&dquo;',
	'<' => '&lt;',
	'>' => '&gt;',
	"\n" => '&#10;',
	"\t" => '&#9;',
    );

    sub x {
	my $indent = 0;
	if (@_ > 1) {
	    warn xdolist($indent,"LIST",@_);
	}
	else {
	    my $type = ref $_[0];
	    if ($type) {
		warn xdoitem($indent,$type,@_);
	    }
	    else {
		warn xdoitem($indent,"ITEM",@_);
	    }
	}
    }

    sub xdolist {
	my $indent = shift;
	my $tag = shift;
	my $in = ' ' x ($indent * 2);
	my $result;
	$result .= "$in<$tag>\n" if defined $tag;
	for my $it (@_) {
	    my $itt = ref $it || "ITEM";
	    $itt =~ s/::/:/g;
	    $result .= xdoitem($indent+1,$itt,$it);
	}
	$result .= "$in</$tag>\n" if defined $tag;
	return $result;
    }

    sub xdohash {
	my $indent = shift;
	my $tag = shift;
	my $hash = shift;
	my $in = ' ' x ($indent * 2);
	my $result = "$in<$tag>\n";
	my @keys = sort keys %$hash;
	my $longest = 0;
	for my $k (@keys) {
	    $longest = length($k) if length($k) > $longest;
	}
	my $K;
	for my $k (@keys) {
	    my $tmp;
	    $K = $$hash{$k}, next if $k eq 'Kids';
	    my $sp = ' ' x ($longest - length($k));
	    if (ref $$hash{$k}) {
		$tmp = xdoitem($indent+1,"kv",$$hash{$k});
		$tmp =~ s!^ *<kv>\n *</kv>!$in  <kv/>!;
	    }
	    else {
		$tmp = xdoitem($indent+1,"kv",$$hash{$k});
	    }
	    $k =~ s/([\t\n'"<>&])/$xmlrepl{$1}/g;
	    $tmp =~ s/<kv/<kv k='$k'$sp/ or
		$tmp =~ s/^(.*)$/$in  <kv k='$k'>\n$in  $1$in  <\/kv>\n/s;
	    $result .= $tmp;
	}
	if ($K and @$K) {
	    $result .= xdolist($indent, undef, @$K);
	}
	$result .= "$in</$tag>\n";
    }

    sub xdoitem {
	my $indent = shift;
	my $tag = shift;
	my $item = shift;
	my $in = ' ' x ($indent * 2);
	my $r = ref $item;
	if (not $r) {
	    $item =~ s/([\t\n'"<>&])/$xmlrepl{$1}/g;
	    return "$in<$tag>$item</$tag>\n";
	}
	(my $newtag = $r) =~ s/::/:/g;
	my $t = "$item";
	if ($t =~ /\bARRAY\b/) {
	    if (@{$item}) {
		return xdolist($indent,$tag,@{$item});
	    }
	    else {
		return "$in<$tag />\n";
	    }
	}
	if ($t =~ /\bHASH\b/) {
	    return xdohash($indent,$tag,$item);
	}
	if ($r =~ /^p5::/) {
	    return "$in<$newtag>$$item</$newtag>\n";
	}
	else {
	    return "$in<$newtag type='$r'/>\n";
	}
    }

    my %trepl = (
	"'" => '\\\'',
	'"' => '\\"',
	"\n" => '\\n',
	"\t" => '\\t',
    );

    sub t {
	my $indent = 0;
	if (@_ > 1) {
	    tdolist($indent,"LIST",@_);
	}
	else {
	    my $type = ref $_[0];
	    if ($type) {
		tdoitem($indent,$type,@_);
	    }
	    else {
		tdoitem($indent,"ITEM",@_);
	    }
	}
	print STDERR "\n";
    }

    sub tdolist {
	my $indent = shift;
	my $tag = shift || "ARRAY";
	my $in = ' ' x ($indent * 2);
	if (@_) {
	    print STDERR "[\n";
	    for my $it (@_) {
		my $itt = ref $it || "ITEM";
		print STDERR $in,"  ";
		tdoitem($indent+1,$itt,$it);
		print STDERR "\n";
	    }
	    print STDERR "$in]";
	}
	else {
	    print STDERR "[]";
	}
    }

    sub tdohash {
	my $indent = shift;
	my $tag = shift;
	my $hash = shift;
	my $in = ' ' x ($indent * 2);

	print STDERR "$tag => {\n";

	my @keys = sort keys %$hash;
	my $longest = 0;
	for my $k (@keys) {
	    $longest = length($k) if length($k) > $longest;
	}
	my $K;
	for my $k (@keys) {
	    my $sp = ' ' x ($longest - length($k));
	    print STDERR "$in  $k$sp => ";
	    tdoitem($indent+1,"",$$hash{$k});
	    if ($k eq 'Kids') {
		print STDERR " # Kids";
	    }
	    print STDERR "\n";
	}
	print STDERR "$in} # $tag";
    }

    sub tdoitem {
	my $indent = shift;
	my $tag = shift;
	my $item = shift;
	if (not defined $item) {
	    print STDERR "UNDEF";
	    return;
	}
#	my $in = ' ' x ($indent * 2);
	my $r = ref $item;
	if (not $r) {
	    $item =~ s/([\t\n"])/$trepl{$1}/g;
	    print STDERR "\"$item\"";
	    return;
	}
	my $t = "$item";
	if ($r =~ /^p5::/) {
	    my $str = $$item{uni};
	    my $enc = $enc[$$item{enc}] . ' ';
	    $enc =~ s/iso-8859-1 //;
	    $str =~ s/([\t\n"])/$trepl{$1}/g;
	    print STDERR "$r $enc\"$str\"";
	}
	elsif ($t =~ /\bARRAY\b/) {
	    tdolist($indent,$tag,@{$item});
	}
	elsif ($t =~ /\bHASH\b/) {
	    tdohash($indent,$tag,$item);
	}
	else {
	    print STDERR "$r type='$r'";
	}
    }
}

sub encnum {
    my $encname = shift;
    if (not exists $enc{$encname}) {
	push @enc, $encname;
	return $enc{$encname} = $#enc;
    }
    return $enc{$encname};
}

use PLXML;

package p5::text;

use Encode;

sub new {
    my $class = shift;
    my $text = shift;
    die "Too many args to new" if @_;
    die "Attempt to bless non-text $text" if ref $text;
    return bless( { uni => $text,
		    enc => $::curenc,
		  }, $class);
}

sub uni { my $self = shift; $$self{uni}; }	# internal stuff all in utf8

sub enc {
    my $self = shift;
    my $enc = $enc[$$self{enc} || 0];
    return encode($enc, $$self{uni});
}

package p5::closequote;	BEGIN { @p5::closequote::ISA = 'p5::punct'; }
package p5::closer;	BEGIN { @p5::closer::ISA = 'p5::punct'; }
package p5::declarator;	BEGIN { @p5::declarator::ISA = 'p5::token'; }
package p5::junk;	BEGIN { @p5::junk::ISA = 'p5::text'; }
package p5::label;	BEGIN { @p5::label::ISA = 'p5::token'; }
#package p5::name;	BEGIN { @p5::name::ISA = 'p5::token'; }
package p5::opener;	BEGIN { @p5::opener::ISA = 'p5::punct'; }
package p5::openquote;	BEGIN { @p5::openquote::ISA = 'p5::punct'; }
package p5::operator;	BEGIN { @p5::operator::ISA = 'p5::token'; }
package p5::punct;	BEGIN { @p5::punct::ISA = 'p5::token'; }
package p5::remod;	BEGIN { @p5::remod::ISA = 'p5::token'; }
package p5::sigil;	BEGIN { @p5::sigil::ISA = 'p5::punct'; }
package p5::token;	BEGIN { @p5::token::ISA = 'p5::text'; }
#package p5::version;	BEGIN { @p5::version::ISA = 'p5::token'; }

################################################################
# Routines to turn XML tree into an AST.  Mostly this amounts to hoisting
# misplaced nodes and flattening various things into lists.

package PLXML;

sub AUTOLOAD {
    ::x("AUTOLOAD $PLXML::AUTOLOAD", @_);
    return "[[[ $PLXML::AUTOLOAD ]]]";
}

sub prepreproc {
    my $self = shift;
    my $kids = $$self{Kids};
    $self->{mp} = {};
    if (defined $kids) {
	my $i;
	for ($i = 0; $i < @$kids; $i++) {
	    if (ref $kids->[$i] eq "PLXML::madprops") {
		$self->{mp} = splice(@$kids, $i, 1)->hash($self,@_);
		$i--;
		next;
	    }
	    else {
		prepreproc($kids->[$i], $self, @_);
	    }
	}
    }
}

sub preproc {
    my $self = shift;
    if (ref $self eq 'PLXML::op_null' and $$self{was}) {
	return "PLXML::op_$$self{was}"->key();
    }
    else {
	return $self->key();
    }
}

sub newtype {
    my $self = shift;
    my $t = ref $self || $self;
    $t = "PLXML::op_$$self{was}" if $t eq 'PLXML::op_null' and $$self{was};
    $t =~ s/PLXML/P5AST/ or die "Bad type: $t";
    return $t;
}

sub madness {
    my $self = shift;
    my @keys = split(' ', shift);
    @keys = map { $_ eq 'd' ? ('k', 'd') : $_ } @keys;
    my @vals = ();
    for my $key (@keys) {
	my $madprop = $self->{mp}{$key};
	next unless defined $madprop;
	if (ref $madprop eq 'PLXML::mad_op') {
	    if ($key eq 'b') {
		push @vals, $madprop->blockast($self, @_);
	    }
	    else {
		push @vals, $madprop->ast($self, @_);
	    }
	    next;
	}
	my $white;
	if ($white = $self->{mp}{"_$key"}) {
	    push @vals, p5::junk->new($white);
	}
	my $type = $madtype{$key} || "p5::token";
	push @vals, $type->new($madprop);
	if ($white = $self->{mp}{"#$key"}) {
	    push @vals, p5::junk->new($white);
	}
    }
    @vals;
}

sub blockast {
    my $self = shift;
    $self->ast(@_);
}

sub ast {
    my $self = shift;

    my @newkids;
    for my $kid (@{$$self{Kids}}) {
	push @newkids, $kid->ast($self, @_);
    }
    return $self->newtype->new(Kids => [uc $self->key(), "(", @newkids, ")"]);
}

sub op {
    my $self = shift;
    my $desc = $self->desc();
    if ($desc =~ /\((.*?)\)/) {
	return $1;
    }
    else {
	return " <<" . $self->key() . ">> ";
    }
}

sub mp {
    my $self = shift;
    return $self->{mp};
}

package PLXML::Characters;

sub ast { die "oops" }
sub pair { die "oops" }

package PLXML::madprops;

sub ast {
    die "oops madprops";
}

sub hash {
    my $self = shift;
    my @pairs;
    my %hash = ();
    my $firstthing = '';
    my $lastthing = '';
    
    # We need to guarantee key uniqueness at this point.
    for my $kid (@{$$self{Kids}}) {
	my ($k,$v) = $kid->pair($self, @_);
	$firstthing ||= $k;
        $k .= 'x' while exists $hash{$k};
        $lastthing = $k;
	$hash{$k} = $v;
    }
    $hash{FIRST} = $firstthing;
    $hash{LAST} = $lastthing;
    return \%hash;
}

package PLXML::mad_op;

sub pair {
    my $self = shift;
    my $key = $$self{key};
    return $key,$self;
}

sub ast {
    my $self = shift;
    $self->prepreproc(@_);
    my @vals;
    for my $kid (@{$$self{Kids}}) {
        push @vals, $kid->ast($self, @_);
    }
    if (@vals == 1) {
	return @vals;
    }
    else {
	return P5AST::op_list->new(Kids => [@vals]);
    }
}

sub blockast {
    my $self = shift;
    $self->prepreproc(@_);
    my @vals;
    for my $kid (@{$$self{Kids}}) {
        push @vals, $kid->blockast($self, @_);
    }
    if (@vals == 1) {
	return @vals;
    }
    else {
	return P5AST::op_lineseq->new(Kids => [@vals]);
    }
}

package PLXML::mad_pv;

sub pair {
    my $self = shift;
    my $key = $$self{key};
    my $val = $$self{val};
    $val =~ s/STUPIDXML\(#x(\w+)\)/chr(hex $1)/eg;
    return $key,$val;
}

package PLXML::mad_sv;

sub pair {
    my $self = shift;
    my $key = $$self{key};
    my $val = $$self{val};
    $val =~ s/STUPIDXML\(#x(\w+)\)/chr(hex $1)/eg;
    return $key,$val;
}

package PLXML::baseop;

sub ast {
    my $self = shift;

    my @retval;
    my @newkids;
    for my $kid (@{$$self{Kids}}) {
	push @newkids, $kid->ast($self, @_);
    }
    if (@newkids) {
	push @retval, uc $self->key(), "(", @newkids , ")";
    }
    else {
	push @retval, $self->madness('o ( )');
    }
    return $self->newtype->new(Kids => [@retval]);
}

package PLXML::baseop_unop;

sub ast {
    my $self = shift;
    my @newkids = $self->madness('d o (');

    if (exists $$self{Kids}) {
	my $arg = $$self{Kids}[0];
	push @newkids, $arg->ast($self, @_) if defined $arg;
    }
    push @newkids, $self->madness(')');

    return $self->newtype()->new(Kids => [@newkids]);
}

package PLXML::binop;

sub ast {
    my $self = shift;
    my @newkids;

    my $left = $$self{Kids}[0];
    push @newkids, $left->ast($self, @_);

    push @newkids, $self->madness('o');

    my $right = $$self{Kids}[1];
    if (defined $right) {
	push @newkids, $right->ast($self, @_);
    }

    return $self->newtype->new(Kids => [@newkids]);
}

package PLXML::cop;

package PLXML::filestatop;

sub ast {
    my $self = shift;

    my @newkids = $self->madness('o (');

    if (@{$$self{Kids}}) {
	for my $kid (@{$$self{Kids}}) {
	    push @newkids, $kid->ast($self, @_);
	}
    }
    if ($$self{mp}{O}) {
	push @newkids, $self->madness('O');
    }
    push @newkids, $self->madness(')');

    return $self->newtype->new(Kids => [@newkids]);
}

package PLXML::listop;

sub ast {
    my $self = shift;

    my @retval;
    my @after;
    if (@retval = $self->madness('X')) {
	my @before, $self->madness('o x');
	return P5AST::listop->new(Kids => [@before,@retval]);
    }

    push @retval, $self->madness('o d ( [ {');

    my @newkids;
    for my $kid (@{$$self{Kids}}) {
	next if ref $kid eq 'PLXML::op_pushmark';
	next if ref $kid eq 'PLXML::op_null' and
		defined $$kid{was} and $$kid{was} eq 'pushmark';
	push @newkids, $kid->ast($self, @_);
    }

    my $x = "";

    if ($$self{mp}{S}) {
	push @retval, $self->madness('S');
    }
    push @retval, @newkids;

    push @retval, $self->madness('} ] )');
    return $self->newtype->new(Kids => [@retval,@after]);
}

package PLXML::logop;

sub ast {
    my $self = shift;

    my @newkids;
    push @newkids, $self->madness('o (');
    for my $kid (@{$$self{Kids}}) {
	push @newkids, $kid->ast($self, @_);
    }
    push @newkids, $self->madness(')');
    return $self->newtype->new(Kids => [@newkids]);
}

package PLXML::loop;

package PLXML::loopexop;

sub ast {
    my $self = shift;
    my @newkids = $self->madness('o (');

    if ($$self{mp}{L} or not $$self{flags} =~ /\bSPECIAL\b/) {
	my @label = $self->madness('L');
	if (@label) {
	    push @newkids, @label;
	}
	else {
	    my $arg = $$self{Kids}[0];
	    push @newkids, $arg->ast($self, @_) if defined $arg;
	}
    }
    push @newkids, $self->madness(')');

    return $self->newtype->new(Kids => [@newkids]);
}


package PLXML::padop;

package PLXML::padop_svop;

package PLXML::pmop;

sub ast {
    my $self = shift;

    return P5AST::pmop->new(Kids => []) unless exists $$self{flags};

    my $bits = $self->fetchbits($$self{flags},@_);

    my @newkids;
    if ($bits->{binding}) {
	push @newkids, $bits->{binding};
	push @newkids, $self->madness('~');
    }
    if (exists $bits->{regcomp} and $bits->{regcomp}) {
	my @front = $self->madness('q');
	my @back = $self->madness('Q');
	push @newkids, @front, $bits->{regcomp}, @back,
		$self->madness('m');
    }
    elsif ($$self{mp}{q}) {
	push @newkids, $self->madness('q = Q m');
    }
    elsif ($$self{mp}{X}) {
	push @newkids, $self->madness('X m');
    }
    else {
	push @newkids, $self->madness('e m');
    }

    return $self->newtype->new(Kids => [@newkids]);
}

sub innerpmop {
    my $pmop = shift;
    my $bits = shift;
    for my $key (grep {!/^Kids/} keys %$pmop) {
	$bits->{$key} = $pmop->{$key};
    }

    # Have to delete all the fake evals of the repl.  This is a pain...
    if (@{$$pmop{Kids}}) {
	my $really = $$pmop{Kids}[0]{Kids}[0];
	if (ref $really eq 'PLXML::op_substcont') {
	    $really = $$really{Kids}[0];
	}
	while ((ref $really) =~ /^PLXML::op_.*(null|entereval)/) {
	    if (exists $$really{was}) {
		$bits->{repl} = $really->ast(@_);
		return;
	    }
	    $really = $$really{Kids}[0];
	}
	if (ref $really eq 'PLXML::op_scope' and
	    @{$$really{Kids}} == 1 and
	    ref $$really{Kids}[0] eq 'PLXML::op_null' and
	    not @{$$really{Kids}[0]{Kids}})
	{
	    $bits->{repl} = '';
	    return;
	}
	if (ref $really eq 'PLXML::op_leave' and
	    @{$$really{Kids}} == 2 and
	    ref $$really{Kids}[1] eq 'PLXML::op_null' and
	    not @{$$really{Kids}[1]{Kids}})
	{
	    $bits->{repl} = '';
	    return;
	}
	if ((ref $really) =~ /^PLXML::op_(scope|leave)/) {
	    # should be at inner do {...} here, so skip that fakery too
	    $bits->{repl} = $really->newtype->new(Kids => [$really->PLXML::op_lineseq::lineseq(@_)]);
	    # but retrieve the whitespace before fake '}'
	    if ($$really{mp}{'_}'}) {
		push(@{$bits->{repl}->{Kids}}, p5::junk->new($$really{mp}{'_}'}));
	    }
	}
	else {	# something else, padsv probably
	    $bits->{repl} = $really->ast(@_);
	}
    }
}

sub fetchbits {
    my $self = shift;
    my $flags = shift || '';
    my %bits = %$self;
    my @kids = @{$$self{Kids}};
    if (@kids) {
	delete $bits{Kids};
	my $arg = shift @kids;
	innerpmop($arg,\%bits, $self, @_);
	if ($flags =~ /STACKED/) {
	    $arg = shift @kids;
	    $bits{binding} = $arg->ast($self, @_);
	}
	if ($bits{when} ne "COMP" and @kids) {
	    $arg = pop @kids;
	    $bits{regcomp} = $arg->ast($self, @_);
	}
	if (not exists $bits{repl} and @kids) {
	    $arg = shift @kids;
	    $bits{repl} = $arg->ast($self, @_);
	}
    }
    return \%bits;
}

package PLXML::pvop_svop;

package PLXML::unop;

sub ast {
    my $self = shift;
    my @newkids = $self->madness('o (');

    if (exists $$self{Kids}) {
	my $arg = $$self{Kids}[0];
	push @newkids, $arg->ast($self, @_) if defined $arg;
    }
    push @newkids, $self->madness(')');

    return $self->newtype->new(Kids => [@newkids]);
}

package PLXML;
package PLXML::Characters;
package PLXML::madprops;
package PLXML::mad_op;
package PLXML::mad_pv;
package PLXML::baseop;
package PLXML::baseop_unop;
package PLXML::binop;
package PLXML::cop;
package PLXML::filestatop;
package PLXML::listop;
package PLXML::logop;
package PLXML::loop;
package PLXML::loopexop;
package PLXML::padop;
package PLXML::padop_svop;
package PLXML::pmop;
package PLXML::pvop_svop;
package PLXML::unop;
package PLXML::op_null;

# Null nodes typed by first madprop.

my %astmad;

BEGIN {
    %astmad = (
	'p' => sub {		# peg for #! line, etc.
	    my $self = shift;
	    my @newkids;
	    push @newkids, $self->madness('p px');
	    $::curstate = 0;
	    return P5AST::peg->new(Kids => [@newkids])
	},
	'(' => sub {		# extra parens around the whole thing
	    my $self = shift;
	    my @newkids;
	    push @newkids, $self->madness('dx d o (');
	    for my $kid (@{$$self{Kids}}) {
		push @newkids, $kid->ast($self, @_);
	    }
	    push @newkids, $self->madness(')');
	    return P5AST::parens->new(Kids => [@newkids])
	},
	'~' => sub {				# binding operator
	    my $self = shift;
	    my @newkids;
	    push @newkids, $$self{Kids}[0]->ast($self,@_);
	    push @newkids, $self->madness('~');
	    push @newkids, $$self{Kids}[1]->ast($self,@_);
	    return P5AST::bindop->new(Kids => [@newkids])
	},
	';' => sub {		# null statements/blocks
	    my $self = shift;
	    my @newkids;
	    push @newkids, $self->madness('{ ; }');
	    $::curstate = 0;
	    return P5AST::nothing->new(Kids => [@newkids])
	},
	'I' => sub {		# if or unless statement keyword
	    my $self = shift;
	    my @newkids;
	    push @newkids, $self->madness('L I (');
	    my @subkids;
	    for my $kid (@{$$self{Kids}}) {
		push @subkids, $kid->ast($self, @_);
	    }
	    die "oops in op_null->new" unless @subkids == 1;
	    my $newself = $subkids[0];
	    @subkids = @{$$newself{Kids}};
	    
	    unshift @{$subkids[0]{Kids}}, @newkids;
	    push @{$subkids[0]{Kids}}, $self->madness(')');
	    return bless($newself, 'P5AST::condstate');
	},
	'U' => sub {			# use
	    my $self = shift;
	    my @newkids;
	    my @module = $self->madness('U');
	    my @args = $self->madness('A');
	    my $module = $module[-1]{Kids}[-1];
	    if ($module->uni eq 'bytes') {
		$::curenc = Nomad::encnum('iso-8859-1');
	    }
	    elsif ($module->uni eq 'utf8') {
		if ($$self{mp}{o} eq 'no') {
		    $::curenc = Nomad::encnum('iso-8859-1');
		}
		else {
		    $::curenc = Nomad::encnum('utf-8');
		}
	    }
	    elsif ($module->uni eq 'encoding') {
		if ($$self{mp}{o} eq 'no') {
		    $::curenc = Nomad::encnum('iso-8859-1');
		}
		else {
		    $::curenc = Nomad::encnum(eval $args[0]->p5text); # XXX bletch
		}
	    }
	    # (Surrounding {} ends up here if use is only thing in block.)
	    push @newkids, $self->madness('{ o');
	    push @newkids, @module;
	    push @newkids, $self->madness('V');
	    push @newkids, @args;
	    push @newkids, $self->madness('S ; }');
	    $::curstate = 0;
	    return P5AST::use->new(Kids => [@newkids])
	},
	'?' => sub {			# ternary
	    my $self = shift;
	    my @newkids;
	    my @subkids;
	    my @condkids = @{$$self{Kids}[0]{Kids}};
	    
	    push @newkids, $condkids[0]->ast($self,@_), $self->madness('?');
	    push @newkids, $condkids[1]->ast($self,@_), $self->madness(':');
	    push @newkids, $condkids[2]->ast($self,@_);
	    return P5AST::ternary->new(Kids => [@newkids])
	},
	'&' => sub {			# subroutine
	    my $self = shift;
	    my @newkids;
	    push @newkids, $self->madness('d n s a : { & } ;');
	    $::curstate = 0;
	    return P5AST::sub->new(Kids => [@newkids])
	},
	'i' => sub {			# modifier if
	    my $self = shift;
	    my @newkids;
	    push @newkids, $self->madness('i');
	    my $cond = $$self{Kids}[0];
	    my @subkids;
	    for my $kid (@{$$cond{Kids}}) {
		push @subkids, $kid->ast($self, @_);
	    }
	    push @newkids, shift @subkids;
	    unshift @newkids, @subkids;
	    return P5AST::condmod->new(Kids => [@newkids])
	},
	'P' => sub {				# package declaration
	    my $self = shift;
	    my @newkids;
	    push @newkids, $self->madness('o');
	    push @newkids, $self->madness('P');
	    push @newkids, $self->madness(';');
	    $::curstate = 0;
	    return P5AST::package->new(Kids => [@newkids])
	},
	'F' => sub {				# format
	    my $self = shift;
	    my @newkids = $self->madness('F n b');
	    $::curstate = 0;
	    return P5AST::format->new(Kids => [@newkids])
	},
	'x' => sub {				# qw literal
	    my $self = shift;
	    return P5AST::qwliteral->new(Kids => [$self->madness('x')])
	},
	'q' => sub {				# random quote
	    my $self = shift;
	    return P5AST::quote->new(Kids => [$self->madness('q = Q')])
	},
	'X' => sub {				# random literal
	    my $self = shift;
	    return P5AST::token->new(Kids => [$self->madness('X')])
	},
	':' => sub {				# attr list
	    my $self = shift;
	    return P5AST::attrlist->new(Kids => [$self->madness(':')])
	},
	',' => sub {				# "unary ," so to speak
	    my $self = shift;
	    my @newkids;
	    push @newkids, $self->madness(',');
	    push @newkids, $$self{Kids}[0]->ast($self,@_);
	    return P5AST::listelem->new(Kids => [@newkids])
	},
	'C' => sub {				# constant conditional
	    my $self = shift;
	    my @newkids;
	    push @newkids, $$self{Kids}[0]->ast($self,@_);
	    my @folded = $self->madness('C');
	    if (@folded) {
		my @t = $self->madness('t');
		my @e = $self->madness('e');
		if (@e) {
		    return P5AST::op_cond_expr->new(
			Kids => [
			    $self->madness('I ('),
			    @folded,
			    $self->madness(') ?'),
			    P5AST::op_cond_expr->new(Kids => [@newkids]),
			    $self->madness(':'),
			    @e
			] );
		}
		else {
		    return P5AST::op_cond_expr->new(
			Kids => [
			    $self->madness('I ('),
			    @folded,
			    $self->madness(') ?'),
			    @t,
			    $self->madness(':'),
			    @newkids
			] );
		}
	    }
	    return P5AST::op_null->new(Kids => [@newkids])
	},
	'+' => sub {				# unary +
	    my $self = shift;
	    my @newkids;
	    push @newkids, $self->madness('+');
	    push @newkids, $$self{Kids}[0]->ast($self,@_);
	    return P5AST::preplus->new(Kids => [@newkids])
	},
	'D' => sub {				# do block
	    my $self = shift;
	    my @newkids;
	    push @newkids, $self->madness('D');
	    push @newkids, $$self{Kids}[0]->ast($self,@_);
	    return P5AST::doblock->new(Kids => [@newkids])
	},
	'3' => sub {				# C-style for loop
	    my $self = shift;
	    my @newkids;

	    # What a mess!
	    my (undef, $init, $lineseq) = @{$$self{Kids}[0]{Kids}};
	    my (undef, $leaveloop) = @{$$lineseq{Kids}};
	    my (undef, $null) = @{$$leaveloop{Kids}};
	    my $and;
	    my $cond;
	    my $lineseq2;
	    my $block;
	    my $cont;
	    if (exists $$null{was} and $$null{was} eq 'and') {
		($lineseq2) = @{$$null{Kids}};
	    }
	    else {
		($and) = @{$$null{Kids}};
		($cond, $lineseq2) = @{$$and{Kids}};
	    }
	    if ($$lineseq2{mp}{'{'}) {
		$block = $lineseq2;
	    }
	    else {
		($block, $cont) = @{$$lineseq2{Kids}};
	    }

	    push @newkids, $self->madness('L 3 (');
	    push @newkids, $init->ast($self,@_);
	    push @newkids, $self->madness('1');
	    if (defined $cond) {
		push @newkids, $cond->ast($self,@_);
	    }
	    elsif (defined $null) {
		push @newkids, $null->madness('1');
	    }
	    push @newkids, $self->madness('2');
	    if (defined $cont) {
		push @newkids, $cont->ast($self,@_);
	    }
	    push @newkids, $self->madness(')');
	    push @newkids, $block->blockast($self,@_);
	    $::curstate = 0;
	    return P5AST::cfor->new(Kids => [@newkids])
	},
	'o' => sub {			# random useless operator
	    my $self = shift;
	    my @newkids;
	    push @newkids, $self->madness('o');
	    my $kind = $newkids[-1] || '';
	    $kind = $kind->uni if ref $kind;
	    my @subkids;
	    for my $kid (@{$$self{Kids}}) {
		push @subkids, $kid->ast($self, @_);
	    }
	    if ($kind eq '=') {	# stealth readline
		unshift(@newkids, shift(@subkids));
		push(@newkids, @subkids);
		return P5AST::op_aassign->new(Kids => [@newkids])
	    }
	    else {
		my $newself = $subkids[0];
		splice(@{$newself->{Kids}}, 1, 0,
			    $self->madness('ox ('),
			    @newkids,
			    $self->madness(')')
		);
		return $newself;
	    }
	},
    );
}

# Null nodes are an untyped mess inside Perl.  Instead of fixing it there,
# we derive an effective type either from the "was" field or the first madprop.
# (The individual routines select the actual new type.)

sub ast {
    my $self = shift;
    my $was = $$self{was} || 'peg';
    my $mad = $$self{mp}{FIRST} || "unknown";

    # First try for a "was".
    my $meth = "PLXML::op_${was}::astnull";
    if (exists &{$meth}) {
	return $self->$meth(@_);
    }

    # Look at first madprop.
    if (exists $astmad{$mad}) {
	return $astmad{$mad}->($self);
    }
    warn "No mad $mad" unless $mad eq 'unknown';

    # Do something generic.
    my @newkids;
    for my $kid (@{$$self{Kids}}) {
	push @newkids, $kid->ast($self, @_);
    }
    return $self->newtype->new(Kids => [@newkids]);
}

sub blockast {
    my $self = shift;
    local $::curstate;
    local $::curenc = $::curenc;
    return $self->madness('{ ; }');
}

package PLXML::op_stub;

sub ast {
    my $self = shift;
    return $self->newtype->new(Kids => [$self->madness(', x ( ) q = Q')]);
}

package PLXML::op_scalar;

sub ast {
    my $self = shift;

    my @pre = $self->madness('o q');
    my $op = pop @pre;
    if ($op->uni =~ /^<</) {
	my @newkids;
	my $opstub = bless { start => $op }, 'P5AST::heredoc';
	push @newkids, $opstub;
	push @newkids, $self->madness('(');

	my @kids = @{$$self{Kids}};

	my @divert;
	for my $kid (@kids) {
	    next if ref $kid eq 'PLXML::op_pushmark';
	    next if ref $kid eq 'PLXML::op_null' and
		    defined $$kid{was} and $$kid{was} eq 'pushmark';
	    push @divert, $kid->ast($self, @_);
	}
	$opstub->{doc} = P5AST::op_list->new(Kids => [@divert]);
	$opstub->{end} = ($self->madness('Q'))[-1];

	push @newkids, $self->madness(')');

	return $self->newtype->new(Kids => [@pre,@newkids]);
    }
    return $self->PLXML::baseop_unop::ast();
}

package PLXML::op_pushmark;

sub ast { () }

package PLXML::op_wantarray;
package PLXML::op_const;

sub astnull {
    my $self = shift;
    my @newkids;
    return unless $$self{mp};
    push @newkids, $self->madness('q = Q X : f O ( )');
    return P5AST::op_const->new(Kids => [@newkids]);
}

sub ast {
    my $self = shift;
    return unless %{$$self{mp}};

    my @before;

    my $const;
    my @args = $self->madness('f');
    if (@args) {
    }
    elsif (exists $self->{mp}{q}) {
	push @args, $self->madness('d q');
	if ($args[-1]->uni =~ /^<</) {
	    my $opstub = bless { start => pop(@args) }, 'P5AST::heredoc';
	    $opstub->{doc} = P5AST::op_const->new(Kids => [$self->madness('=')]);
	    $opstub->{end} = ($self->madness('Q'))[-1];
	    push @args, $opstub;
	}
	else {
	    push @args, $self->madness('= Q');
	}
    }
    elsif (exists $self->{mp}{X}) {
	push @before, $self->madness('d');	# was local $[ probably
	if (not $$self{mp}{O}) {
	    push @before, $self->madness('o');	# was unary
	}
	my @X = $self->madness(': X');
	if (exists $$self{private} and $$self{private} =~ /BARE/) {
	    return $self->newtype->new(Kids => [@X]);
	}
	my $X = pop @X;
	push @before, @X;
	@args = (
	    $self->madness('x'),
	    $X);
	if ($$self{mp}{O}) {
	    push @args, $self->madness('o O');
	}
    }
    elsif (exists $self->{mp}{O}) {
	push @args, $self->madness('O');
    }
    elsif ($$self{private} =~ /\bBARE\b/) {
	@args = ($$self{PV});
    }
    elsif (exists $$self{mp}{o}) {
	@args = $self->madness('o');
    }
    elsif (exists $$self{PV}) {
	@args = ('"', $$self{PV}, '"');
    }
    elsif (exists $$self{NV}) {
	@args = $$self{NV};
    }
    elsif (exists $$self{IV}) {
	@args = $$self{IV};
    }
    else {
	@args = $self->SUPER::text(@_);
    }
    return $self->newtype->new(Kids => [@before, @args]);
}


package PLXML::op_gvsv;

sub ast {
    my $self = shift;
    my @args;
    my @retval;
    for my $attr (qw/gv GV flags/) {
	if (exists $$self{$attr}) {
	    push @args, $attr, $$self{$attr};
	}
    }
    push @retval, @args;
    push @retval, $self->madness('X');
    return $self->newtype->new(Kids => [@retval]);
}

package PLXML::op_gv;

sub ast {
    my $self = shift;
    my @newkids;
    push @newkids, $self->madness('X K');

    return $self->newtype->new(Kids => [@newkids]);
}

package PLXML::op_gelem;

sub ast {
    my $self = shift;

    local $::curstate;	# in case there are statements in subscript
    local $::curenc = $::curenc;
    my @newkids;
    push @newkids, $self->madness('dx d');
    for my $kid (@{$$self{Kids}}) {
	push @newkids, $kid->ast($self, @_);
    }
    splice @newkids, -1, 0, $self->madness('o {');
    push @newkids, $self->madness('}');

    return $self->newtype->new(Kids => [@newkids]);
}

package PLXML::op_padsv;

sub ast {
    my $self = shift;
    my @args;
    push @args, $self->madness('dx d ( $ )');

    return $self->newtype->new(Kids => [@args]);
}

package PLXML::op_padav;

sub astnull { ast(@_) }

sub ast {
    my $self = shift;
    my @retval;
    push @retval, $self->madness('dx d (');
    push @retval, $self->madness('$ @');
    push @retval, $self->madness(') o O');
    return $self->newtype->new(Kids => [@retval]);
}

package PLXML::op_padhv;

sub astnull { ast(@_) }

sub ast {
    my $self = shift;
    my @retval;
    push @retval, $self->madness('dx d (');
    push @retval, $self->madness('$ @ %');
    push @retval, $self->madness(') o O');
    return $self->newtype->new(Kids => [@retval]);
}

package PLXML::op_padany;

package PLXML::op_pushre;

sub ast {
    my $self = shift;
    if ($$self{mp}{q}) {
	return $self->madness('q = Q m');
    }
    if ($$self{mp}{X}) {
	return $self->madness('X m');
    }
    if ($$self{mp}{e}) {
	return $self->madness('e m');
    }
    return $$self{Kids}[1]->ast($self,@_), $self->madness('m');
}

package PLXML::op_rv2gv;

sub ast {
    my $self = shift;

    my @newkids;
    push @newkids, $self->madness('dx d ( * $');
    push @newkids, $$self{Kids}[0]->ast();
    push @newkids, $self->madness(')');
    return $self->newtype->new(Kids => [@newkids]);
}

package PLXML::op_rv2sv;

sub astnull {
    my $self = shift;
    return P5AST::op_rv2sv->new(Kids => [$self->madness('O o dx d ( $ ) : a')]);
}

sub ast {
    my $self = shift;

    my @newkids;
    push @newkids, $self->madness('dx d ( $');
    if (ref $$self{Kids}[0] ne "PLXML::op_gv") {
	push @newkids, $$self{Kids}[0]->ast();
    }
    push @newkids, $self->madness(') : a');
    return $self->newtype->new(Kids => [@newkids]);
}

package PLXML::op_av2arylen;

sub ast {
    my $self = shift;

    my @newkids;
    push @newkids, $$self{Kids}[0]->madness('l');
    push @newkids, $$self{Kids}[0]->ast();
    return $self->newtype->new(Kids => [@newkids]);
}

package PLXML::op_rv2cv;

sub astnull {
    my $self = shift;
    my @newkids;
    push @newkids, $self->madness('X');
    return @newkids if @newkids;
    if (exists $$self{mp}{'&'}) {
	push @newkids, $self->madness('&');
	if (@{$$self{Kids}}) {
	    push @newkids, $$self{Kids}[0]->ast(@_);
	}
    }
    else {
	push @newkids, $$self{Kids}[0]->ast(@_);
    }
    return P5AST::op_rv2cv->new(Kids => [@newkids]);
}

sub ast {
    my $self = shift;

    my @newkids;
    push @newkids, $self->madness('&');
    if (@{$$self{Kids}}) {
	push @newkids, $$self{Kids}[0]->ast();
    }
    return $self->newtype->new(Kids => [@newkids]);
}

package PLXML::op_anoncode;

sub ast {
    my $self = shift;
    my $arg = $$self{Kids}[0];
    local $::curstate;		# hide nested statements in sub
    local $::curenc = $::curenc;
    if (defined $arg) {
	return $arg->ast(@_);
    }
    return ';';  # XXX literal ; should come through somewhere
}

package PLXML::op_prototype;
package PLXML::op_refgen;

sub ast {
    my $self = shift;
    my @newkids = $self->madness('o s a');

    if (exists $$self{Kids}) {
	my $arg = $$self{Kids}[0];
	push @newkids, $arg->ast($self, @_) if defined $arg;
    }

    my $res = $self->newtype->new(Kids => [@newkids]);
    return $res;
}

package PLXML::op_srefgen;

sub ast {
    my @newkids;
    my $self = shift;
    if ($$self{mp}{FIRST} eq '{') {
	local $::curstate;	# this is officially a block, so hide it
	local $::curenc = $::curenc;
	push @newkids, $self->madness('{');
	for my $kid (@{$$self{Kids}}) {
	    push @newkids, $kid->ast($self, @_);
	}
	push @newkids, $self->madness('; }');
	return P5AST::op_stringify->new(Kids => [@newkids]);
    }
    else {
	push @newkids, $self->madness('o [');
	for my $kid (@{$$self{Kids}}) {
	    push @newkids, $kid->ast($self, @_);
	}
	push @newkids, $self->madness(']');
	return P5AST::op_stringify->new(Kids => [@newkids]);
    }
}

package PLXML::op_ref;
package PLXML::op_bless;
package PLXML::op_backtick;

sub ast {
    my $self = shift;
    my @args;
    if (exists $self->{mp}{q}) {
	push @args, $self->madness('q');
	if ($args[-1]->uni =~ /^<</) {
	    my $opstub = bless { start => $args[-1] }, 'P5AST::heredoc';
	    $args[-1] = $opstub;
	    $opstub->{doc} = P5AST::op_const->new(Kids => [$self->madness('=')]);
	    $opstub->{end} = ($self->madness('Q'))[-1];
	}
	else {
	    push @args, $self->madness('= Q');
	}
    }
    return $self->newtype->new(Kids => [@args]);
}

package PLXML::op_glob;

sub astnull {
    my $self = shift;
    my @retval = $self->madness('o q = Q');
    if (not @retval or $retval[-1]->uni eq 'glob') {
	push @retval, $self->madness('(');
	push @retval, $$self{Kids}[0]->ast($self,@_);
	push @retval, $self->madness(')');
    }
    return P5AST::op_glob->new(Kids => [@retval]);
}

package PLXML::op_readline;

sub astnull {
    my $self = shift;
    my @retval;
    if (exists $$self{mp}{q}) {
	@retval = $self->madness('q = Q');
    }
    elsif (exists $$self{mp}{X}) {
	@retval = $self->madness('X');
    }
    return P5AST::op_readline->new(Kids => [@retval]);
}

sub ast {
    my $self = shift;

    my @retval;

    my @args;
    my $const;
    if (exists $$self{mp}{q}) {
	@args = $self->madness('q = Q');
    }
    elsif (exists $$self{mp}{X}) {
	@args = $self->madness('X');
    }
    elsif (exists $$self{GV}) {
	@args = $$self{IV};
    }
    elsif (@{$$self{Kids}}) {
	@args = $self->PLXML::unop::ast(@_);
    }
    else {
	@args = $self->SUPER::text(@_);
    }
    return $self->newtype->new(Kids => [@retval,@args]);
}


package PLXML::op_rcatline;
package PLXML::op_regcmaybe;
package PLXML::op_regcreset;
package PLXML::op_regcomp;

sub ast {
    my $self = shift;
    $self->PLXML::unop::ast(@_);
}

package PLXML::op_match;

sub ast {
    my $self = shift;
    my $retval = $self->SUPER::ast(@_);
    my $p5re;
    if (not $p5re = $retval->p5text()) {
	$retval = $self->newtype->new(Kids => [$self->madness('X q = Q m')]);
	$p5re = $retval->p5text();
    }
    if ($deinterpolate) {
	$retval->{P5re} = P5re::qrparse($p5re);
    }
    return $retval;
}

package PLXML::op_qr;

sub ast {
    my $self = shift;
    my $retval;
    if (exists $$self{flags}) {
	$retval = $self->SUPER::ast(@_);
    }
    else {
	$retval = $self->newtype->new(Kids => [$self->madness('X q = Q m')]);
    }
    if ($deinterpolate) {
	my $p5re = $retval->p5text();
	$retval->{P5re} = P5re::qrparse($p5re);
    }
    return $retval;
}

package PLXML::op_subst;

sub ast {
    my $self = shift;

    my $bits = $self->fetchbits($$self{flags},@_);

    my @newkids;
    if ($bits->{binding}) {
	push @newkids, $bits->{binding};
	push @newkids, $self->madness('~');
    }
    my $X = p5::token->new($$self{mp}{X});
    my @lfirst = $self->madness('q');
    my @llast = $self->madness('Q');
    push @newkids,
	@lfirst,
	$self->madness('E'),	# XXX s/b e probably
	@llast;
    my @rfirst = $self->madness('z');
    my @rlast = $self->madness('Z');
    my @mods = $self->madness('m');
    if ($rfirst[-1]->uni ne $llast[-1]->uni) {
	push @newkids, @rfirst;
    }
    # remove the fake '\n' if /e and '#' in replacement.
    if (@mods and $mods[0] =~ m/e/ and ($self->madness('R'))[0]->uni =~ m/#/) {
        unshift @rlast, bless {}, 'chomp'; # hack to remove '\n'
    }
    push @newkids, $bits->{repl}, @rlast, @mods;

    my $retval = $self->newtype->new(Kids => [@newkids]);
    if ($deinterpolate) {
	my $p5re = $retval->p5text();
	$retval->{P5re} = P5re::qrparse($p5re);
    }
    return $retval;
}

package PLXML::op_substcont;
package PLXML::op_trans;

sub ast {
    my $self = shift;

#    my $bits = $self->fetchbits($$self{flags},@_);
#
    my @newkids;
    my @lfirst = $self->madness('q');
    my @llast = $self->madness('Q');
    push @newkids,
	@lfirst,
	$self->madness('E'),
	@llast;
    my @rfirst = $self->madness('z');
    my @repl = $self->madness('R');
    my @rlast = $self->madness('Z');
    my @mods = $self->madness('m');
    if ($rfirst[-1]->uni ne $llast[-1]->uni) {
	push @newkids, @rfirst;
    }

    push @newkids, @repl, @rlast, @mods;

    my $res = $self->newtype->new(Kids => [@newkids]);
    return $res;
}

package PLXML::op_sassign;

sub ast {
    my $self = shift;
    my @newkids;

    my $right = $$self{Kids}[1];
    eval { push @newkids, $right->ast($self, @_); };

    push @newkids, $self->madness('o');

    my $left = $$self{Kids}[0];
    push @newkids, $left->ast($self, @_);

    return $self->newtype->new(Kids => [@newkids]);
}

package PLXML::op_aassign;

sub astnull { ast(@_) }

sub ast {
    my $self = shift;
    my @newkids;

    my $right = $$self{Kids}[1];
    push @newkids, $right->ast($self, @_);

    push @newkids, $self->madness('o');

    my $left = $$self{Kids}[0];
    push @newkids, $left->ast($self, @_);

    return $self->newtype->new(Kids => [@newkids]);
}

package PLXML::op_chop;
package PLXML::op_schop;
package PLXML::op_chomp;
package PLXML::op_schomp;
package PLXML::op_defined;
package PLXML::op_undef;
package PLXML::op_study;
package PLXML::op_pos;
package PLXML::op_preinc;

sub ast {
    my $self = shift;
    if ($$self{targ}) {		# stealth post inc or dec
	return $self->PLXML::op_postinc::ast(@_);
    }
    return $self->SUPER::ast(@_);
}

package PLXML::op_i_preinc;

sub ast { my $self = shift; $self->PLXML::op_preinc::ast(@_); }

package PLXML::op_predec;

sub ast { my $self = shift; $self->PLXML::op_preinc::ast(@_); }

package PLXML::op_i_predec;

sub ast { my $self = shift; $self->PLXML::op_preinc::ast(@_); }

package PLXML::op_postinc;

sub ast {
    my $self = shift;
    my @newkids;

    if (exists $$self{Kids}) {
	my $arg = $$self{Kids}[0];
	push @newkids, $arg->ast($self, @_) if defined $arg;
    }
    push @newkids, $self->madness('o');

    my $res = $self->newtype->new(Kids => [@newkids]);
    return $res;
}

package PLXML::op_i_postinc;

sub ast { my $self = shift; $self->PLXML::op_postinc::ast(@_); }

package PLXML::op_postdec;

sub ast { my $self = shift; $self->PLXML::op_postinc::ast(@_); }

package PLXML::op_i_postdec;

sub ast { my $self = shift; $self->PLXML::op_postinc::ast(@_); }

package PLXML::op_pow;
package PLXML::op_multiply;
package PLXML::op_i_multiply;
package PLXML::op_divide;
package PLXML::op_i_divide;
package PLXML::op_modulo;
package PLXML::op_i_modulo;
package PLXML::op_repeat;

sub ast {
    my $self = shift;
    return $self->SUPER::ast(@_)
	unless exists $$self{private} and $$self{private} =~ /DOLIST/;

    my $newself = $$self{Kids}[0]->ast($self,@_);
    splice @{$newself->{Kids}}, -1, 0, $self->madness('o');

    return bless $newself, $self->newtype;	# rebless the op_null
}

package PLXML::op_add;
package PLXML::op_i_add;
package PLXML::op_subtract;
package PLXML::op_i_subtract;
package PLXML::op_concat;

sub astnull {
    my $self = shift;
    my @newkids;

    my @after;
    my $left = $$self{Kids}[0];
    push @newkids, $left->ast($self, @_);

    push @newkids, $self->madness('o');

    my $right = $$self{Kids}[1];
    push @newkids, $right->ast($self, @_);
    return P5AST::op_concat->new(Kids => [@newkids]);
}

sub ast {
    my $self = shift;
    my $parent = $_[0];
    my @newkids;

    my @after;
    my $left = $$self{Kids}[0];
    push @newkids, $left->ast($self, @_);

    push @newkids, $self->madness('o');

    my $right = $$self{Kids}[1];
    push @newkids, $right->ast($self, @_);

    return $self->newtype->new(Kids => [@newkids, @after]);
}

package PLXML::op_stringify;

sub astnull {
    ast(@_);
}

sub ast {
    my $self = shift;
    my @newkids;
    my @front = $self->madness('q (');
    my @back = $self->madness(') Q');
    my @M = $self->madness('M');
    if (@M) {
	push @newkids, $M[0], $self->madness('o');
    }
    push @newkids, @front;
    for my $kid (@{$$self{Kids}}) {
	push @newkids, $kid->ast($self, @_);
    }
    push @newkids, @back;
    return P5AST::op_stringify->new(Kids => [@newkids]);
}

package PLXML::op_left_shift;
package PLXML::op_right_shift;
package PLXML::op_lt;
package PLXML::op_i_lt;
package PLXML::op_gt;
package PLXML::op_i_gt;
package PLXML::op_le;
package PLXML::op_i_le;
package PLXML::op_ge;
package PLXML::op_i_ge;
package PLXML::op_eq;
package PLXML::op_i_eq;
package PLXML::op_ne;
package PLXML::op_i_ne;
package PLXML::op_ncmp;
package PLXML::op_i_ncmp;
package PLXML::op_slt;
package PLXML::op_sgt;
package PLXML::op_sle;
package PLXML::op_sge;
package PLXML::op_seq;
package PLXML::op_sne;
package PLXML::op_scmp;
package PLXML::op_bit_and;
package PLXML::op_bit_xor;
package PLXML::op_bit_or;
package PLXML::op_negate;
package PLXML::op_i_negate;
package PLXML::op_not;

sub ast {
    my $self = shift;
    my @newkids = $self->madness('o (');
    my @swap;
    if (@newkids and $newkids[-1]->uni eq '!~') {
	@swap = @newkids;
	@newkids = ();
    }

    if (exists $$self{Kids}) {
	my $arg = $$self{Kids}[0];
	push @newkids, $arg->ast($self, @_) if defined $arg;
    }
    if (@swap) {
	splice @{$newkids[-1][0]{Kids}}, -2, 0, @swap;	# XXX WAG
    }
    push @newkids, $self->madness(')');

    my $res = $self->newtype->new(Kids => [@newkids]);
    return $res;
}

package PLXML::op_complement;
package PLXML::op_atan2;
package PLXML::op_sin;
package PLXML::op_cos;
package PLXML::op_rand;
package PLXML::op_srand;
package PLXML::op_exp;
package PLXML::op_log;
package PLXML::op_sqrt;
package PLXML::op_int;
package PLXML::op_hex;
package PLXML::op_oct;
package PLXML::op_abs;
package PLXML::op_length;
package PLXML::op_substr;
package PLXML::op_vec;
package PLXML::op_index;
package PLXML::op_rindex;
package PLXML::op_sprintf;
package PLXML::op_formline;
package PLXML::op_ord;
package PLXML::op_chr;
package PLXML::op_crypt;
package PLXML::op_ucfirst;

sub ast {
    my $self = shift;
    return $self->PLXML::listop::ast(@_);
}

package PLXML::op_lcfirst;

sub ast {
    my $self = shift;
    return $self->PLXML::listop::ast(@_);
}

package PLXML::op_uc;

sub ast {
    my $self = shift;
    return $self->PLXML::listop::ast(@_);
}

package PLXML::op_lc;

sub ast {
    my $self = shift;
    return $self->PLXML::listop::ast(@_);
}

package PLXML::op_quotemeta;

sub ast {
    my $self = shift;
    return $self->PLXML::listop::ast(@_);
}

package PLXML::op_rv2av;

sub astnull {
    my $self = shift;
    return P5AST::op_rv2av->new(Kids => [$self->madness('$ @')]);
}

sub ast {
    my $self = shift;

    if (ref $$self{Kids}[0] eq 'PLXML::op_const' and $$self{mp}{'O'}) {
	return $self->madness('O');
    }

    my @before;
    push @before, $self->madness('dx d (');

    my @newkids;
    push @newkids, $self->madness('$ @ K');
    if (ref $$self{Kids}[0] ne "PLXML::op_gv") {
	push @newkids, $$self{Kids}[0]->ast();
    }
    my @after;
    push @after, $self->madness(') a');
    return $self->newtype->new(Kids => [@before, @newkids, @after]);
}

package PLXML::op_aelemfast;

sub ast {
    my $self = shift;
    return $self->madness('$');
}

package PLXML::op_aelem;

sub astnull {
    my $self = shift;
    my @newkids;
    push @newkids, $self->madness('dx d');
    for my $kid (@{$$self{Kids}}) {
	push @newkids, $kid->ast($self, @_);
    }
    splice @newkids, -1, 0, $self->madness('a [');
    push @newkids, $self->madness(']');
    return P5AST::op_aelem->new(Kids => [@newkids]);
}

sub ast {
    my $self = shift;

    my @before = $self->madness('dx d');
    my @newkids;
    for my $kid (@{$$self{Kids}}) {
	push @newkids, $kid->ast(@_);
    }
    splice @newkids, -1, 0, $self->madness('a [');
    push @newkids, $self->madness(']');

    return $self->newtype->new(Kids => [@before, @newkids]);
}

package PLXML::op_aslice;

sub astnull {
    my $self = shift;
    my @newkids;
    push @newkids, $self->madness('[');
    for my $kid (@{$$self{Kids}}) {
	push @newkids, $kid->ast(@_);
    }
    unshift @newkids, pop @newkids;
    unshift @newkids, $self->madness('dx d');
    push @newkids, $self->madness(']');
    return P5AST::op_aslice->new(Kids => [@newkids]);
}

sub ast {
    my $self = shift;

    my @newkids;
    push @newkids, $self->madness('[');
    for my $kid (@{$$self{Kids}}) {
	push @newkids, $kid->ast(@_);
    }
    unshift @newkids, pop @newkids;
    unshift @newkids, $self->madness('dx d');
    push @newkids, $self->madness(']');

    return $self->newtype->new(Kids => [@newkids]);
}

package PLXML::op_each;
package PLXML::op_values;
package PLXML::op_keys;
package PLXML::op_delete;
package PLXML::op_exists;
package PLXML::op_rv2hv;

sub astnull {
    my $self = shift;
    return P5AST::op_rv2hv->new(Kids => [$self->madness('$')]);
}

sub ast {
    my $self = shift;

    my @before;
    push @before, $self->madness('dx d (');

    my @newkids;
    push @newkids, $self->madness('$ @ % K');
    if (ref $$self{Kids}[0] ne "PLXML::op_gv") {
	push @newkids, $$self{Kids}[0]->ast();
    }
    my @after;
    push @after, $self->madness(') a');
    return $self->newtype->new(Kids => [@before, @newkids, @after]);
}

package PLXML::op_helem;

sub astnull {
    my $self = shift;
    local $::curstate;	# hash subscript potentially a lineseq
    local $::curenc = $::curenc;

    my @newkids;
    push @newkids, $self->madness('dx d');
    for my $kid (@{$$self{Kids}}) {
	push @newkids, $kid->ast($self, @_);
    }
    splice @newkids, -1, 0, $self->madness('a {');
    push @newkids, $self->madness('}');
    return P5AST::op_helem->new(Kids => [@newkids]);
}

sub ast {
    my $self = shift;
    local $::curstate;	# hash subscript potentially a lineseq
    local $::curenc = $::curenc;

    my @before = $self->madness('dx d');
    my @newkids;
    for my $kid (@{$$self{Kids}}) {
	push @newkids, $kid->ast($self, @_);
    }
    splice @newkids, -1, 0, $self->madness('a {');
    push @newkids, $self->madness('}');

    return $self->newtype->new(Kids => [@before, @newkids]);
}


package PLXML::op_hslice;

sub astnull {
    my $self = shift;
    my @newkids;
    push @newkids, $self->madness('{');
    for my $kid (@{$$self{Kids}}) {
	push @newkids, $kid->ast(@_);
    }
    unshift @newkids, pop @newkids;
    unshift @newkids, $self->madness('dx d'); 
    push @newkids, $self->madness('}');
    return P5AST::op_hslice->new(Kids => [@newkids]);
}

sub ast {
    my $self = shift;

    my @newkids;
    push @newkids, $self->madness('{');
    for my $kid (@{$$self{Kids}}) {
	push @newkids, $kid->ast(@_);
    }
    unshift @newkids, pop @newkids;
    unshift @newkids, $self->madness('dx d'); 
    push @newkids, $self->madness('}');

    return $self->newtype->new(Kids => [@newkids]);
}

package PLXML::op_unpack;
package PLXML::op_pack;
package PLXML::op_split;
package PLXML::op_join;
package PLXML::op_list;

sub astnull {
    my $self = shift;
    my @newkids;
    my @retval;
    my @before;
    if (@retval = $self->madness('X')) {
	push @before, $self->madness('x o');
	return @before,@retval;
    }
    my @kids = @{$$self{Kids}};
    for my $kid (@kids) {
	next if ref $kid eq 'PLXML::op_pushmark';
	next if ref $kid eq 'PLXML::op_null' and
		defined $$kid{was} and $$kid{was} eq 'pushmark';
	push @newkids, $kid->ast($self, @_);
    }

    my $x = "";
    my @newnewkids = ();
    push @newnewkids, $self->madness('dx d (');
    push @newnewkids, @newkids;
    push @newnewkids, $self->madness(') :');
    return P5AST::op_list->new(Kids => [@newnewkids]);
}

sub ast {
    my $self = shift;

    my @retval;
    my @before;
    if (@retval = $self->madness('X')) {
	push @before, $self->madness('o');
	return $self->newtype->new(Kids => [@before,@retval]);
    }
    push @retval, $self->madness('dx d (');

    my @newkids;
    for my $kid (@{$$self{Kids}}) {
	push @newkids, $kid->ast($self, @_);
    }
    my $x = "";
    my @newnewkids = ();
    push @newnewkids, @newkids;
    @newkids = @newnewkids;
    push @retval, @newkids;
    push @retval, $self->madness(') :');
    return $self->newtype->new(Kids => [@retval]);
}

package PLXML::op_lslice;

sub ast {
    my $self = shift;
    my @newkids;

    if ($$self{mp}{q}) {
	push @newkids, $self->madness('q = Q');
    }
    elsif ($$self{mp}{x}) {
	push @newkids, $self->madness('x');
    }
    else {
	push @newkids, $self->madness('(');
	my $list = $$self{Kids}[1];
	push @newkids, $list->ast($self, @_);
	push @newkids, $self->madness(')');
    }

    push @newkids, $self->madness('[');

    my $slice = $$self{Kids}[0];
    push @newkids, $slice->ast($self, @_);
    push @newkids, $self->madness(']');

    return $self->newtype->new(Kids => [@newkids]);
}

package PLXML::op_anonlist;
package PLXML::op_anonhash;
package PLXML::op_splice;
package PLXML::op_push;
package PLXML::op_pop;
package PLXML::op_shift;
package PLXML::op_unshift;
package PLXML::op_sort;
package PLXML::op_reverse;

sub astnull {
    my $self = shift;
    $self->PLXML::listop::ast(@_);
}

package PLXML::op_grepstart;
package PLXML::op_grepwhile;
package PLXML::op_mapstart;
package PLXML::op_mapwhile;
package PLXML::op_range;

sub ast {
    my $self = shift;
    return $self->PLXML::binop::ast(@_);
}

package PLXML::op_flip;
package PLXML::op_flop;
package PLXML::op_and;

sub astnull {
    my $self = shift;
    my @newkids;
    my @first = $self->madness('1');
    my @second = $self->madness('2');
    my @stuff = $$self{Kids}[0]->ast();
    if (my @I = $self->madness('I')) {
	if (@second) {
	    push @newkids, @I;
	    push @newkids, $self->madness('(');
	    push @newkids, @stuff;
	    push @newkids, $self->madness(')');
	    push @newkids, @second;
	}
	else {
	    push @newkids, @I;
	    push @newkids, $self->madness('(');
	    push @newkids, @first;
	    push @newkids, $self->madness(')');
	    push @newkids, @stuff;
	}
    }
    elsif (my @i = $self->madness('i')) {
	if (@second) {
	    push @newkids, @second;
	    push @newkids, @i;
	    push @newkids, @stuff;
	}
	else {
	    push @newkids, @stuff;
	    push @newkids, @i;
	    push @newkids, @first;
	}
    }
    elsif (my @o = $self->madness('o')) {
	if (@second) {
	    push @newkids, @stuff;
	    push @newkids, @o;
	    push @newkids, @second;
	}
	else {
	    push @newkids, @first;
	    push @newkids, @o;
	    push @newkids, @stuff;
	}
    }
    return P5AST::op_and->new(Kids => [@newkids]);
}

package PLXML::op_or;

sub astnull {
    my $self = shift;
    my @newkids;
    my @first = $self->madness('1');
    my @second = $self->madness('2');
    my @i = $self->madness('i');
    my @stuff = $$self{Kids}[0]->ast();
    if (@second) {
	if (@i) {
	    push @newkids, @second;
	    push @newkids, $self->madness('i');
	    push @newkids, @stuff;
	}
	else {
	    push @newkids, @stuff;
	    push @newkids, $self->madness('o');
	    push @newkids, @second;
	}
    }
    else {
	if (@i) {
	    push @newkids, @stuff;
	    push @newkids, $self->madness('i');
	    push @newkids, @first;
	}
	else {
	    push @newkids, @first;
	    push @newkids, $self->madness('o');
	    push @newkids, @stuff;
	}
    }
    return "P5AST::op_$$self{was}"->new(Kids => [@newkids]);
}


package PLXML::op_xor;
package PLXML::op_cond_expr;
package PLXML::op_andassign;
package PLXML::op_orassign;
package PLXML::op_method;
package PLXML::op_entersub;

sub ast {
    my $self = shift;

    if ($$self{mp}{q}) {
	return $self->madness('q = Q');
    }
    if ($$self{mp}{X}) {		# <FH> override?
	return $self->madness('X');
    }
    if ($$self{mp}{A}) {
	return $self->astmethod(@_);
    }
    if ($$self{mp}{a}) {
	return $self->astarrow(@_);
    }

    my @retval;

    my @newkids;
    my @kids = @{$$self{Kids}};
    if (@kids == 1 and ref $kids[0] eq 'PLXML::op_null' and $kids[0]{was} =~ /list/) {
	@kids = @{$kids[0]{Kids}};
    }
    my $dest = pop @kids;
    my @dest = $dest->ast($self, @_);
    
    if (ref($dest) =~ /method/) {
	my $invocant = shift @kids;
	$invocant = shift @kids if ref($invocant) eq 'PLXML::op_pushmark';
	my @invocant = $invocant->ast($self, @_);
	push @retval, @dest;
	push @retval, @invocant;
    }
    elsif (exists $$self{mp}{o} and $$self{mp}{o} eq 'do') {
	push @retval, $self->madness('o');
	push @retval, @dest;
    }
    else {
	push @retval, $self->madness('o');
	push @retval, @dest;
    }
    while (@kids) {
	my $kid = shift(@kids);
	push @newkids, $kid->ast($self, @_);
    }

    push @retval, $self->madness('(');
    push @retval, @newkids;
    push @retval, $self->madness(')');
    return $self->newtype->new(Kids => [@retval]);
}

sub astmethod {
    my $self = shift;
    my @newkids;
    my @kids;
    for my $kid (@{$$self{Kids}}) {
	next if ref $kid eq 'PLXML::op_pushmark';
	next if ref $kid eq 'PLXML::op_null' and
		defined $$kid{was} and $$kid{was} eq 'pushmark';
	push @kids, $kid;
    }
    my @invocant;
    if ($$self{flags} =~ /\bSTACKED\b/) {
	push @invocant, shift(@kids)->ast($self, @_);
    }
    for my $kid (@kids) {
	push @newkids, $kid->ast($self, @_);
    }
    my $dest = pop(@newkids);
    if (ref $dest eq 'PLXML::op_rv2cv' and $$self{flags} =~ /\bMOD\b/) {
	$dest = pop(@newkids);
    }
    my $x = "";
    my @retval;
    push @retval, @invocant;
    push @retval, $self->madness('A');
    push @retval, $dest;
    push @retval, $self->madness('(');
    push @retval, @newkids;
    push @retval, $self->madness(')');
    return $self->newtype->new(Kids => [@retval]);
}

sub astarrow {
    my $self = shift;
    my @newkids;
    my @retval;
    my @kids = @{$$self{Kids}};
    if (@kids == 1 and ref $kids[0] eq 'PLXML::op_null' and $kids[0]{was} =~ /list/) {
	@kids = @{$kids[0]{Kids}};
    }
    while (@kids > 1) {
	my $kid = shift(@kids);
	push @newkids, $kid->ast($self, @_);
    }
    my @dest = $kids[0]->ast($self, @_);
    my $x = "";
    push @retval, @dest;
    push @retval, $self->madness('a');
    push @retval, $self->madness('(');
    push @retval, @newkids;
    push @retval, $self->madness(')');
    return $self->newtype->new(Kids => [@retval]);
}

package PLXML::op_leavesub;

sub ast {
    my $self = shift;
    if (ref $$self{Kids}[0] eq "PLXML::op_null") {
	return $$self{Kids}[0]->ast(@_);
    }
    return $$self{Kids}[0]->blockast($self, @_);
}

package PLXML::op_leavesublv;

sub ast {
    my $self = shift;

    return $$self{Kids}[0]->blockast($self, @_);
}

package PLXML::op_caller;
package PLXML::op_warn;
package PLXML::op_die;
package PLXML::op_reset;
package PLXML::op_lineseq;

sub lineseq {
    my $self = shift;
    my @kids = @{$$self{Kids}};
    local $::curstate = 0;	# (probably redundant, but that's okay)
    local $::prevstate = 0;
    local $::curenc = $::curenc;
    my @retval;
    my @newstuff;
    my $newprev;
    while (@kids) {
	my $kid = shift @kids;
	my $thing = $kid->ast($self, @_);
	next unless defined $thing;
	if ($::curstate ne $::prevstate) {
	    if ($::prevstate) {
		push @newstuff, $::prevstate->madness(';');
		push @{$newprev->{Kids}}, @newstuff if $newprev;
		@newstuff = ();
	    }
	    $::prevstate = $::curstate;
	    $newprev = $thing;
	    push @retval, $thing;
	}
	elsif ($::prevstate) {
	    push @newstuff, $thing;
	}
	else {
	    push @retval, $thing;
	}
    }
    if ($::prevstate) {
	push @newstuff, $::prevstate->madness(';');
	push @{$newprev->{Kids}}, @newstuff if $newprev;
	@newstuff = ();
	$::prevstate = 0;
    }
    return @retval;
}

sub blockast {
    my $self = shift;
    local $::curstate;

    my @retval;
    push @retval, $self->madness('{');
 
    my @newkids = $self->PLXML::op_lineseq::lineseq(@_);
    push @retval, @newkids;

    push @retval, $self->madness('; }');
    return $self->newtype->new(Kids => [@retval]);
}

package PLXML::op_nextstate;

sub newtype { return "P5AST::statement" }

sub astnull {
    my $self = shift;
    my @newkids;
    push @newkids, $self->madness('L');
    $::curstate = $self;
    return P5AST::statement->new(Kids => [@newkids]);
}

sub ast {
    my $self = shift;

    my @newkids;
    push @newkids, $self->madness('L');
    $::curstate = $self;
    return $self->newtype->new(Kids => [@newkids]);
}


package PLXML::op_dbstate;
package PLXML::op_unstack;
package PLXML::op_enter;

sub ast { () }

package PLXML::op_leave;

sub astnull {
    ast(@_);
}

sub ast {
    my $self = shift;

    my $mad = $$self{mp}{FIRST} || "unknown";

    my @retval;
    if ($mad eq 'w') {
	my @newkids;
	my @tmpkids;
	push @tmpkids, $self->{Kids};
	my $anddo = $$self{Kids}[-1]{Kids}[0]{Kids};
	eval { push @newkids, $anddo->[1]->ast($self,@_); };
	push @newkids, "[[[NOANDDO]]]" if $@;
	push @newkids, $self->madness('w');
	push @newkids, $anddo->[0]->ast($self,@_);

	return $self->newtype->new(Kids => [@newkids]);
    }

    local $::curstate;
    push @retval, $self->madness('o {');

    my @newkids = $self->PLXML::op_lineseq::lineseq(@_);
    push @retval, @newkids;
    push @retval, $self->madness(q/; }/);
    my $retval = $self->newtype->new(Kids => [@retval]);

    if ($$self{mp}{C}) {
	my @before;
	my @after;
	push @before, $self->madness('I ( C )');
	if ($$self{mp}{t}) {
	    push @before, $self->madness('t');
	}
	elsif ($$self{mp}{e}) {
	    push @after, $self->madness('e');
	}
	return P5AST::op_cond->new(Kids => [@before, $retval, @after]);
    }
    else {
	return $retval;
    }
}

package PLXML::op_scope;

sub ast {
    my $self = shift;
    local $::curstate;

    my @newkids;
    push @newkids, $self->madness('o');

    push @newkids, $self->madness('{');
    push @newkids, $self->PLXML::op_lineseq::lineseq(@_);
    push @newkids, $self->madness('; }');

    my @folded = $self->madness('C');
    if (@folded) {
	my @t = $self->madness('t');
	my @e = $self->madness('e');
	if (@e) {
	    return $self->newtype->new(
		Kids => [
		    $self->madness('I ('),
		    @folded,
		    $self->madness(')'),
		    $self->newtype->new(Kids => [@newkids]),
		    @e
		] );
	}
	else {
	    return $self->newtype->new(
		Kids => [
		    $self->madness('I ('),
		    @folded,
		    $self->madness(')'),
		    @t,
		    $self->newtype->new(Kids => [@newkids])
		] );
	}
    }
    return $self->newtype->new(Kids => [@newkids]);
}

package PLXML::op_enteriter;

sub ast {
    my $self = shift;
    my (undef,$range,$var) = @{$self->{Kids}};
    my @retval;
    push @retval, $self->madness('v');
    if (!@retval and defined $var) {
	push @retval, $var->ast($self,@_);
    }
    else {
	push @retval, '';
    }
    if (ref $range eq 'PLXML::op_null' and $$self{flags} =~ /STACKED/) {
	my (undef,$min,$max) = @{$range->{Kids}};
	push @retval, $min->ast($self,@_);
	if (defined $max) {
	    if (exists $$range{mp}{O}) {	# deeply buried .. operator
		PLXML::prepreproc($$range{mp}{O});
		push @retval,
		  $$range{mp}{'O'}{Kids}[0]{Kids}[0]{Kids}[0]{Kids}[0]->madness('o')
	    }
	    else {
		push @retval, '..';		# XXX missing whitespace
	    }
	    push @retval, $max->ast($self,@_);
	}
    }
    else {
	push @retval, $range->ast($self,@_);
    }
    return $self->newtype->new(Kids => [@retval]);
}

package PLXML::op_iter;
package PLXML::op_enterloop;

sub ast {
}

package PLXML::op_leaveloop;

sub ast {
    my $self = shift;

    my @retval;
    my @newkids;
    my $enterloop = $$self{Kids}[0];
    my $nextthing = $$self{Kids}[1];

    if ($$self{mp}{W}) {
	push @retval, $self->madness('L');
	push @newkids, $self->madness('W d');

	if (ref $enterloop eq 'PLXML::op_enteriter') {
	    my ($var,@rest) = @{$enterloop->ast($self,@_)->{Kids}};
	    push @newkids, $var if $var;
	    push @newkids, $self->madness('q ( x = Q');
	    push @newkids, @rest;
	}
	else {
	    push @newkids, $self->madness('(');
	    push @newkids, $enterloop->ast($self,@_);
	}
    }
    my $andor;

    if (ref $nextthing eq 'PLXML::op_null') {
	if ($$nextthing{mp}{'1'}) {
	    push @newkids, $nextthing->madness('1');
	    push @newkids, $self->madness(')');
	    push @newkids, $$nextthing{Kids}[0]->blockast($self,@_);
	}
	elsif ($$nextthing{mp}{'2'}) {
	    push @newkids, $$nextthing{Kids}[0]->ast($self,@_);
	    push @newkids, $self->madness(')');
	    push @newkids, $$nextthing{mp}{'2'}->blockast($self,@_);
	}
	elsif ($$nextthing{mp}{'U'}) {
	    push @newkids, $nextthing->ast($self,@_);
	}
	else {
	    # bypass the op_null
	    $andor = $nextthing->{Kids}[0];
	    eval {
		push @newkids, $$andor{Kids}[0]->ast($self, @_);
	    };
	    push @newkids, $self->madness(')');
	    eval {
		push @newkids, $$andor{Kids}[1]->blockast($self, @_);
	    };
	}
    }
    else {
	$andor = $nextthing;
	push @newkids, $nextthing->madness('O');
	push @newkids, $self->madness(')');
	push @newkids, $nextthing->blockast($self, @_);
    }
    if ($$self{mp}{w}) {
	push @newkids, $self->madness('w');
	push @newkids, $enterloop->ast($self,@_);
    }

    push @retval, @newkids;

    return $self->newtype->new(Kids => [@retval]);
}

package PLXML::op_return;
package PLXML::op_last;
package PLXML::op_next;
package PLXML::op_redo;
package PLXML::op_dump;
package PLXML::op_goto;
package PLXML::op_exit;
package PLXML::op_open;
package PLXML::op_close;
package PLXML::op_pipe_op;
package PLXML::op_fileno;
package PLXML::op_umask;
package PLXML::op_binmode;
package PLXML::op_tie;
package PLXML::op_untie;
package PLXML::op_tied;
package PLXML::op_dbmopen;
package PLXML::op_dbmclose;
package PLXML::op_sselect;
package PLXML::op_select;
package PLXML::op_getc;
package PLXML::op_read;
package PLXML::op_enterwrite;
package PLXML::op_leavewrite;
package PLXML::op_prtf;
package PLXML::op_print;
package PLXML::op_sysopen;
package PLXML::op_sysseek;
package PLXML::op_sysread;
package PLXML::op_syswrite;
package PLXML::op_send;
package PLXML::op_recv;
package PLXML::op_eof;
package PLXML::op_tell;
package PLXML::op_seek;
package PLXML::op_truncate;
package PLXML::op_fcntl;
package PLXML::op_ioctl;
package PLXML::op_flock;
package PLXML::op_socket;
package PLXML::op_sockpair;
package PLXML::op_bind;
package PLXML::op_connect;
package PLXML::op_listen;
package PLXML::op_accept;
package PLXML::op_shutdown;
package PLXML::op_gsockopt;
package PLXML::op_ssockopt;
package PLXML::op_getsockname;
package PLXML::op_getpeername;
package PLXML::op_lstat;
package PLXML::op_stat;
package PLXML::op_ftrread;
package PLXML::op_ftrwrite;
package PLXML::op_ftrexec;
package PLXML::op_fteread;
package PLXML::op_ftewrite;
package PLXML::op_fteexec;
package PLXML::op_ftis;
package PLXML::op_fteowned;
package PLXML::op_ftrowned;
package PLXML::op_ftzero;
package PLXML::op_ftsize;
package PLXML::op_ftmtime;
package PLXML::op_ftatime;
package PLXML::op_ftctime;
package PLXML::op_ftsock;
package PLXML::op_ftchr;
package PLXML::op_ftblk;
package PLXML::op_ftfile;
package PLXML::op_ftdir;
package PLXML::op_ftpipe;
package PLXML::op_ftlink;
package PLXML::op_ftsuid;
package PLXML::op_ftsgid;
package PLXML::op_ftsvtx;
package PLXML::op_fttty;
package PLXML::op_fttext;
package PLXML::op_ftbinary;
package PLXML::op_chdir;
package PLXML::op_chown;
package PLXML::op_chroot;
package PLXML::op_unlink;
package PLXML::op_chmod;
package PLXML::op_utime;
package PLXML::op_rename;
package PLXML::op_link;
package PLXML::op_symlink;
package PLXML::op_readlink;
package PLXML::op_mkdir;
package PLXML::op_rmdir;
package PLXML::op_open_dir;
package PLXML::op_readdir;
package PLXML::op_telldir;
package PLXML::op_seekdir;
package PLXML::op_rewinddir;
package PLXML::op_closedir;
package PLXML::op_fork;
package PLXML::op_wait;
package PLXML::op_waitpid;
package PLXML::op_system;
package PLXML::op_exec;
package PLXML::op_kill;
package PLXML::op_getppid;
package PLXML::op_getpgrp;
package PLXML::op_setpgrp;
package PLXML::op_getpriority;
package PLXML::op_setpriority;
package PLXML::op_time;
package PLXML::op_tms;
package PLXML::op_localtime;
package PLXML::op_gmtime;
package PLXML::op_alarm;
package PLXML::op_sleep;
package PLXML::op_shmget;
package PLXML::op_shmctl;
package PLXML::op_shmread;
package PLXML::op_shmwrite;
package PLXML::op_msgget;
package PLXML::op_msgctl;
package PLXML::op_msgsnd;
package PLXML::op_msgrcv;
package PLXML::op_semget;
package PLXML::op_semctl;
package PLXML::op_semop;
package PLXML::op_require;
package PLXML::op_dofile;
package PLXML::op_entereval;

sub ast {
    my $self = shift;
    local $::curstate;		# eval {} has own statement sequence
    return $self->SUPER::ast(@_);
}

package PLXML::op_leaveeval;
package PLXML::op_entertry;
package PLXML::op_leavetry;

sub ast {
    my $self = shift;

    return $self->PLXML::op_leave::ast(@_);
}

package PLXML::op_ghbyname;
package PLXML::op_ghbyaddr;
package PLXML::op_ghostent;
package PLXML::op_gnbyname;
package PLXML::op_gnbyaddr;
package PLXML::op_gnetent;
package PLXML::op_gpbyname;
package PLXML::op_gpbynumber;
package PLXML::op_gprotoent;
package PLXML::op_gsbyname;
package PLXML::op_gsbyport;
package PLXML::op_gservent;
package PLXML::op_shostent;
package PLXML::op_snetent;
package PLXML::op_sprotoent;
package PLXML::op_sservent;
package PLXML::op_ehostent;
package PLXML::op_enetent;
package PLXML::op_eprotoent;
package PLXML::op_eservent;
package PLXML::op_gpwnam;
package PLXML::op_gpwuid;
package PLXML::op_gpwent;
package PLXML::op_spwent;
package PLXML::op_epwent;
package PLXML::op_ggrnam;
package PLXML::op_ggrgid;
package PLXML::op_ggrent;
package PLXML::op_sgrent;
package PLXML::op_egrent;
package PLXML::op_getlogin;
package PLXML::op_syscall;
package PLXML::op_lock;
package PLXML::op_threadsv;
package PLXML::op_setstate;
package PLXML::op_method_named;

sub ast {
    my $self = shift;
    return $self->madness('O');
}

package PLXML::op_dor;

sub astnull {
    my $self = shift;
    $self->PLXML::op_or::astnull(@_);
}

package PLXML::op_dorassign;
package PLXML::op_custom;

