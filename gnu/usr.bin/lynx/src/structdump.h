/*
 *
 * Some macros to dump out formatted struct's via the trace file.  -KED
 *
 */
#ifndef STRUCTDUMP_H
#define STRUCTDUMP_H

/* usage: DUMPSTRUCT_LINK(link_ptr, "message"); */
#define   DUMPSTRUCT_LINK(L,X) \
if ((L)) { \
CTRACE((tfp, "\n" \
            "KED:     link_ptr=%p  sizeof=%d  ["X"]\n" \
            "link       struct {\n"      \
            "           *lname=%p\n"     \
            "            lname=|%s|\n"   \
            "          *target=%p\n"     \
            "           target=|%s|\n"   \
            "        *hightext=%p\n"     \
            "         hightext=|%s|\n"   \
            "       *hightext2=%p\n"     \
            "        hightext2=|%s|\n"   \
            " hightext2_offset=%d\n"     \
            "      inUnderline=%1x\n"    \
            "               lx=%d\n"     \
            "               ly=%d\n"     \
            "             type=%d\n"     \
            "    anchor_number=%d\n"     \
            "  anchor_line_num=%d\n"     \
            "            *form=%p\n"     \
            "}\n", \
            (L), sizeof(*((L))), \
            (L)->lname, (L)->lname, (L)->target, (L)->target, \
            (L)->l_hightext, (L)->l_hightext, \
	    (L)->l_hightext2, (L)->l_hightext2, \
            (L)->l_hightext2_offset, \
	    (L)->inUnderline, (L)->lx, (L)->ly, \
            (L)->type, (L)->anchor_number, (L)->anchor_line_num, (L)->form)); \
}else{ \
CTRACE((tfp, "\n" \
            "KED:     link_ptr=0x00000000  (NULL)     ["X"]\n")); \
} \
CTRACE_FLUSH(tfp);

/* usage: DUMPSTRUCT_ANCHOR(anchor_ptr, "message"); */
#define   DUMPSTRUCT_ANCHOR(A,X) \
if ((A)) { \
CTRACE((tfp, "\n" \
            "KED:   anchor_ptr=%p  sizeof=%d  ["X"]\n" \
            "TextAnchor struct {\n"      \
            "            *next=%p\n"     \
            "           number=%d\n"     \
            "         line_pos=%d\n"     \
            "           extent=%d\n"     \
            "         line_num=%d\n"     \
            "        *hightext=%p\n"     \
            "         hightext=|%s|\n"   \
            "       *hightext2=%p\n"     \
            "        hightext2=|%s|\n"   \
            "  hightext2offset=%d\n"     \
            "        link_type=%d\n"     \
            "     *input_field=%p\n"     \
            "      input_field=|%s|\n"   \
            "      show_anchor=%1x\n"    \
            "      inUnderline=%1x\n"    \
            "   expansion_anch=%1x\n"    \
            "          *anchor=%p\n"     \
            "}\n", \
            (A), sizeof(*((A))), \
            (A)->next, (A)->number, (A)->line_pos, \
            (A)->extent, (A)->line_num, \
            (A)->hightext, (A)->hightext, (A)->hightext2, (A)->hightext2, \
            (A)->hightext2offset, (A)->link_type, \
            (A)->input_field, (A)->input_field->name, (A)->show_anchor, \
            (A)->inUnderline, (A)->expansion_anch, (A)->anchor)); \
}else{ \
CTRACE((tfp, "\n" \
            "KED:   anchor_ptr=0x00000000  (NULL)     ["X"]\n")); \
} \
CTRACE_FLUSH(tfp);

/* usage: DUMPSTRUCT_FORM(forminfo_ptr, "message"); */
#define   DUMPSTRUCT_FORMINFO(F,X) \
if ((F)) { \
CTRACE((tfp, "\n" \
            "KED: forminfo_ptr=%p  sizeof=%d  ["X"]\n" \
            "FormInfo   struct {\n"      \
            "            *name=%p\n"     \
            "             name=|%s|\n"   \
            "           number=%d\n"     \
            "             type=%d\n"     \
            "           *value=%p\n"     \
            "            value=|%s|\n"   \
            "      *orig_value=%p\n"     \
            "       orig_value=|%s|\n"   \
            "             size=%d\n"     \
            "        maxlength=%d\n"     \
            "            group=%d\n"     \
            "        num_value=%d\n"     \
            "           hrange=%d\n"     \
            "           lrange=%d\n"     \
            "     *select_list=%p\n"     \
            "    submit_action=|%s|\n"   \
            "    submit_method=%d\n"     \
            "   submit_enctype=|%s|\n"   \
            "     submit_title=|%s|\n"   \
            "         no_cache=%1x\n"    \
            "  cp_submit_value=|%s|\n"   \
            "orig_submit_value=|%s|\n"   \
            "           size_l=%d\n"     \
            "         disabled=%d\n"     \
            "          name_cs=%d\n"     \
            "         value_cs=%d\n"     \
            "        accept_cs=|%s|\n"   \
            "}\n", \
            (F), sizeof(*((F))), \
            (F)->name, (F)->name, (F)->number, (F)->type, \
            (F)->value, (F)->value, (F)->orig_value, (F)->orig_value, \
            (F)->size, (F)->maxlength, (F)->group, (F)->num_value, \
            (F)->hrange, (F)->lrange, (F)->select_list, (F)->submit_action, \
            (F)->submit_method, (F)->submit_enctype, (F)->submit_title, \
            (F)->no_cache, (F)->cp_submit_value, (F)->orig_submit_value, \
            (F)->size_l, (F)->disabled, (F)->name_cs, (F)->value_cs, \
            (F)->accept_cs)); \
} else { \
CTRACE((tfp, "\n" \
            "KED: forminfo_ptr=0x00000000  (NULL)     ["X"]\n")); \
} \
CTRACE_FLUSH(tfp);

/* usage: DUMPSTRUCT_LINE(htline_ptr, "message"); */
#define   DUMPSTRUCT_LINE(L,X) \
if ((L)) { \
CTRACE((tfp, "\n" \
            "KED: htline_ptr=%p  sizeof=%d  ["X"]\n" \
            "HTLine  struct {\n"      \
            "         *next=%p\n"     \
            "         *prev=%p\n"     \
            "        offset=%d\n"     \
            "          size=%d\n"     \
            "   split_after=%1x\n"    \
            "        bullet=%1x\n"    \
            "expansion_line=%1x\n"    \
            "w/o U_C_S def\n"         \
            "        data[]=%p\n"     \
            "          data=|%s|\n"   \
            "}\n", \
            (L), sizeof(*((L))), \
            (L)->next, (L)->prev, (L)->offset, (L)->size, (L)->split_after, \
            (L)->bullet, (L)->expansion_line, (L)->data, (L)->data)); \
}else{ \
CTRACE((tfp, "\n" \
            "KED: htline_ptr=0x00000000  (NULL)     ["X"]\n")); \
} \
CTRACE_FLUSH(tfp);

#endif /* STRUCTDUMP_H */
