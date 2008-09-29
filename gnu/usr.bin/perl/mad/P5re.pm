#!/usr/bin/perl

# Copyright (C) 2005, Larry Wall
# This software may be copied under the same terms as Perl.

package P5re;

use strict;
use warnings;

our @EXPORT_OK = qw(re re2xml qr2xml);

my $indent = 0;
my $in = "";
my $delim = 1;
my $debug = 0;
my $maxbrack;

our $extended;
our $insensitive;
our $singleline;
our $multiline;

my %xmlish = (
	chr(0x00) => "STUPIDXML(#x00)",
	chr(0x01) => "STUPIDXML(#x01)",
	chr(0x02) => "STUPIDXML(#x02)",
	chr(0x03) => "STUPIDXML(#x03)",
	chr(0x04) => "STUPIDXML(#x04)",
	chr(0x05) => "STUPIDXML(#x05)",
	chr(0x06) => "STUPIDXML(#x06)",
	chr(0x07) => "STUPIDXML(#x07)",
	chr(0x08) => "STUPIDXML(#x08)",
	chr(0x09) => "&#9;",
	chr(0x0a) => "&#10;",
	chr(0x0b) => "STUPIDXML(#x0b)",
	chr(0x0c) => "STUPIDXML(#x0c)",
	chr(0x0d) => "&#13;",
	chr(0x0e) => "STUPIDXML(#x0e)",
	chr(0x0f) => "STUPIDXML(#x0f)",
	chr(0x10) => "STUPIDXML(#x10)",
	chr(0x11) => "STUPIDXML(#x11)",
	chr(0x12) => "STUPIDXML(#x12)",
	chr(0x13) => "STUPIDXML(#x13)",
	chr(0x14) => "STUPIDXML(#x14)",
	chr(0x15) => "STUPIDXML(#x15)",
	chr(0x16) => "STUPIDXML(#x16)",
	chr(0x17) => "STUPIDXML(#x17)",
	chr(0x18) => "STUPIDXML(#x18)",
	chr(0x19) => "STUPIDXML(#x19)",
	chr(0x1a) => "STUPIDXML(#x1a)",
	chr(0x1b) => "STUPIDXML(#x1b)",
	chr(0x1c) => "STUPIDXML(#x1c)",
	chr(0x1d) => "STUPIDXML(#x1d)",
	chr(0x1e) => "STUPIDXML(#x1e)",
	chr(0x1f) => "STUPIDXML(#x1f)",
	chr(0x7f) => "STUPIDXML(#x7f)",
	chr(0x80) => "STUPIDXML(#x80)",
	chr(0x81) => "STUPIDXML(#x81)",
	chr(0x82) => "STUPIDXML(#x82)",
	chr(0x83) => "STUPIDXML(#x83)",
	chr(0x84) => "STUPIDXML(#x84)",
	chr(0x86) => "STUPIDXML(#x86)",
	chr(0x87) => "STUPIDXML(#x87)",
	chr(0x88) => "STUPIDXML(#x88)",
	chr(0x89) => "STUPIDXML(#x89)",
	chr(0x90) => "STUPIDXML(#x90)",
	chr(0x91) => "STUPIDXML(#x91)",
	chr(0x92) => "STUPIDXML(#x92)",
	chr(0x93) => "STUPIDXML(#x93)",
	chr(0x94) => "STUPIDXML(#x94)",
	chr(0x95) => "STUPIDXML(#x95)",
	chr(0x96) => "STUPIDXML(#x96)",
	chr(0x97) => "STUPIDXML(#x97)",
	chr(0x98) => "STUPIDXML(#x98)",
	chr(0x99) => "STUPIDXML(#x99)",
	chr(0x9a) => "STUPIDXML(#x9a)",
	chr(0x9b) => "STUPIDXML(#x9b)",
	chr(0x9c) => "STUPIDXML(#x9c)",
	chr(0x9d) => "STUPIDXML(#x9d)",
	chr(0x9e) => "STUPIDXML(#x9e)",
	chr(0x9f) => "STUPIDXML(#x9f)",
	'<'       => "&lt;",
	'>'       => "&gt;",
	'&'       => "&amp;",
	'"'       => "&#34;",		# XML idiocy
);

