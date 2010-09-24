package P5AST;

$::herequeue = '';

1;

{
    my %newkey = qw(
    );

    sub translate {
	my $class = shift;
	my $key = shift;
	$key = $newkey{$key} || "op_$key";
	return "P5AST::$key";
    }
}

sub new {
    my $class = shift;
    bless {@_}, $class;
}

sub AUTOLOAD {
    warn "AUTOLOAD $P5AST::AUTOLOAD(" . join(',', @_) . ")\n";
}

sub DESTROY { }

sub p5arraytext {
    my $kid = shift;
    my $text = "";
    for my $subkid (@$kid) {
	my $type = ref $subkid;
	if ($type eq 'ARRAY') {
	    if ($dowarn) {
		warn "Extra array\n";
		$text .= '〔 '. p5arraytext($subkid) . ' 〕';
	    }
	    else {
		$text .= p5arraytext($subkid);
	    }
	}
	elsif ($type =~ /^p5::/) {
	    my $newtext = $subkid->enc();
	    if ($::herequeue && $newtext =~ s/\n/\n$::herequeue/) {
		$::herequeue = '';
	    }
	    $text .= $newtext;
	}
	elsif ($type) {
	    $text .= $subkid->text(@_);
	}
	else {
	    $text .= $subkid;
	}
    }
    return $text;
}

sub p5text {
    my $self = shift;
#    my $pre = $self->pretext();
#    my $post = $self->posttext();
    my $text = "";
    foreach my $kid (@{$$self{Kids}}) {
	my $type = ref $kid;
	if ($type eq 'ARRAY') {
	    $text .= p5arraytext($kid);
	}
	elsif ($type =~ /^p5::/) {
	    my $newtext = $kid->enc();
	    if ($::herequeue && $newtext =~ s/\n/\n$::herequeue/) {
		$::herequeue = '';
	    }
	    $text .= $newtext;
	}
        elsif ($type eq "chomp") {
            $text =~ s/\n$//g;
        }
	elsif ($type) {
	    $text .= $kid->p5text(@_);
	}
	elsif (defined $kid) {
	    $text .= $kid;
	}
	else {
	    $text .= '[[[ UNDEF ]]]';
	}
    }
    return $text;
}

sub p5subtext {
    my $self = shift;
    my @text;
    foreach my $kid (@{$$self{Kids}}) {
	my $text = $kid->p5text(@_);
	push @text, $text if defined $text;
    }
    return @text;
}

sub p6text {
    return $_[0]->p5text();	# assume it's the same
}

package P5AST::heredoc; @ISA = 'P5AST';

sub p5text {
    my $self = shift;
    my $newdoc;
    {
	local $::herequeue;			# don't interpolate outer heredoc yet
	$newdoc = $self->{doc}->p5text(@_) .  $self->{end}->enc();
	if ($::herequeue) {			# heredoc within the heredoc?
	    $newdoc .= $::herequeue;
	    $::herequeue = '';
	}
    }
    $::herequeue .= $newdoc;
    my $start = $self->{start};
    my $type = ref $start;
    if ($type =~ /^p5::/) {		# XXX too much cut-n-paste here...
	return $start->enc();
    }
    elsif ($type) {
	return $start->p5text(@_);
    }
    else {
	return $start;
    }
}

package P5AST::BAD;

sub p5text {
    my $self = shift;
    my $t = ref $t;
    warn "Shouldn't have a node of type $t";
}

package P5AST::baseop; 		@ISA = 'P5AST';
package P5AST::baseop_unop; 	@ISA = 'P5AST::baseop';
package P5AST::binop; 		@ISA = 'P5AST::baseop';
package P5AST::cop; 		@ISA = 'P5AST::baseop';
package P5AST::filestatop; 	@ISA = 'P5AST::baseop';
package P5AST::listop; 		@ISA = 'P5AST::baseop';
package P5AST::logop; 		@ISA = 'P5AST::baseop';
package P5AST::loop; 		@ISA = 'P5AST::baseop';
package P5AST::loopexop; 	@ISA = 'P5AST::baseop';
package P5AST::padop; 		@ISA = 'P5AST::baseop';
package P5AST::padop_svop; 	@ISA = 'P5AST::baseop';
package P5AST::pmop; 		@ISA = 'P5AST::baseop';
package P5AST::pvop_svop; 	@ISA = 'P5AST::baseop';
package P5AST::unop; 		@ISA = 'P5AST::baseop';

