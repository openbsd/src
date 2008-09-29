package CharClass::Matcher;
use strict;
use warnings;
use warnings FATAL => 'all';
use Text::Wrap qw(wrap);
use Encode;
use Data::Dumper;
$Data::Dumper::Useqq= 1;
our $hex_fmt= "0x%02X";

=head1 NAME

CharClass::Matcher -- Generate C macros that match character classes efficiently

=head1 SYNOPSIS

    perl Porting/regcharclass.pl

=head1 DESCRIPTION

Dynamically generates macros for detecting special charclasses
in latin-1, utf8, and codepoint forms. Macros can be set to return
the length (in bytes) of the matched codepoint, or the codepoint itself.

To regenerate regcharclass.h, run this script from perl-root. No arguments
are necessary.

Using WHATEVER as an example the following macros will be produced:

=over 4

=item is_WHATEVER(s,is_utf8)

=item is_WHATEVER_safe(s,e,is_utf8)

Do a lookup as appropriate based on the is_utf8 flag. When possible
comparisons involving octect<128 are done before checking the is_utf8
flag, hopefully saving time.

=item is_WHATEVER_utf8(s)

=item is_WHATEVER_utf8_safe(s,e)

Do a lookup assuming the string is encoded in (normalized) UTF8.

=item is_WHATEVER_latin1(s)

=item is_WHATEVER_latin1_safe(s,e)

Do a lookup assuming the string is encoded in latin-1 (aka plan octets).

=item is_WHATEVER_cp(cp)

Check to see if the string matches a given codepoint (hypotethically a
U32). The condition is constructed as as to "break out" as early as
possible if the codepoint is out of range of the condition.

IOW:

  (cp==X || (cp>X && (cp==Y || (cp>Y && ...))))

Thus if the character is X+1 only two comparisons will be done. Making
matching lookups slower, but non-matching faster.

=back

Additionally it is possible to generate C<what_> variants that return
the codepoint read instead of the number of octets read, this can be
done by suffixing '-cp' to the type description.

=head2 CODE FORMAT

perltidy  -st -bt=1 -bbt=0 -pt=0 -sbt=1 -ce -nwls== "%f"


=head1 AUTHOR

Author: Yves Orton (demerphq) 2007

=head1 BUGS

No tests directly here (although the regex engine will fail tests
if this code is broken). Insufficient documentation and no Getopts
handler for using the module as a script.

=head1 LICENSE

You may distribute under the terms of either the GNU General Public
License or the Artistic License, as specified in the README file.

=cut

# Sub naming convention:
# __func : private subroutine, can not be called as a method
# _func  : private method, not meant for external use
# func   : public method.

# private subs
#-------------------------------------------------------------------------------
#
# ($cp,$n,$l,$u)=__uni_latin($str);
#
# Return a list of arrays, each of which when interepreted correctly
# represent the string in some given encoding with specific conditions.
#
# $cp - list of codepoints that make up the string.
# $n  - list of octets that make up the string if all codepoints < 128
# $l  - list of octets that make up the string in latin1 encoding if all
#       codepoints < 256, and at least one codepoint is >127.
# $u  - list of octets that make up the string in utf8 if any codepoint >127
#
#   High CP | Defined
#-----------+----------
#   0 - 127 : $n
# 128 - 255 : $l, $u
# 256 - ... : $u
#

sub __uni_latin1 {
    my $str= shift;
    my $max= 0;
    my @cp;
    for my $ch ( split //, $str ) {
        my $cp= ord $ch;
        push @cp, $cp;
        $max= $cp if $max < $cp;
    }
    my ( $n, $l, $u );
    if ( $max < 128 ) {
        $n= [@cp];
    } else {
        $l= [@cp] if $max && $max < 256;

        my $copy= $str;    # must copy string, FB_CROAK makes encode destructive
        $u= eval { Encode::encode( "utf8", $copy, Encode::FB_CROAK ) };
        # $u is utf8 but with the utf8 flag OFF
        # therefore "C*" gets us the values of the bytes involved.
        $u= [ unpack "C*", $u ] if defined $u;
    }
    return ( \@cp, $n, $l, $u );
}

#
# $clean= __clean($expr);
#
# Cleanup a ternary expression, removing unnecessary parens and apply some
# simplifications using regexes.
#