sub xmlquote {
    my $text = shift;
    $text =~ s/(.)/$xmlish{$1} || $1/seg;
    return $text;
}

sub text {
    my $self = shift;
    return xmlquote($self->{text});
}

sub rep {
    my $self = shift;
    return xmlquote($self->{rep});
}

sub xmlkids {
    my $self = shift;
    my $array = $self->{Kids};
    my $ret = "";
    $indent += 2;
    $in = ' ' x $indent;
    foreach my $chunk (@$array) {
	if (ref $chunk eq "ARRAY") {
	    die;
	}
	elsif (ref $chunk) {
	    $ret .= $chunk->xml();
	}
	else {
	    warn $chunk;
	}
    }
    $indent -= 2;
    $in = ' ' x $indent;
    return $ret;
};

package P5re::RE; our @ISA = 'P5re';

sub xml {
    my $self = shift;
    my %flags = @_;
    if ($flags{indent}) {
	$indent = delete $flags{indent} || 0;
	$in = ' ' x $indent;
    }

    my $kind = $self->{kind};

    my $first = $self->{Kids}[0];
    if ($first and ref $first eq 'P5re::Mod') {
	for my $c (qw(i m s x)) {
	    next unless defined $first->{$c};
	    $self->{$c} = $first->{$c};
	    delete $first->{$c};
	}
    }

    my $modifiers = "";
    foreach my $k (sort keys %$self) {
	next if $k eq 'kind' or $k eq "Kids";
	my $v = $self->{$k};
	$k =~ s/^[A-Z]//;
	$modifiers .= " $k=\"$v\"";
    }
    my $text = "$in<$kind$modifiers>\n";
    $text .= $self->xmlkids();
    $text .= "$in</$kind>\n";
    return $text;
}

package P5re::Alt; our @ISA = 'P5re';

sub xml {
    my $self = shift;
    my $text = "$in<alt>\n";
    $text .= $self->xmlkids();
    $text .= "$in</alt>\n";
    return $text;
}

#package P5re::Atom; our @ISA = 'P5re';
#
#sub xml {
#    my $self = shift;
#    my $text = "$in<atom>\n";
#    $text .= $self->xmlkids();
#    $text .= "$in</atom>\n";
#    return $text;
#}

package P5re::Quant; our @ISA = 'P5re';

sub xml {
    my $self = shift;
    my $q = $self->{rep};
    my $min = $self->{min};
    my $max = $self->{max};
    my $greedy = $self->{greedy};
    my $text = "$in<quant rep=\"$q\" min=\"$min\" max=\"$max\" greedy=\"$greedy\">\n";
    $text .= $self->xmlkids();
    $text .= "$in</quant>\n";
    return $text;
}

package P5re::White; our @ISA = 'P5re';

sub xml {
    my $self = shift;
    return "$in<white text=\"" . $self->text() . "\" />\n";
}

package P5re::Char; our @ISA = 'P5re';

sub xml {
    my $self = shift;
    return "$in<char text=\"" . $self->text() . "\" />\n";
}

package P5re::Comment; our @ISA = 'P5re';

sub xml {
    my $self = shift;
    return "$in<comment rep=\"" . $self->rep() . "\" />\n";
}

package P5re::Mod; our @ISA = 'P5re';

sub xml {
    my $self = shift;
    my $modifiers = "";
    foreach my $k (sort keys %$self) {
	next if $k eq 'kind' or $k eq "Kids";
	my $v = $self->{$k};
	$k =~ s/^[A-Z]//;
	$modifiers .= " $k=\"$v\"";
    }
    return "$in<mod$modifiers />\n";
}

package P5re::Meta; our @ISA = 'P5re';

