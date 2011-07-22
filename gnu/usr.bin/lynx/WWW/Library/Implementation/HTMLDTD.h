/*
 * $LynxId: HTMLDTD.h,v 1.32 2008/07/06 17:38:13 tom Exp $
 *
			      The HTML DTD -- software interface in libwww
			      HTML DTD - SOFTWARE INTERFACE

   SGML purists should excuse the use of the term "DTD" in this file to
   represent DTD-related information which is not exactly a DTD itself.

   The C modular structure doesn't work very well here, as the dtd is
   partly in the .h and partly in the .c which are not very independent.
   Tant pis.

 */
#ifndef HTMLDTD_H
#define HTMLDTD_H

#include <SGML.h>
#include <HTFont.h>

#ifdef __cplusplus
extern "C" {
#endif
/*
 *  Valid name chars for tag parsing.
 */
#define IsNmStart(c) (isalpha(UCH(c)))
#define IsNmChar(c) (isalnum(UCH(c)) || \
		      c == '_' || c=='-' || c == '.' || c==':')
#define ReallyEmptyTagNum(e) ((HTML_dtd.tags[e].contents == SGML_EMPTY) && \
			      !(HTML_dtd.tags[e].flags & Tgf_nreie))
#define ReallyEmptyTag(t) ((t->contents == SGML_EMPTY) && \
			   !(t->flags & Tgf_nreie))

#include <hdr_HTMLDTD.h>

#ifdef USE_PRETTYSRC
/* values of HTML attributes' types */
#define HTMLA_NORMAL 0		/* nothing specific */
#define HTMLA_ANAME  1		/* anchor name - 'id' or a's 'name' */
#define HTMLA_HREF   2		/* href */
#define HTMLA_CLASS  4		/* class name.  */
#define HTMLA_AUXCLASS 8	/* attribute, the value of which also designates
				   a class name */
#endif
    extern const SGML_dtd HTML_dtd;

    extern void HTSwitchDTD(int new_flag);

    extern HTTag HTTag_unrecognized;
    extern HTTag HTTag_mixedObject;

/*

Start anchor element

   It is kinda convenient to have a particular routine for starting an anchor
   element, as everything else for HTML is simple anyway.

  ON ENTRY

   targetstream points to a structured stream object.

   name and href point to attribute strings or are NULL if the attribute is
   to be omitted.

 */
    extern void HTStartAnchor(HTStructured * targetstream, const char *name,
			      const char *href);

    extern void HTStartAnchor5(HTStructured * targetstream, const char *name,
			       const char *href,
			       const char *linktype,
			       int tag_charset);

/*

Start IsIndex element - FM

   It is kinda convenient to have a particular routine for starting an IsIndex
   element with the prompt and/or href (action) attributes specified.

  ON ENTRY

   targetstream points to a structured stream object.

   prompt and href point to attribute strings or are NULL if the attribute is
   to be omitted.

 */
    extern void HTStartIsIndex(HTStructured * targetstream, const char *prompt,
			       const char *href);

#ifdef __cplusplus
}
#endif
#endif				/* HTMLDTD_H */
