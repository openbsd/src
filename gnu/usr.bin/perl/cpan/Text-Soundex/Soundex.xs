/* -*- c -*- */

/* (c) Copyright 1998-2003 by Mark Mielke
 *
 * Freedom to use these sources for whatever you want, as long as credit
 * is given where credit is due, is hereby granted. You may make modifications
 * where you see fit but leave this copyright somewhere visible. As well try
 * to initial any changes you make so that if i like the changes i can
 * incorporate them into any later versions of mine.
 *
 *      - Mark Mielke <mark@mielke.cc>
 */

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#define SOUNDEX_ACCURACY (4)	/* The maximum code length... (should be>=2) */

#if !(PERL_REVISION >= 5 && PERL_VERSION >= 8)
#  define utf8n_to_uvchr utf8_to_uv
#endif

static char sv_soundex_table[0x100];
static void sv_soundex_initialize (void)
{
  memset(&sv_soundex_table[0], '\0', sizeof(sv_soundex_table));
  sv_soundex_table['A'] = '0';
  sv_soundex_table['a'] = '0';
  sv_soundex_table['E'] = '0';
  sv_soundex_table['e'] = '0';
  sv_soundex_table['H'] = '0';
  sv_soundex_table['h'] = '0';
  sv_soundex_table['I'] = '0';
  sv_soundex_table['i'] = '0';
  sv_soundex_table['O'] = '0';
  sv_soundex_table['o'] = '0';
  sv_soundex_table['U'] = '0';
  sv_soundex_table['u'] = '0';
  sv_soundex_table['W'] = '0';
  sv_soundex_table['w'] = '0';
  sv_soundex_table['Y'] = '0';
  sv_soundex_table['y'] = '0';
  sv_soundex_table['B'] = '1';
  sv_soundex_table['b'] = '1';
  sv_soundex_table['F'] = '1';
  sv_soundex_table['f'] = '1';
  sv_soundex_table['P'] = '1';
  sv_soundex_table['p'] = '1';
  sv_soundex_table['V'] = '1';
  sv_soundex_table['v'] = '1';
  sv_soundex_table['C'] = '2';
  sv_soundex_table['c'] = '2';
  sv_soundex_table['G'] = '2';
  sv_soundex_table['g'] = '2';
  sv_soundex_table['J'] = '2';
  sv_soundex_table['j'] = '2';
  sv_soundex_table['K'] = '2';
  sv_soundex_table['k'] = '2';
  sv_soundex_table['Q'] = '2';
  sv_soundex_table['q'] = '2';
  sv_soundex_table['S'] = '2';
  sv_soundex_table['s'] = '2';
  sv_soundex_table['X'] = '2';
  sv_soundex_table['x'] = '2';
  sv_soundex_table['Z'] = '2';
  sv_soundex_table['z'] = '2';
  sv_soundex_table['D'] = '3';
  sv_soundex_table['d'] = '3';
  sv_soundex_table['T'] = '3';
  sv_soundex_table['t'] = '3';
  sv_soundex_table['L'] = '4';
  sv_soundex_table['l'] = '4';
  sv_soundex_table['M'] = '5';
  sv_soundex_table['m'] = '5';
  sv_soundex_table['N'] = '5';
  sv_soundex_table['n'] = '5';
  sv_soundex_table['R'] = '6';
  sv_soundex_table['r'] = '6';
}

static SV *sv_soundex (SV* source)
{
  char *source_p;
  char *source_end;

  {
    STRLEN source_len;
    source_p = SvPV(source, source_len);
    source_end = &source_p[source_len];
  }

  while (source_p != source_end)
    {
      char codepart_last = sv_soundex_table[(unsigned char) *source_p];

      if (codepart_last != '\0')
        {
          SV   *code     = newSV(SOUNDEX_ACCURACY);
          char *code_p   = SvPVX(code);
          char *code_end = &code_p[SOUNDEX_ACCURACY];

          SvCUR_set(code, SOUNDEX_ACCURACY);
          SvPOK_only(code);

          *code_p++ = toupper(*source_p++);

          while (source_p != source_end && code_p != code_end)
            {
              char c = *source_p++;
              char codepart = sv_soundex_table[(unsigned char) c];

              if (codepart != '\0')
                if (codepart != codepart_last && (codepart_last = codepart) != '0')
                  *code_p++ = codepart;
            }

          while (code_p != code_end)
            *code_p++ = '0';

          *code_end = '\0';

          return code;
        }

      source_p++;
    }

  return SvREFCNT_inc(perl_get_sv("Text::Soundex::nocode", FALSE));
}

static SV *sv_soundex_utf8 (SV* source)
{
  U8 *source_p;
  U8 *source_end;

  {
    STRLEN source_len;
    source_p = (U8 *) SvPV(source, source_len);
    source_end = &source_p[source_len];
  }

  while (source_p < source_end)
    {
      STRLEN offset;
      UV c = utf8n_to_uvchr(source_p, source_end-source_p, &offset, 0);
      char codepart_last = (c <= 0xFF) ? sv_soundex_table[c] : '\0';
      source_p = (offset >= 1) ? &source_p[offset] : source_end;

      if (codepart_last != '\0')
        {
          SV   *code     = newSV(SOUNDEX_ACCURACY);
          char *code_p   = SvPVX(code);
          char *code_end = &code_p[SOUNDEX_ACCURACY];

          SvCUR_set(code, SOUNDEX_ACCURACY);
          SvPOK_only(code);

          *code_p++ = toupper(c);

          while (source_p != source_end && code_p != code_end)
            {
              char codepart;
              c = utf8n_to_uvchr(source_p, source_end-source_p, &offset, 0);
              codepart = (c <= 0xFF) ? sv_soundex_table[c] : '\0';
              source_p = (offset >= 1) ? &source_p[offset] : source_end;

              if (codepart != '\0')
                if (codepart != codepart_last && (codepart_last = codepart) != '0')
                  *code_p++ = codepart;
            }

          while (code_p != code_end)
            *code_p++ = '0';

          *code_end = '\0';

          return code;
        }

      source_p++;
    }

  return SvREFCNT_inc(perl_get_sv("Text::Soundex::nocode", FALSE));
}

MODULE = Text::Soundex				PACKAGE = Text::Soundex

PROTOTYPES: DISABLE

void
soundex_xs (...)
INIT:
{
  sv_soundex_initialize();
}
PPCODE:
{
  int i;
  for (i = 0; i < items; i++)
    {
      SV *sv = ST(i);

      if (DO_UTF8(sv))
        sv = sv_soundex_utf8(sv);
      else
        sv = sv_soundex(sv);

      PUSHs(sv_2mortal(sv));
    }
}