sub xml {
    my $self = shift;
    my $sem = "";
    if ($self->{sem}) {
	$sem = 'sem="' . $self->{sem} . '" '
    }
    return "$in<meta rep=\"" . $self->rep() . "\" $sem/>\n";
}

package P5re::Back; our @ISA = 'P5re';

sub xml {
    my $self = shift;
    return "$in<backref to=\"" . P5re::xmlquote($self->{to}) . "\"/>\n";
}

package P5re::Var; our @ISA = 'P5re';

sub xml {
    my $self = shift;
    return "$in<var name=\"" . $self->{name} . "\" />\n";
}

package P5re::Closure; our @ISA = 'P5re';

sub xml {
    my $self = shift;
    return "$in<closure rep=\"" . P5re::xmlquote($self->{rep}) . "\" />\n";
}

package P5re::CClass; our @ISA = 'P5re';

sub xml {
    my $self = shift;
    my $neg = $self->{neg} ? "negated" : "normal";
    my $text = "$in<cclass match=\"$neg\">\n";
    $text .= $self->xmlkids();
    $text .= "$in</cclass>\n";
    return $text;
}

package P5re::Range; our @ISA = 'P5re';

sub xml {
    my $self = shift;
    my $text = "$in<range>\n";
    $text .= $self->xmlkids();
    $text .= "$in</range>\n";
    return $text;
}

package P5re;

unless (caller) {
    while (<>) {
	chomp;
	print qr2xml($_);
	print "#######################################\n";
    }
}

sub qrparse {
    my $qr = shift;
    my $mod;
    if ($qr =~ /^s/) {
	$qr =~ s/^(?:\w*)(\W)((?:\\.|.)*?)\1(.*)\1(\w*)$/$2/;
	$mod = $4;
    }
    else {
	$qr =~ s/^(?:\w*)(\W)(.*)\1(\w*)$/$2/;
	$mod = $3;
    }
    substr($qr,0,0) = "(?$mod)" if defined $mod and $mod ne "";
    return parse($qr,@_);
}

sub qr2xml {
    return qrparse(@_)->xml();
}

sub re2xml {
    my $re = shift;
    return parse($re,@_)->xml();
}

sub parse {
    local($_) = shift;
    my %flags = @_;
    $maxbrack = 0;
    $indent = delete $flags{indent} || 0;
    $in = ' ' x $indent;
    warn "$_\n" if $debug;
    my $re = re('re');
    @$re{keys %flags} = values %flags;
    return $re;
}

sub re {
    my $kind = shift;

    my $oldextended = $extended;
    my $oldinsensitive = $insensitive;
    my $oldmultiline = $multiline;
    my $oldsingleline = $singleline;

    local $extended = $extended;
    local $insensitive = $insensitive;
    local $multiline = $multiline;
    local $singleline = $singleline;

    my $first = alt();

    my $re;
    if (not /^\|/) {
	$first->{kind} = $kind;
	$re = bless $first, "P5re::RE";  # rebless to remove single alt
    }
    else {
	my @alts = ($first);

	while (s/^\|//) {
	    push(@alts, alt());
	}
	$re = bless { Kids => [@alts], kind => $kind }, "P5re::RE";	
    }

    $re->{x} = $oldextended || 0;
    $re->{i} = $oldinsensitive || 0;
    $re->{m} = $oldmultiline || 0;
    $re->{s} = $oldsingleline || 0;
    return $re;
}

sub alt {
    my @quants;

    my $quant;
    while ($quant = quant()) {
	if (@quants and
	    ref $quant eq ref $quants[-1] and
	    exists $quants[-1]{text} and
	    exists $quant->{text} )
	{
	    $quants[-1]{text} .= $quant->{text};
	}
	else {
	    push(@quants, $quant);
	}
    }
    return bless { Kids => [@quants] }, "P5re::Alt";	
}