sub __clean {
    my ( $expr )= @_;
    our $parens;
    $parens= qr/ (?> \( (?> (?: (?> [^()]+ ) | (??{ $parens }) )* ) \) ) /x;

    #print "$parens\n$expr\n";
    1 while $expr =~ s/ \( \s* ( $parens ) \s* \) /$1/gx;
    1 while $expr =~ s/ \( \s* ($parens) \s* \? \s*
        \( \s* ($parens) \s* \? \s* ($parens|[^:]+?) \s* : \s* ($parens|[^)]+?) \s* \)
        \s* : \s* \4 \s* \)/( ( $1 && $2 ) ? $3 : 0 )/gx;
    return $expr;
}

#
# $text= __macro(@args);
# Join args together by newlines, and then neatly add backslashes to the end
# of every  line as expected by the C pre-processor for #define's.
#

sub __macro {
    my $str= join "\n", @_;
    $str =~ s/\s*$//;
    my @lines= map { s/\s+$//; s/\t/        /g; $_ } split /\n/, $str;
    my $last= pop @lines;
    $str= join "\n", ( map { sprintf "%-76s\\", $_ } @lines ), $last;
    1 while $str =~ s/^(\t*) {8}/$1\t/gm;
    return $str . "\n";
}

#
# my $op=__incrdepth($op);
#
# take an 'op' hashref and add one to it and all its childrens depths.
#

sub __incrdepth {
    my $op= shift;
    return unless ref $op;
    $op->{depth} += 1;
    __incrdepth( $op->{yes} );
    __incrdepth( $op->{no} );
    return $op;
}

# join two branches of an opcode together with a condition, incrementing
# the depth on the yes branch when we do so.
# returns the new root opcode of the tree.
sub __cond_join {
    my ( $cond, $yes, $no )= @_;
    return {
        test  => $cond,
        yes   => __incrdepth( $yes ),
        no    => $no,
        depth => 0,
    };
}

# Methods

# constructor
#
# my $obj=CLASS->new(op=>'SOMENAME',title=>'blah',txt=>[..]);
#
# Create a new CharClass::Matcher object by parsing the text in
# the txt array. Currently applies the following rules:
#
# Element starts with C<0x>, line is evaled the result treated as
# a number which is passed to chr().
#
# Element starts with C<">, line is evaled and the result treated
# as a string.
#
# Each string is then stored in the 'strs' subhash as a hash record
# made up of the results of __uni_latin1, using the keynames
# 'low','latin1','utf8', as well as the synthesized 'LATIN1' and
# 'UTF8' which hold a merge of 'low' and their lowercase equivelents.
#
# Size data is tracked per type in the 'size' subhash.
#
# Return an object
#
sub new {
    my $class= shift;
    my %opt= @_;
    for ( qw(op txt) ) {
        die "in " . __PACKAGE__ . " constructor '$_;' is a mandatory field"
          if !exists $opt{$_};
    }

    my $self= bless {
        op    => $opt{op},
        title => $opt{title} || '',
    }, $class;
    foreach my $txt ( @{ $opt{txt} } ) {
        my $str= $txt;
        if ( $str =~ /^[""]/ ) {
            $str= eval $str;
        } elsif ( $str =~ /^0x/ ) {
            $str= chr eval $str;
        } elsif ( /\S/ ) {
            die "Unparseable line: $txt\n";
        } else {
            next;
        }
        my ( $cp, $low, $latin1, $utf8 )= __uni_latin1( $str );
        my $UTF8= $low   || $utf8;
        my $LATIN1= $low || $latin1;
        #die Dumper($txt,$cp,$low,$latin1,$utf8)
        #    if $txt=~/NEL/ or $utf8 and @$utf8>3;

        @{ $self->{strs}{$str} }{qw( str txt low utf8 latin1 cp UTF8 LATIN1 )}=
          ( $str, $txt, $low, $utf8, $latin1, $cp, $UTF8, $LATIN1 );
        my $rec= $self->{strs}{$str};
        foreach my $key ( qw(low utf8 latin1 cp UTF8 LATIN1) ) {
            $self->{size}{$key}{ 0 + @{ $self->{strs}{$str}{$key} } }++
              if $self->{strs}{$str}{$key};
        }
        $self->{has_multi} ||= @$cp > 1;
        $self->{has_ascii} ||= $latin1 && @$latin1;
        $self->{has_low}   ||= $low && @$low;
        $self->{has_high}  ||= !$low && !$latin1;
    }
    $self->{val_fmt}= $hex_fmt;
    $self->{count}= 0 + keys %{ $self->{strs} };
    return $self;
}

# my $trie = make_trie($type,$maxlen);
#
# using the data stored in the object build a trie of a specifc type,
# and with specific maximum depth. The trie is made up the elements of
# the given types array for each string in the object (assuming it is
# not too long.)
#
# returns the trie, or undef if there was no relevent data in the object.
#

sub make_trie {
    my ( $self, $type, $maxlen )= @_;

    my $strs= $self->{strs};
    my %trie;
    foreach my $rec ( values %$strs ) {
        die "panic: unknown type '$type'"
          if !exists $rec->{$type};
        my $dat= $rec->{$type};
        next unless $dat;
        next if $maxlen && @$dat > $maxlen;
        my $node= \%trie;
        foreach my $elem ( @$dat ) {
            $node->{$elem} ||= {};
            $node= $node->{$elem};
        }
        $node->{''}= $rec->{str};
    }
    return 0 + keys( %trie ) ? \%trie : undef;
}

# my $optree= _optree()
#
# recursively convert a trie to an optree where every node represents
# an if else branch.
#
#

sub _optree {
    my ( $self, $trie, $test_type, $ret_type, $else, $depth )= @_;
    return unless defined $trie;
    if ( $self->{has_multi} and $ret_type =~ /cp|both/ ) {
        die "Can't do 'cp' optree from multi-codepoint strings";
    }
    $ret_type ||= 'len';
    $else= 0  unless defined $else;
    $depth= 0 unless defined $depth;

    my @conds= sort { $a <=> $b } grep { length $_ } keys %$trie;
    if ( $trie->{''} ) {
        if ( $ret_type eq 'cp' ) {
            $else= $self->{strs}{ $trie->{''} }{cp}[0];
            $else= sprintf "$self->{val_fmt}", $else if $else > 9;
        } elsif ( $ret_type eq 'len' ) {
            $else= $depth;
        } elsif ( $ret_type eq 'both') {
            $else= $self->{strs}{ $trie->{''} }{cp}[0];
            $else= sprintf "$self->{val_fmt}", $else if $else > 9;
            $else= "len=$depth, $else";
        }
    }
    return $else if !@conds;
    my $node= {};
    my $root= $node;
    my ( $yes_res, $as_code, @cond );
    my $test= $test_type eq 'cp' ? "cp" : "((U8*)s)[$depth]";
    my $Update= sub {
        $node->{vals}= [@cond];
        $node->{test}= $test;
        $node->{yes}= $yes_res;
        $node->{depth}= $depth;
        $node->{no}= shift;
    };
    while ( @conds ) {
        my $cond= shift @conds;
        my $res=
          $self->_optree( $trie->{$cond}, $test_type, $ret_type, $else,
            $depth + 1 );
        my $res_code= Dumper( $res );
        if ( !$yes_res || $res_code ne $as_code ) {
            if ( $yes_res ) {
                $Update->( {} );
                $node= $node->{no};
            }
            ( $yes_res, $as_code )= ( $res, $res_code );
            @cond= ( $cond );
        } else {
            push @cond, $cond;
        }
    }
    $Update->( $else );
    return $root;
}

# my $optree= optree(%opts);
#
# Convert a trie to an optree, wrapper for _optree

sub optree {
    my $self= shift;
    my %opt= @_;
    my $trie= $self->make_trie( $opt{type}, $opt{max_depth} );
    $opt{ret_type} ||= 'len';
    my $test_type= $opt{type} eq 'cp' ? 'cp' : 'depth';
    return $self->_optree( $trie, $test_type, $opt{ret_type}, $opt{else}, 0 );
}

# my $optree= generic_optree(%opts);
#
# build a "generic" optree out of the three 'low', 'latin1', 'utf8'
# sets of strings, including a branch for handling the string type check.
#

sub generic_optree {
    my $self= shift;
    my %opt= @_;

    $opt{ret_type} ||= 'len';
    my $test_type= 'depth';
    my $else= $opt{else} || 0;

    my $latin1= $self->make_trie( 'latin1', $opt{max_depth} );
    my $utf8= $self->make_trie( 'utf8',     $opt{max_depth} );

    $_= $self->_optree( $_, $test_type, $opt{ret_type}, $else, 0 )
      for $latin1, $utf8;

    if ( $utf8 ) {
        $else= __cond_join( "( is_utf8 )", $utf8, $latin1 || $else );
    } elsif ( $latin1 ) {
        $else= __cond_join( "!( is_utf8 )", $latin1, $else );
    }
    my $low= $self->make_trie( 'low', $opt{max_depth} );
    if ( $low ) {
        $else= $self->_optree( $low, $test_type, $opt{ret_type}, $else, 0 );
    }

    return $else;
}

# length_optree()
#
# create a string length guarded optree.
#

sub length_optree {
    my $self= shift;
    my %opt= @_;
    my $type= $opt{type};

    die "Can't do a length_optree on type 'cp', makes no sense."
      if $type eq 'cp';

    my ( @size, $method );

    if ( $type eq 'generic' ) {
        $method= 'generic_optree';
        my %sizes= (
            %{ $self->{size}{low}    || {} },
            %{ $self->{size}{latin1} || {} },
            %{ $self->{size}{utf8}   || {} }
        );
        @size= sort { $a <=> $b } keys %sizes;
    } else {
        $method= 'optree';
        @size= sort { $a <=> $b } keys %{ $self->{size}{$type} };
    }

    my $else= ( $opt{else} ||= 0 );
    for my $size ( @size ) {
        my $optree= $self->$method( %opt, type => $type, max_depth => $size );
        my $cond= "((e)-(s) > " . ( $size - 1 ).")";
        $else= __cond_join( $cond, $optree, $else );
    }
    return $else;
}

# _cond_as_str
# turn a list of conditions into a text expression
# - merges ranges of conditions, and joins the result with ||
sub _cond_as_str {
    my ( $self, $op, $combine )= @_;
    my $cond= $op->{vals};
    my $test= $op->{test};
    return "( $test )" if !defined $cond;

    # rangify the list
    my @ranges;
    my $Update= sub {
        if ( @ranges ) {
            if ( $ranges[-1][0] == $ranges[-1][1] ) {
                $ranges[-1]= $ranges[-1][0];
            } elsif ( $ranges[-1][0] + 1 == $ranges[-1][1] ) {
                $ranges[-1]= $ranges[-1][0];
                push @ranges, $ranges[-1] + 1;
            }
        }
    };
    for my $cond ( @$cond ) {
        if ( !@ranges || $cond != $ranges[-1][1] + 1 ) {
            $Update->();
            push @ranges, [ $cond, $cond ];
        } else {
            $ranges[-1][1]++;
        }
    }
    $Update->();
    return $self->_combine( $test, @ranges )
      if $combine;
    @ranges= map {
        ref $_
          ? sprintf(
            "( $self->{val_fmt} <= $test && $test <= $self->{val_fmt} )",
            @$_ )
          : sprintf( "$self->{val_fmt} == $test", $_ );
    } @ranges;
    return "( " . join( " || ", @ranges ) . " )";
}

# _combine
# recursively turn a list of conditions into a fast break-out condition
# used by _cond_as_str() for 'cp' type macros.
sub _combine {
    my ( $self, $test, @cond )= @_;
    return if !@cond;
    my $item= shift @cond;
    my ( $cstr, $gtv );
    if ( ref $item ) {
        $cstr=
          sprintf( "( $self->{val_fmt} <= $test && $test <= $self->{val_fmt} )",
            @$item );
        $gtv= sprintf "$self->{val_fmt}", $item->[1];
    } else {
        $cstr= sprintf( "$self->{val_fmt} == $test", $item );
        $gtv= sprintf "$self->{val_fmt}", $item;
    }
    if ( @cond ) {
        return "( $cstr || ( $gtv < $test &&\n"
          . $self->_combine( $test, @cond ) . " ) )";
    } else {
        return $cstr;
    }
}

# _render()
# recursively convert an optree to text with reasonably neat formatting
sub _render {
    my ( $self, $op, $combine, $brace )= @_;
    if ( !ref $op ) {
        return $op;
    }
    my $cond= $self->_cond_as_str( $op, $combine );
    my $yes= $self->_render( $op->{yes}, $combine, 1 );
    my $no= $self->_render( $op->{no},   $combine, 0 );
    return "( $cond )" if $yes eq '1' and $no eq '0';
    my ( $lb, $rb )= $brace ? ( "( ", " )" ) : ( "", "" );
    return "$lb$cond ? $yes : $no$rb"
      if !ref( $op->{yes} ) && !ref( $op->{no} );
    my $ind1= " " x 4;
    my $ind= "\n" . ( $ind1 x $op->{depth} );

    if ( ref $op->{yes} ) {
        $yes= $ind . $ind1 . $yes;
    } else {
        $yes= " " . $yes;
    }

    return "$lb$cond ?$yes$ind: $no$rb";
}

# $expr=render($op,$combine)
#
# convert an optree to text with reasonably neat formatting. If $combine
# is true then the condition is created using "fast breakouts" which
# produce uglier expressions that are more efficient for common case,
# longer lists such as that resulting from type 'cp' output.
# Currently only used for type 'cp' macros.
sub render {
    my ( $self, $op, $combine )= @_;
    my $str= "( " . $self->_render( $op, $combine ) . " )";
    return __clean( $str );
}

# make_macro
# make a macro of a given type.
# calls into make_trie and (generic_|length_)optree as needed
# Opts are:
# type     : 'cp','generic','low','latin1','utf8','LATIN1','UTF8'
# ret_type : 'cp' or 'len'
# safe     : add length guards to macro
#
# type defaults to 'generic', and ret_type to 'len' unless type is 'cp'
# in which case it defaults to 'cp' as well.
#
# it is illegal to do a type 'cp' macro on a pattern with multi-codepoint
# sequences in it, as the generated macro will accept only a single codepoint
# as an argument.
#
# returns the macro.


sub make_macro {
    my $self= shift;
    my %opts= @_;
    my $type= $opts{type} || 'generic';
    die "Can't do a 'cp' on multi-codepoint character class '$self->{op}'"
      if $type eq 'cp'
      and $self->{has_multi};
    my $ret_type= $opts{ret_type} || ( $opts{type} eq 'cp' ? 'cp' : 'len' );
    my $method;
    if ( $opts{safe} ) {
        $method= 'length_optree';
    } elsif ( $type eq 'generic' ) {
        $method= 'generic_optree';
    } else {
        $method= 'optree';
    }
    my $optree= $self->$method( %opts, type => $type, ret_type => $ret_type );
    my $text= $self->render( $optree, $type eq 'cp' );
    my @args= $type eq 'cp' ? 'cp' : 's';
    push @args, "e" if $opts{safe};
    push @args, "is_utf8" if $type eq 'generic';
    push @args, "len" if $ret_type eq 'both';
    my $pfx= $ret_type eq 'both'    ? 'what_len_' : 
             $ret_type eq 'cp'      ? 'what_'     : 'is_';
    my $ext= $type     eq 'generic' ? ''          : '_' . lc( $type );
    $ext .= "_safe" if $opts{safe};
    my $argstr= join ",", @args;
    return "/*** GENERATED CODE ***/\n"
      . __macro( "#define $pfx$self->{op}$ext($argstr)\n$text" );
}

# if we arent being used as a module (highly likely) then process
# the __DATA__ below and produce macros in regcharclass.h
# if an argument is provided to the script then it is assumed to
# be the path of the file to output to, if the arg is '-' outputs
# to STDOUT.
if ( !caller ) {



    $|++;
    my $path= shift @ARGV;

    if ( !$path ) {
        $path= "regcharclass.h";
        if ( !-e $path ) { $path= "../$path" }
        if ( !-e $path ) { die "Can't find '$path' to update!\n" }
    }
    my $out_fh;
    if ( $path eq '-' ) {
        $out_fh= \*STDOUT;
    } else {
        rename $path, "$path.bak";
        open $out_fh, ">", $path
          or die "Can't write to '$path':$!";
        binmode $out_fh;    # want unix line endings even when run on win32.
    }
    my ( $zero )= $0 =~ /([^\\\/]+)$/;
    print $out_fh <<"HEADER";
/*  -*- buffer-read-only: t -*-
 *
 *    regcharclass.h
 *
 *    Copyright (C) 2007, by Larry Wall and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 * !!!!!!!   DO NOT EDIT THIS FILE   !!!!!!!
 * This file is built by Porting/$zero.
 * 
 * Any changes made here will be lost!
 *
 */

HEADER

    my ( $op, $title, @txt, @types, @mods );
    my $doit= sub {
        return unless $op;
        print $out_fh "/*\n\t$op: $title\n\n";
        print $out_fh join "\n", ( map { "\t$_" } @txt ), "*/", "";
        my $obj= __PACKAGE__->new( op => $op, title => $title, txt => \@txt );

        #die Dumper(\@types,\@mods);

        foreach my $type_spec ( @types ) {
            my ( $type, $ret )= split /-/, $type_spec;
            $ret ||= 'len';
            foreach my $mod ( @mods ) {
                next if $mod eq 'safe' and $type eq 'cp';
                my $macro= $obj->make_macro(
                    type     => $type,
                    ret_type => $ret,
                    safe     => $mod eq 'safe'
                );
                print $out_fh $macro, "\n";
            }
        }
    };

    while ( <DATA> ) {
        s/^\s*#//;
        next unless /\S/;
        chomp;
        if ( /^([A-Z]+)/ ) {
            $doit->();
            ( $op, $title )= split /\s*:\s*/, $_, 2;
            @txt= ();
        } elsif ( s/^=>// ) {
            my ( $type, $modifier )= split /:/, $_;
            @types= split ' ', $type;
            @mods= split ' ',  $modifier;
        } else {
            push @txt, "$_";
        }
    }
    $doit->();
    print $out_fh "/* ex: set ro: */\n";
    print "updated $path\n" if $path ne '-';
}

#
# Valid types: generic, LATIN1, UTF8, low, latin1, utf8
# default return value is octects read.
# append -cp to make it codepoint matched.
# modifiers come after the colon, valid possibilities
# being 'fast' and 'safe'.
#
1; # in the unlikely case we are being used as a module

__DATA__
LNBREAK: Line Break: \R
=> generic UTF8 LATIN1 :fast safe
"\x0D\x0A"      # CRLF - Network (Windows) line ending
0x0A            # LF  | LINE FEED
0x0B            # VT  | VERTICAL TAB
0x0C            # FF  | FORM FEED
0x0D            # CR  | CARRIAGE RETURN
0x85            # NEL | NEXT LINE
0x2028          # LINE SEPARATOR
0x2029          # PARAGRAPH SEPARATOR

HORIZWS: Horizontal Whitespace: \h \H
=> generic UTF8 LATIN1 cp :fast safe
0x09            # HT
0x20            # SPACE
0xa0            # NBSP
0x1680          # OGHAM SPACE MARK
0x180e          # MONGOLIAN VOWEL SEPARATOR
0x2000          # EN QUAD
0x2001          # EM QUAD
0x2002          # EN SPACE
0x2003          # EM SPACE
0x2004          # THREE-PER-EM SPACE
0x2005          # FOUR-PER-EM SPACE
0x2006          # SIX-PER-EM SPACE
0x2007          # FIGURE SPACE
0x2008          # PUNCTUATION SPACE
0x2009          # THIN SPACE
0x200A          # HAIR SPACE
0x202f          # NARROW NO-BREAK SPACE
0x205f          # MEDIUM MATHEMATICAL SPACE
0x3000          # IDEOGRAPHIC SPACE

VERTWS: Vertical Whitespace: \v \V
=> generic UTF8 LATIN1 cp :fast safe
0x0A            # LF
0x0B            # VT
0x0C            # FF
0x0D            # CR
0x85            # NEL
0x2028          # LINE SEPARATOR
0x2029          # PARAGRAPH SEPARATOR


TRICKYFOLD: Problematic fold case letters.
=> generic cp generic-cp generic-both :fast safe
0x00DF	# LATIN1 SMALL LETTER SHARP S
0x0390	# GREEK SMALL LETTER IOTA WITH DIALYTIKA AND TONOS
0x03B0	# GREEK SMALL LETTER UPSILON WITH DIALYTIKA AND TONOS