# Nothing.

package P5AST::op_null; 	@ISA = 'P5AST::baseop';
package P5AST::op_stub; 	@ISA = 'P5AST::baseop';
package P5AST::op_scalar; 	@ISA = 'P5AST::baseop_unop';

# Pushy stuff.

package P5AST::op_pushmark; 	@ISA = 'P5AST::baseop';
package P5AST::op_wantarray; 	@ISA = 'P5AST::baseop';
package P5AST::op_const; 	@ISA = 'P5AST::padop_svop';
package P5AST::op_gvsv; 	@ISA = 'P5AST::padop_svop';
package P5AST::op_gv; 		@ISA = 'P5AST::padop_svop';
package P5AST::op_gelem; 	@ISA = 'P5AST::binop';
package P5AST::op_padsv; 	@ISA = 'P5AST::baseop';
package P5AST::op_padav; 	@ISA = 'P5AST::baseop';
package P5AST::op_padhv; 	@ISA = 'P5AST::baseop';
package P5AST::op_padany; 	@ISA = 'P5AST::baseop';
package P5AST::op_pushre; 	@ISA = 'P5AST::pmop';
package P5AST::op_rv2gv; 	@ISA = 'P5AST::unop';
package P5AST::op_rv2sv; 	@ISA = 'P5AST::unop';
package P5AST::op_av2arylen; 	@ISA = 'P5AST::unop';
package P5AST::op_rv2cv; 	@ISA = 'P5AST::unop';
package P5AST::op_anoncode; 	@ISA = 'P5AST::padop_svop';
package P5AST::op_prototype; 	@ISA = 'P5AST::baseop_unop';
package P5AST::op_refgen; 	@ISA = 'P5AST::unop';
package P5AST::op_srefgen; 	@ISA = 'P5AST::unop';
package P5AST::op_ref; 		@ISA = 'P5AST::baseop_unop';
package P5AST::op_bless; 	@ISA = 'P5AST::listop';
package P5AST::op_backtick; 	@ISA = 'P5AST::baseop_unop';
package P5AST::op_glob; 	@ISA = 'P5AST::listop';
package P5AST::op_readline; 	@ISA = 'P5AST::baseop_unop';
package P5AST::op_rcatline; 	@ISA = 'P5AST::padop_svop';
package P5AST::op_regcmaybe; 	@ISA = 'P5AST::unop';
package P5AST::op_regcreset; 	@ISA = 'P5AST::unop';
package P5AST::op_regcomp; 	@ISA = 'P5AST::logop';
package P5AST::op_match; 	@ISA = 'P5AST::pmop';
package P5AST::op_qr; 		@ISA = 'P5AST::pmop';
package P5AST::op_subst; 	@ISA = 'P5AST::pmop';
package P5AST::op_substcont; 	@ISA = 'P5AST::logop';
package P5AST::op_trans; 	@ISA = 'P5AST::pvop_svop';
package P5AST::op_sassign; 	@ISA = 'P5AST::baseop';
package P5AST::op_aassign; 	@ISA = 'P5AST::binop';
package P5AST::op_chop; 	@ISA = 'P5AST::baseop_unop';
package P5AST::op_schop; 	@ISA = 'P5AST::baseop_unop';
package P5AST::op_chomp; 	@ISA = 'P5AST::baseop_unop';
package P5AST::op_schomp; 	@ISA = 'P5AST::baseop_unop';
package P5AST::op_defined; 	@ISA = 'P5AST::baseop_unop';
package P5AST::op_undef; 	@ISA = 'P5AST::baseop_unop';
package P5AST::op_study; 	@ISA = 'P5AST::baseop_unop';
package P5AST::op_pos; 		@ISA = 'P5AST::baseop_unop';
package P5AST::op_preinc; 	@ISA = 'P5AST::unop';
package P5AST::op_i_preinc; 	@ISA = 'P5AST::unop';
package P5AST::op_predec; 	@ISA = 'P5AST::unop';
package P5AST::op_i_predec; 	@ISA = 'P5AST::unop';
package P5AST::op_postinc; 	@ISA = 'P5AST::unop';
package P5AST::op_i_postinc; 	@ISA = 'P5AST::unop';
package P5AST::op_postdec; 	@ISA = 'P5AST::unop';
package P5AST::op_i_postdec; 	@ISA = 'P5AST::unop';
package P5AST::op_pow; 		@ISA = 'P5AST::binop';
package P5AST::op_multiply; 	@ISA = 'P5AST::binop';
package P5AST::op_i_multiply; 	@ISA = 'P5AST::binop';
package P5AST::op_divide; 	@ISA = 'P5AST::binop';
package P5AST::op_i_divide; 	@ISA = 'P5AST::binop';
package P5AST::op_modulo; 	@ISA = 'P5AST::binop';
package P5AST::op_i_modulo; 	@ISA = 'P5AST::binop';
package P5AST::op_repeat; 	@ISA = 'P5AST::binop';
package P5AST::op_add; 		@ISA = 'P5AST::binop';
package P5AST::op_i_add; 	@ISA = 'P5AST::binop';
package P5AST::op_subtract; 	@ISA = 'P5AST::binop';
package P5AST::op_i_subtract; 	@ISA = 'P5AST::binop';
package P5AST::op_concat; 	@ISA = 'P5AST::binop';
package P5AST::op_stringify; 	@ISA = 'P5AST::listop';
package P5AST::op_left_shift; 	@ISA = 'P5AST::binop';
package P5AST::op_right_shift; 	@ISA = 'P5AST::binop';
package P5AST::op_lt; 		@ISA = 'P5AST::binop';
package P5AST::op_i_lt; 	@ISA = 'P5AST::binop';
package P5AST::op_gt; 		@ISA = 'P5AST::binop';
package P5AST::op_i_gt; 	@ISA = 'P5AST::binop';
package P5AST::op_le; 		@ISA = 'P5AST::binop';
package P5AST::op_i_le; 	@ISA = 'P5AST::binop';
package P5AST::op_ge; 		@ISA = 'P5AST::binop';
package P5AST::op_i_ge; 	@ISA = 'P5AST::binop';
package P5AST::op_eq; 		@ISA = 'P5AST::binop';
package P5AST::op_i_eq; 	@ISA = 'P5AST::binop';
package P5AST::op_ne; 		@ISA = 'P5AST::binop';
package P5AST::op_i_ne; 	@ISA = 'P5AST::binop';
package P5AST::op_ncmp; 	@ISA = 'P5AST::binop';
package P5AST::op_i_ncmp; 	@ISA = 'P5AST::binop';
package P5AST::op_slt; 		@ISA = 'P5AST::binop';
package P5AST::op_sgt; 		@ISA = 'P5AST::binop';
package P5AST::op_sle; 		@ISA = 'P5AST::binop';
package P5AST::op_sge; 		@ISA = 'P5AST::binop';
package P5AST::op_seq; 		@ISA = 'P5AST::binop';
package P5AST::op_sne; 		@ISA = 'P5AST::binop';
package P5AST::op_scmp; 	@ISA = 'P5AST::binop';
package P5AST::op_bit_and; 	@ISA = 'P5AST::binop';
package P5AST::op_bit_xor; 	@ISA = 'P5AST::binop';
package P5AST::op_bit_or; 	@ISA = 'P5AST::binop';
package P5AST::op_negate; 	@ISA = 'P5AST::unop';
package P5AST::op_i_negate; 	@ISA = 'P5AST::unop';
package P5AST::op_not; 		@ISA = 'P5AST::unop';
package P5AST::op_complement; 	@ISA = 'P5AST::unop';
package P5AST::op_atan2; 	@ISA = 'P5AST::listop';
package P5AST::op_sin; 		@ISA = 'P5AST::baseop_unop';
package P5AST::op_cos; 		@ISA = 'P5AST::baseop_unop';
package P5AST::op_rand; 	@ISA = 'P5AST::baseop_unop';
package P5AST::op_srand; 	@ISA = 'P5AST::baseop_unop';
package P5AST::op_exp; 		@ISA = 'P5AST::baseop_unop';
package P5AST::op_log; 		@ISA = 'P5AST::baseop_unop';
package P5AST::op_sqrt; 	@ISA = 'P5AST::baseop_unop';
package P5AST::op_int; 		@ISA = 'P5AST::baseop_unop';
package P5AST::op_hex; 		@ISA = 'P5AST::baseop_unop';
package P5AST::op_oct; 		@ISA = 'P5AST::baseop_unop';
package P5AST::op_abs; 		@ISA = 'P5AST::baseop_unop';
package P5AST::op_length; 	@ISA = 'P5AST::baseop_unop';
package P5AST::op_substr; 	@ISA = 'P5AST::listop';
package P5AST::op_vec; 		@ISA = 'P5AST::listop';
package P5AST::op_index; 	@ISA = 'P5AST::listop';
package P5AST::op_rindex; 	@ISA = 'P5AST::listop';
package P5AST::op_sprintf; 	@ISA = 'P5AST::listop';
package P5AST::op_formline; 	@ISA = 'P5AST::listop';
package P5AST::op_ord; 		@ISA = 'P5AST::baseop_unop';
package P5AST::op_chr; 		@ISA = 'P5AST::baseop_unop';
package P5AST::op_crypt; 	@ISA = 'P5AST::listop';
package P5AST::op_ucfirst; 	@ISA = 'P5AST::baseop_unop';
package P5AST::op_lcfirst; 	@ISA = 'P5AST::baseop_unop';
package P5AST::op_uc; 		@ISA = 'P5AST::baseop_unop';
package P5AST::op_lc; 		@ISA = 'P5AST::baseop_unop';
package P5AST::op_quotemeta; 	@ISA = 'P5AST::baseop_unop';
package P5AST::op_rv2av; 	@ISA = 'P5AST::unop';
package P5AST::op_aelemfast; 	@ISA = 'P5AST::padop_svop';
package P5AST::op_aelem; 	@ISA = 'P5AST::binop';
package P5AST::op_aslice; 	@ISA = 'P5AST::listop';
package P5AST::op_each; 	@ISA = 'P5AST::baseop_unop';
package P5AST::op_values; 	@ISA = 'P5AST::baseop_unop';
package P5AST::op_keys; 	@ISA = 'P5AST::baseop_unop';
package P5AST::op_delete; 	@ISA = 'P5AST::baseop_unop';
package P5AST::op_exists; 	@ISA = 'P5AST::baseop_unop';
package P5AST::op_rv2hv; 	@ISA = 'P5AST::unop';
package P5AST::op_helem; 	@ISA = 'P5AST::listop';
package P5AST::op_hslice; 	@ISA = 'P5AST::listop';
package P5AST::op_unpack; 	@ISA = 'P5AST::listop';
package P5AST::op_pack; 	@ISA = 'P5AST::listop';
package P5AST::op_split; 	@ISA = 'P5AST::listop';
package P5AST::op_join; 	@ISA = 'P5AST::listop';
package P5AST::op_list; 	@ISA = 'P5AST::listop';
package P5AST::op_lslice; 	@ISA = 'P5AST::binop';
package P5AST::op_anonlist; 	@ISA = 'P5AST::listop';
package P5AST::op_anonhash; 	@ISA = 'P5AST::listop';
package P5AST::op_splice; 	@ISA = 'P5AST::listop';
package P5AST::op_push; 	@ISA = 'P5AST::listop';
package P5AST::op_pop; 		@ISA = 'P5AST::baseop_unop';
package P5AST::op_shift; 	@ISA = 'P5AST::baseop_unop';
package P5AST::op_unshift; 	@ISA = 'P5AST::listop';
package P5AST::op_sort; 	@ISA = 'P5AST::listop';
package P5AST::op_reverse; 	@ISA = 'P5AST::listop';
package P5AST::op_grepstart; 	@ISA = 'P5AST::listop';
package P5AST::op_grepwhile; 	@ISA = 'P5AST::logop';
package P5AST::op_mapstart; 	@ISA = 'P5AST::listop';
package P5AST::op_mapwhile; 	@ISA = 'P5AST::logop';
package P5AST::op_range; 	@ISA = 'P5AST::logop';
package P5AST::op_flip; 	@ISA = 'P5AST::unop';
package P5AST::op_flop; 	@ISA = 'P5AST::unop';
package P5AST::op_and; 		@ISA = 'P5AST::logop';
package P5AST::op_or; 		@ISA = 'P5AST::logop';
package P5AST::op_xor; 		@ISA = 'P5AST::binop';
package P5AST::op_cond_expr; 	@ISA = 'P5AST::logop';
package P5AST::op_andassign; 	@ISA = 'P5AST::logop';
package P5AST::op_orassign; 	@ISA = 'P5AST::logop';
package P5AST::op_method; 	@ISA = 'P5AST::unop';
package P5AST::op_entersub; 	@ISA = 'P5AST::unop';
package P5AST::op_leavesub; 	@ISA = 'P5AST::unop';
package P5AST::op_leavesublv; 	@ISA = 'P5AST::unop';
package P5AST::op_caller; 	@ISA = 'P5AST::baseop_unop';
package P5AST::op_warn; 	@ISA = 'P5AST::listop';
package P5AST::op_die; 		@ISA = 'P5AST::listop';
package P5AST::op_reset; 	@ISA = 'P5AST::baseop_unop';
package P5AST::op_lineseq; 	@ISA = 'P5AST::listop';
package P5AST::op_nextstate; 	@ISA = 'P5AST::BAD';
package P5AST::op_dbstate; 	@ISA = 'P5AST::cop';
package P5AST::op_unstack; 	@ISA = 'P5AST::baseop';
package P5AST::op_enter; 	@ISA = 'P5AST::baseop';
package P5AST::op_leave; 	@ISA = 'P5AST::listop';
package P5AST::op_scope; 	@ISA = 'P5AST::listop';
package P5AST::op_enteriter; 	@ISA = 'P5AST::loop';
package P5AST::op_iter; 	@ISA = 'P5AST::baseop';
package P5AST::op_enterloop; 	@ISA = 'P5AST::loop';
package P5AST::op_leaveloop; 	@ISA = 'P5AST::binop';
package P5AST::op_return; 	@ISA = 'P5AST::listop';
package P5AST::op_last; 	@ISA = 'P5AST::loopexop';
package P5AST::op_next; 	@ISA = 'P5AST::loopexop';
package P5AST::op_redo; 	@ISA = 'P5AST::loopexop';
package P5AST::op_dump; 	@ISA = 'P5AST::loopexop';
package P5AST::op_goto; 	@ISA = 'P5AST::loopexop';
package P5AST::op_exit; 	@ISA = 'P5AST::baseop_unop';
package P5AST::op_open; 	@ISA = 'P5AST::listop';
package P5AST::op_close; 	@ISA = 'P5AST::baseop_unop';
package P5AST::op_pipe_op; 	@ISA = 'P5AST::listop';
package P5AST::op_fileno; 	@ISA = 'P5AST::baseop_unop';
package P5AST::op_umask; 	@ISA = 'P5AST::baseop_unop';
package P5AST::op_binmode; 	@ISA = 'P5AST::listop';
package P5AST::op_tie; 		@ISA = 'P5AST::listop';
package P5AST::op_untie; 	@ISA = 'P5AST::baseop_unop';
package P5AST::op_tied; 	@ISA = 'P5AST::baseop_unop';
package P5AST::op_dbmopen; 	@ISA = 'P5AST::listop';
package P5AST::op_dbmclose; 	@ISA = 'P5AST::baseop_unop';
package P5AST::op_sselect; 	@ISA = 'P5AST::listop';
package P5AST::op_select; 	@ISA = 'P5AST::listop';
package P5AST::op_getc; 	@ISA = 'P5AST::baseop_unop';
package P5AST::op_read; 	@ISA = 'P5AST::listop';
package P5AST::op_enterwrite; 	@ISA = 'P5AST::baseop_unop';
package P5AST::op_leavewrite; 	@ISA = 'P5AST::unop';
package P5AST::op_prtf; 	@ISA = 'P5AST::listop';
package P5AST::op_print; 	@ISA = 'P5AST::listop';
package P5AST::op_say;		@ISA = 'P5AST::listop';
package P5AST::op_sysopen; 	@ISA = 'P5AST::listop';
package P5AST::op_sysseek; 	@ISA = 'P5AST::listop';
package P5AST::op_sysread; 	@ISA = 'P5AST::listop';
package P5AST::op_syswrite; 	@ISA = 'P5AST::listop';
package P5AST::op_send; 	@ISA = 'P5AST::listop';
package P5AST::op_recv; 	@ISA = 'P5AST::listop';
package P5AST::op_eof; 		@ISA = 'P5AST::baseop_unop';
package P5AST::op_tell; 	@ISA = 'P5AST::baseop_unop';
package P5AST::op_seek; 	@ISA = 'P5AST::listop';
package P5AST::op_truncate; 	@ISA = 'P5AST::listop';
package P5AST::op_fcntl; 	@ISA = 'P5AST::listop';
package P5AST::op_ioctl; 	@ISA = 'P5AST::listop';
package P5AST::op_flock; 	@ISA = 'P5AST::listop';
package P5AST::op_socket; 	@ISA = 'P5AST::listop';
package P5AST::op_sockpair; 	@ISA = 'P5AST::listop';
package P5AST::op_bind; 	@ISA = 'P5AST::listop';
package P5AST::op_connect; 	@ISA = 'P5AST::listop';
package P5AST::op_listen; 	@ISA = 'P5AST::listop';
package P5AST::op_accept; 	@ISA = 'P5AST::listop';
package P5AST::op_shutdown; 	@ISA = 'P5AST::listop';
package P5AST::op_gsockopt; 	@ISA = 'P5AST::listop';
package P5AST::op_ssockopt; 	@ISA = 'P5AST::listop';
package P5AST::op_getsockname; 	@ISA = 'P5AST::baseop_unop';
package P5AST::op_getpeername; 	@ISA = 'P5AST::baseop_unop';
package P5AST::op_lstat; 	@ISA = 'P5AST::filestatop';
package P5AST::op_stat; 	@ISA = 'P5AST::filestatop';
package P5AST::op_ftrread; 	@ISA = 'P5AST::filestatop';
package P5AST::op_ftrwrite; 	@ISA = 'P5AST::filestatop';
package P5AST::op_ftrexec; 	@ISA = 'P5AST::filestatop';
package P5AST::op_fteread; 	@ISA = 'P5AST::filestatop';
package P5AST::op_ftewrite; 	@ISA = 'P5AST::filestatop';
package P5AST::op_fteexec; 	@ISA = 'P5AST::filestatop';
package P5AST::op_ftis; 	@ISA = 'P5AST::filestatop';
package P5AST::op_fteowned; 	@ISA = 'P5AST::filestatop';
package P5AST::op_ftrowned; 	@ISA = 'P5AST::filestatop';
package P5AST::op_ftzero; 	@ISA = 'P5AST::filestatop';
package P5AST::op_ftsize; 	@ISA = 'P5AST::filestatop';
package P5AST::op_ftmtime; 	@ISA = 'P5AST::filestatop';
package P5AST::op_ftatime; 	@ISA = 'P5AST::filestatop';
package P5AST::op_ftctime; 	@ISA = 'P5AST::filestatop';
package P5AST::op_ftsock; 	@ISA = 'P5AST::filestatop';
package P5AST::op_ftchr; 	@ISA = 'P5AST::filestatop';
package P5AST::op_ftblk; 	@ISA = 'P5AST::filestatop';
package P5AST::op_ftfile; 	@ISA = 'P5AST::filestatop';
package P5AST::op_ftdir; 	@ISA = 'P5AST::filestatop';
package P5AST::op_ftpipe; 	@ISA = 'P5AST::filestatop';
package P5AST::op_ftlink; 	@ISA = 'P5AST::filestatop';
package P5AST::op_ftsuid; 	@ISA = 'P5AST::filestatop';
package P5AST::op_ftsgid; 	@ISA = 'P5AST::filestatop';
package P5AST::op_ftsvtx; 	@ISA = 'P5AST::filestatop';
package P5AST::op_fttty; 	@ISA = 'P5AST::filestatop';
package P5AST::op_fttext; 	@ISA = 'P5AST::filestatop';
package P5AST::op_ftbinary; 	@ISA = 'P5AST::filestatop';
package P5AST::op_chdir; 	@ISA = 'P5AST::baseop_unop';
package P5AST::op_chown; 	@ISA = 'P5AST::listop';
package P5AST::op_chroot; 	@ISA = 'P5AST::baseop_unop';
package P5AST::op_unlink; 	@ISA = 'P5AST::listop';
package P5AST::op_chmod; 	@ISA = 'P5AST::listop';
package P5AST::op_utime; 	@ISA = 'P5AST::listop';
package P5AST::op_rename; 	@ISA = 'P5AST::listop';
package P5AST::op_link; 	@ISA = 'P5AST::listop';
package P5AST::op_symlink; 	@ISA = 'P5AST::listop';
package P5AST::op_readlink; 	@ISA = 'P5AST::baseop_unop';
package P5AST::op_mkdir; 	@ISA = 'P5AST::listop';
package P5AST::op_rmdir; 	@ISA = 'P5AST::baseop_unop';
package P5AST::op_open_dir; 	@ISA = 'P5AST::listop';
package P5AST::op_readdir; 	@ISA = 'P5AST::baseop_unop';
package P5AST::op_telldir; 	@ISA = 'P5AST::baseop_unop';
package P5AST::op_seekdir; 	@ISA = 'P5AST::listop';
package P5AST::op_rewinddir; 	@ISA = 'P5AST::baseop_unop';
package P5AST::op_closedir; 	@ISA = 'P5AST::baseop_unop';
package P5AST::op_fork; 	@ISA = 'P5AST::baseop';
package P5AST::op_wait; 	@ISA = 'P5AST::baseop';
package P5AST::op_waitpid; 	@ISA = 'P5AST::listop';
package P5AST::op_system; 	@ISA = 'P5AST::listop';
package P5AST::op_exec; 	@ISA = 'P5AST::listop';
package P5AST::op_kill; 	@ISA = 'P5AST::listop';
package P5AST::op_getppid; 	@ISA = 'P5AST::baseop';
package P5AST::op_getpgrp; 	@ISA = 'P5AST::baseop_unop';
package P5AST::op_setpgrp; 	@ISA = 'P5AST::listop';
package P5AST::op_getpriority; 	@ISA = 'P5AST::listop';
package P5AST::op_setpriority; 	@ISA = 'P5AST::listop';
package P5AST::op_time; 	@ISA = 'P5AST::baseop';
package P5AST::op_tms;	 	@ISA = 'P5AST::baseop';
package P5AST::op_localtime; 	@ISA = 'P5AST::baseop_unop';
package P5AST::op_gmtime; 	@ISA = 'P5AST::baseop_unop';
package P5AST::op_alarm; 	@ISA = 'P5AST::baseop_unop';
package P5AST::op_sleep; 	@ISA = 'P5AST::baseop_unop';
package P5AST::op_shmget; 	@ISA = 'P5AST::listop';
package P5AST::op_shmctl; 	@ISA = 'P5AST::listop';
package P5AST::op_shmread; 	@ISA = 'P5AST::listop';
package P5AST::op_shmwrite; 	@ISA = 'P5AST::listop';
package P5AST::op_msgget; 	@ISA = 'P5AST::listop';
package P5AST::op_msgctl; 	@ISA = 'P5AST::listop';
package P5AST::op_msgsnd; 	@ISA = 'P5AST::listop';
package P5AST::op_msgrcv; 	@ISA = 'P5AST::listop';
package P5AST::op_semget; 	@ISA = 'P5AST::listop';
package P5AST::op_semctl; 	@ISA = 'P5AST::listop';
package P5AST::op_semop; 	@ISA = 'P5AST::listop';
package P5AST::op_require; 	@ISA = 'P5AST::baseop_unop';
package P5AST::op_dofile; 	@ISA = 'P5AST::unop';
package P5AST::op_entereval; 	@ISA = 'P5AST::baseop_unop';
package P5AST::op_leaveeval; 	@ISA = 'P5AST::unop';
package P5AST::op_entertry; 	@ISA = 'P5AST::logop';
package P5AST::op_leavetry; 	@ISA = 'P5AST::listop';
package P5AST::op_ghbyname; 	@ISA = 'P5AST::baseop_unop';
package P5AST::op_ghbyaddr; 	@ISA = 'P5AST::listop';
package P5AST::op_ghostent; 	@ISA = 'P5AST::baseop';
package P5AST::op_gnbyname; 	@ISA = 'P5AST::baseop_unop';
package P5AST::op_gnbyaddr; 	@ISA = 'P5AST::listop';
package P5AST::op_gnetent; 	@ISA = 'P5AST::baseop';
package P5AST::op_gpbyname; 	@ISA = 'P5AST::baseop_unop';
package P5AST::op_gpbynumber; 	@ISA = 'P5AST::listop';
package P5AST::op_gprotoent; 	@ISA = 'P5AST::baseop';
package P5AST::op_gsbyname; 	@ISA = 'P5AST::listop';
package P5AST::op_gsbyport; 	@ISA = 'P5AST::listop';
package P5AST::op_gservent; 	@ISA = 'P5AST::baseop';
package P5AST::op_shostent; 	@ISA = 'P5AST::baseop_unop';
package P5AST::op_snetent; 	@ISA = 'P5AST::baseop_unop';
package P5AST::op_sprotoent; 	@ISA = 'P5AST::baseop_unop';
package P5AST::op_sservent; 	@ISA = 'P5AST::baseop_unop';
package P5AST::op_ehostent; 	@ISA = 'P5AST::baseop';
package P5AST::op_enetent; 	@ISA = 'P5AST::baseop';
package P5AST::op_eprotoent; 	@ISA = 'P5AST::baseop';
package P5AST::op_eservent; 	@ISA = 'P5AST::baseop';
package P5AST::op_gpwnam; 	@ISA = 'P5AST::baseop_unop';
package P5AST::op_gpwuid; 	@ISA = 'P5AST::baseop_unop';
package P5AST::op_gpwent; 	@ISA = 'P5AST::baseop';
package P5AST::op_spwent; 	@ISA = 'P5AST::baseop';
package P5AST::op_epwent; 	@ISA = 'P5AST::baseop';
package P5AST::op_ggrnam; 	@ISA = 'P5AST::baseop_unop';
package P5AST::op_ggrgid; 	@ISA = 'P5AST::baseop_unop';
package P5AST::op_ggrent; 	@ISA = 'P5AST::baseop';
package P5AST::op_sgrent; 	@ISA = 'P5AST::baseop';
package P5AST::op_egrent; 	@ISA = 'P5AST::baseop';
package P5AST::op_getlogin; 	@ISA = 'P5AST::baseop';
package P5AST::op_syscall; 	@ISA = 'P5AST::listop';
package P5AST::op_lock; 	@ISA = 'P5AST::baseop_unop';
package P5AST::op_threadsv; 	@ISA = 'P5AST::baseop';
package P5AST::op_setstate; 	@ISA = 'P5AST::cop';
package P5AST::op_method_named; @ISA = 'P5AST::padop_svop';
package P5AST::op_dor; 		@ISA = 'P5AST::logop';
package P5AST::op_dorassign; 	@ISA = 'P5AST::logop';
package P5AST::op_custom; 	@ISA = 'P5AST::baseop';