sub quant {
    my $atom = atom();
    return 0 unless $atom;
#    $atom = bless { Kids => [$atom] }, "P5re::Atom";	
    if (s/^(([*+?])(\??)|\{(\d+)(?:(,)(\d*))?\}(\??))//) {
	my $min = 0;
	my $max = "Inf";
	my $greed = 1;
	if ($2) {
	    if ($2 eq '+') {
		$min = 1;
	    }
	    elsif ($2 eq '?') {
		$max = 1;
	    }
	    $greed = 0 if $3;
	}
	elsif (defined $4) {
	    $min = $4;
	    if ($5) {
		$max = $6 if $6;
	    }
	    else {
		$max = $min;
	    }
	    $greed = 0 if $7;
	}
	$greed = "na" if $min == $max;
	return bless { Kids => [$atom],
		    rep => $1,
		    min => $min,
		    max => $max,
		    greedy => $greed
		}, "P5re::Quant";	
    }
    return $atom;
}

sub atom {
    my $re;
    if ($_ eq "") { return 0 }
    if (/^[)|]/) { return 0 }

    # whitespace is special because we don't know if /x is in effect
    if ($extended) {
	if (s/^(?=\s|#)(\s*(?:#.*)?)//) { return bless { text => $1 }, "P5re::White"; }
    }

    # all the parenthesized forms
    if (s/^\(//) {
	if (s/^\?://) {
	    $re = re('bracket');
	}
	elsif (s/^(\?#.*?)\)/)/) {
	    $re = bless { rep => "($1)" }, "P5re::Comment";	
	}
	elsif (s/^\?=//) {
	    $re = re('lookahead');
	}
	elsif (s/^\?!//) {
	    $re = re('neglookahead');
	}
	elsif (s/^\?<=//) {
	    $re = re('lookbehind');
	}
	elsif (s/^\?<!//) {
	    $re = re('neglookbehind');
	}
	elsif (s/^\?>//) {
	    $re = re('nobacktrack');
	}
	elsif (s/^(\?\??\{.*?\})\)/)/) {
	    $re = bless { rep => "($1)" }, "P5re::Closure";	
	}
	elsif (s/^(\?\(\d+\))//) {
	    my $mods = $1;
	    $re = re('conditional');
	    $re->{Arep} = "$mods";
	}
	elsif (s/^\?(?=\(\?)//) {
	    my $mods = $1;
	    my $cond = atom();
	    $re = re('conditional');
	    unshift(@{$re->{Kids}}, $cond);
	}
	elsif (s/^(\?[-\w]+)://) {
	    my $mods = $1;
	    local $extended = $extended;
	    local $insensitive = $insensitive;
	    local $multiline = $multiline;
	    local $singleline = $singleline;
	    setmods($mods);
	    $re = re('bracket');
	    $re->{Arep} = "($mods)";
	    $re->{x} = $extended || 0;
	    $re->{i} = $insensitive || 0;
	    $re->{m} = $multiline || 0;
	    $re->{s} = $singleline || 0;
	}
	elsif (s/^(\?[-\w]+)//) {
	    my $mods = $1;
	    $re = bless { Arep => "($mods)" }, "P5re::Mod";	
	    setmods($mods);
	    $re->{x} = $extended || 0;
	    $re->{i} = $insensitive || 0;
	    $re->{m} = $multiline || 0;
	    $re->{s} = $singleline || 0;
	}
	elsif (s/^\?//) {
	    $re = re('UNRECOGNIZED');
	}
	else {
	    my $brack = ++$maxbrack;
	    $re = re('capture');
	    $re->{Ato} = $brack;
	}

	if (not s/^\)//) { warn "Expected right paren at: '$_'" }
	return $re;
    }

    # special meta
    if (s/^\.//) {
	my $s = $singleline ? '.' : '\N';
	return bless { rep => '.', sem => $s }, "P5re::Meta";
    }
    if (s/^\^//) {
	my $s = $multiline ? '^^' : '^';
	return bless { rep => '^', sem => $s }, "P5re::Meta";
    }
    if (s/^\$(?:$|(?=[|)]))//) {
	my $s = $multiline ? '$$' : '$';
	return bless { rep => '$', sem => $s }, "P5re::Meta";
    }
    if (s/^([\$\@](\w+|.))//) {		# XXX need to handle subscripts here
	return bless { name => $1 }, "P5re::Var";
    }

    # character classes
    if (s/^\[//) {
	my $re = cclass();
	if (not s/^\]//) { warn "Expected right bracket at: '$_'" }
	return $re;
    }

    # backwhacks
    if (/^\\([1-9]\d*)/ and $1 <= $maxbrack) {
	my $to = $1;
	onechar();
	return bless { to => $to }, "P5re::Back";
    }

    # backwhacks
    if (/^\\(?=\w)/) {
	return bless { rep => onechar() }, "P5re::Meta";
    }

    # backwhacks
    if (s/^\\(.)//) {
	return bless { text => $1 }, "P5re::Char";
    }

    # optimization, would happen anyway
    if (s/^(\w+)//) { return bless { text => $1 }, "P5re::Char"; }

    # random character
    if (s/^(.)//) { return bless { text => $1 }, "P5re::Char"; }
}

sub cclass {
    my @cclass;
    my $cclass = "";
    my $neg = 0;
    if (s/^\^//) { $neg = 1 }
    if (s/^([\]\-])//) { $cclass .= $1 }

    while ($_ ne "" and not /^\]/) {
	# backwhacks
	if (/^\\(?=.)|.-/) {
	    my $o1 = onecharobj();
	    if ($cclass ne "") {
		push @cclass, bless { text => $cclass }, "P5re::Char";
		$cclass = "";
	    }

	    if (s/^-(?=[^]])//) {
		my $o2 = onecharobj();
		push @cclass, bless { Kids => [$o1, $o2] }, "P5re::Range";
	    }
	    else {
		push @cclass, $o1;
	    }
	}
	elsif (s/^(\[([:=.])\^?\w*\2\])//) {
	    if ($cclass ne "") {
		push @cclass, bless { text => $cclass }, "P5re::Char";
		$cclass = "";
	    }
	    push @cclass, bless { rep => $1 }, "P5re::Meta";
	}
	else {
	    $cclass .= onechar();
	}
    }

    if ($cclass ne "") {
	push @cclass, bless { text => $cclass }, "P5re::Char";
    }
    return bless { Kids => [@cclass], neg => $neg }, "P5re::CClass";
}

sub onecharobj {
    my $ch = onechar();
    if ($ch =~ /^\\/) {
	$ch = bless { rep => $ch }, "P5re::Meta";
    }
    else {
	$ch = bless { text => $ch }, "P5re::Char";
    }
}

sub onechar {
    die "Oops, short cclass" unless s/^(.)//;
    my $ch = $1;
    if ($ch eq '\\') {
	if (s/^([rntf]|[0-7]{1,4})//) { $ch .= $1 }
	elsif (s/^(x[0-9a-fA-f]{1,2})//) { $ch .= $1 }
	elsif (s/^(x\{[0-9a-fA-f]+\})//) { $ch .= $1 }
	elsif (s/^([NpP]\{.*?\})//) { $ch .= $1 }
	elsif (s/^([cpP].)//) { $ch .= $1 }
	elsif (s/^(.)//) { $ch .= $1 }
	else {
	    die "Oops, short backwhack";
	}
    }
    return $ch;
}

sub setmods {
    my $mods = shift;
    if ($mods =~ /\-.*x/) {
	$extended = 0;
    }
    elsif ($mods =~ /x/) {
	$extended = 1;
    }
    if ($mods =~ /\-.*i/) {
	$insensitive = 0;
    }
    elsif ($mods =~ /i/) {
	$insensitive = 1;
    }
    if ($mods =~ /\-.*m/) {
	$multiline = 0;
    }
    elsif ($mods =~ /m/) {
	$multiline = 1;
    }
    if ($mods =~ /\-.*s/) {
	$singleline = 0;
    }
    elsif ($mods =~ /s/) {
	$singleline = 1;
    }
}

1;