# New node types (implicit types within perl)

package P5AST::statement; 	@ISA = 'P5AST::cop';
package P5AST::peg;		@ISA = 'P5AST::baseop';
package P5AST::parens;		@ISA = 'P5AST::baseop';
package P5AST::bindop;		@ISA = 'P5AST::baseop';
package P5AST::nothing;		@ISA = 'P5AST::baseop';
package P5AST::condstate;	@ISA = 'P5AST::logop';
package P5AST::use;		@ISA = 'P5AST::baseop';
package P5AST::ternary;		@ISA = 'P5AST::baseop';
package P5AST::sub;		@ISA = 'P5AST::baseop';
package P5AST::condmod;		@ISA = 'P5AST::logop';
package P5AST::package;		@ISA = 'P5AST::baseop';
package P5AST::format;		@ISA = 'P5AST::baseop';
package P5AST::qwliteral;	@ISA = 'P5AST::baseop';
package P5AST::quote;		@ISA = 'P5AST::baseop';
package P5AST::token;		@ISA = 'P5AST::baseop';
package P5AST::attrlist;	@ISA = 'P5AST::baseop';
package P5AST::listelem;	@ISA = 'P5AST::baseop';
package P5AST::preplus;		@ISA = 'P5AST::baseop';
package P5AST::doblock;		@ISA = 'P5AST::baseop';
package P5AST::cfor;		@ISA = 'P5AST::baseop';
package P5AST::pmop;		@ISA = 'P5AST::baseop';
