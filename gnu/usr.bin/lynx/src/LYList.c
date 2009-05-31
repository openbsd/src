/*			Lynx Document Reference List Support	      LYList.c
 *			====================================
 *
 *	Author: FM	Foteos Macrides (macrides@sci.wfbr.edu)
 *
 */

#include <HTUtils.h>
#include <HTAlert.h>
#include <LYUtils.h>
#include <GridText.h>
#include <LYList.h>
#include <LYMap.h>
#include <LYClean.h>
#include <LYGlobalDefs.h>
#include <LYCharUtils.h>
#include <LYCharSets.h>
#include <LYStrings.h>
#include <LYHistory.h>

#ifdef DIRED_SUPPORT
#include <LYUpload.h>
#include <LYLocal.h>
#endif /* DIRED_SUPPORT */

#include <LYexit.h>
#include <LYLeaks.h>

/*	showlist - F.Macrides (macrides@sci.wfeb.edu)
 *	--------
 *	Create a temporary text/html file with a list of links to
 *	HyperText References in the current document.
 *
 *  On entry
 *	titles		Set:	if we want titles where available
 *			Clear:	we only get addresses.
 */

int showlist(DocInfo *newdoc, BOOLEAN titles)
{
    int cnt;
    int refs, hidden_links;
    static char tempfile[LY_MAXPATH];
    static BOOLEAN last_titles = TRUE;
    FILE *fp0;
    char *Address = NULL, *Title = NULL, *cp = NULL;
    char *LinkTitle = NULL;	/* Rel stored as property of link, not of dest */
    BOOLEAN intern_w_post = FALSE;
    const char *desc = "unknown field or link";
    void *helper;

    refs = HText_sourceAnchors(HTMainText);
    hidden_links = HText_HiddenLinkCount(HTMainText);
    if (refs <= 0 && hidden_links > 0 &&
	LYHiddenLinks != HIDDENLINKS_SEPARATE) {
	HTUserMsg(NO_VISIBLE_REFS_FROM_DOC);
	return (-1);
    }
    if (refs <= 0 && hidden_links <= 0) {
	HTUserMsg(NO_REFS_FROM_DOC);
	return (-1);
    }

    if ((fp0 = InternalPageFP(tempfile, titles == last_titles)) == 0)
	return (-1);

    LYLocalFileToURL(&(newdoc->address), tempfile);

    LYRegisterUIPage(newdoc->address,
		     titles ? UIP_LIST_PAGE : UIP_ADDRLIST_PAGE);
    last_titles = titles;
    LYforce_HTML_mode = TRUE;	/* force this file to be HTML */
    LYforce_no_cache = TRUE;	/* force this file to be new */

#ifdef EXP_ADDRLIST_PAGE
    if (titles != TRUE)
	BeginInternalPage(fp0, ADDRLIST_PAGE_TITLE, LIST_PAGE_HELP);
    else
#endif
	BeginInternalPage(fp0, LIST_PAGE_TITLE, LIST_PAGE_HELP);

    StrAllocCopy(Address, HTLoadedDocumentURL());
    LYEntify(&Address, FALSE);
    fprintf(fp0, "%s%s<p>\n", gettext("References in "),
	    (non_empty(Address)
	     ? Address
	     : gettext("this document:")));
    FREE(Address);
    if (refs > 0) {
	fprintf(fp0, "<%s compact>\n", ((keypad_mode == NUMBERS_AS_ARROWS) ?
					"ol" : "ul"));
	if (hidden_links > 0)
	    fprintf(fp0, "<lh><em>%s</em>\n", gettext("Visible links:"));
    }
    if (hidden_links > 0) {
	if (LYHiddenLinks == HIDDENLINKS_IGNORE)
	    hidden_links = 0;
    }
    helper = NULL;		/* init */
    for (cnt = 1; cnt <= refs; cnt++) {
	HTChildAnchor *child = HText_childNextNumber(cnt, &helper);
	HTAnchor *dest_intl = NULL;
	HTAnchor *dest;
	HTParentAnchor *parent;
	char *address;
	const char *title;

	if (child == 0) {
	    /*
	     * child should not be 0 unless form field numbering is on and cnt
	     * is the number of a form input field.  HText_FormDescNumber()
	     * will set desc to a description of what type of input field this
	     * is.  We'll list it to ensure that the link numbers on the list
	     * page match the numbering in the original document, but won't
	     * create a forward link to the form.  - FM && LE
	     *
	     * Changed to create a fake hidden link, to get the numbering right
	     * in connection with always treating this file as
	     * HIDDENLINKS_MERGE in GridText.c - kw
	     */
	    if (fields_are_numbered()) {
		HText_FormDescNumber(cnt, &desc);
		fprintf(fp0,
			"<li><a id=%d href=\"#%d\">form field</a> = <em>%s</em>\n",
			cnt, cnt, desc);
	    }
	    continue;
	}
#ifndef DONT_TRACK_INTERNAL_LINKS
	dest_intl = HTAnchor_followTypedLink(child, HTInternalLink);
#endif
	dest = dest_intl ?
	    dest_intl : HTAnchor_followLink(child);
	parent = HTAnchor_parent(dest);
	if (!intern_w_post && dest_intl &&
	    HTMainAnchor &&
	    HTMainAnchor->post_data &&
	    parent->post_data &&
	    BINEQ(HTMainAnchor->post_data, parent->post_data)) {
	    /*
	     * Set flag to note that we had at least one internal link, if the
	     * document from which we are generating the list has associated
	     * POST data; after an extra check that the link destination really
	     * has the same POST data so that we can believe it is an internal
	     * link.
	     */
	    intern_w_post = TRUE;
	}
	address = HTAnchor_address(dest);
	title = titles ? HTAnchor_title(parent) : NULL;
	if (dest_intl) {
	    HTSprintf0(&LinkTitle, "(internal)");
	} else if (titles && child->type &&
		   dest == child->dest &&
		   !strncmp(HTAtom_name(child->type),
			    "RelTitle: ", 10)) {
	    HTSprintf0(&LinkTitle, "(%s)", HTAtom_name(child->type) + 10);
	} else {
	    FREE(LinkTitle);
	}
	StrAllocCopy(Address, address);
	FREE(address);
	LYEntify(&Address, TRUE);
	if (non_empty(title)) {
	    LYformTitle(&Title, title);
	    LYEntify(&Title, TRUE);
	    if (*Title) {
		cp = findPoundSelector(Address);
	    } else {
		FREE(Title);
	    }
	}

	fprintf(fp0, "<li><a href=\"%s\"%s>%s%s%s%s%s</a>\n", Address,
		dest_intl ? " TYPE=\"internal link\"" : "",
		NonNull(LinkTitle),
		((HTAnchor *) parent != dest) && Title ? "in " : "",
		(char *) (Title ? Title : Address),
		(Title && cp) ? " - " : "",
		(Title && cp) ? (cp + 1) : "");

	FREE(Address);
	FREE(Title);
    }
    FREE(LinkTitle);

    if (hidden_links > 0) {
	if (refs > 0)
	    fprintf(fp0, "\n</%s>\n\n<p>\n",
		    ((keypad_mode == NUMBERS_AS_ARROWS) ?
		     "ol" : "ul"));
	fprintf(fp0, "<%s compact>\n", ((keypad_mode == NUMBERS_AS_ARROWS) ?
					"ol continue" : "ul"));
	fprintf(fp0, "<lh><em>%s</em>\n", gettext("Hidden links:"));
    }

    for (cnt = 0; cnt < hidden_links; cnt++) {
	StrAllocCopy(Address, HText_HiddenLinkAt(HTMainText, cnt));
	LYEntify(&Address, FALSE);
	if (isEmpty(Address)) {
	    FREE(Address);
	    continue;
	}
	fprintf(fp0, "<li><a href=\"%s\">%s</a>\n", Address, Address);

	FREE(Address);
    }

    fprintf(fp0, "\n</%s>\n", ((keypad_mode == NUMBERS_AS_ARROWS) ?
			       "ol" : "ul"));
    EndInternalPage(fp0);
    LYCloseTempFP(fp0);

    /*
     * Make necessary changes to newdoc before returning to caller.  If the
     * intern_w_post flag is set, we keep the POST data in newdoc that have
     * been passed in.  They should be the same as in the loaded document for
     * which we generated the list.  In that case the file we have written will
     * be associated with the same POST data when it is loaded after we are
     * done here, so that following one of the links we have marked as
     * "internal link" can lead back to the underlying document with the right
     * address+post_data combination.  - kw
     */
    if (intern_w_post) {
	newdoc->internal_link = TRUE;
    } else {
	LYFreePostData(newdoc);
	newdoc->internal_link = FALSE;
    }
    newdoc->isHEAD = FALSE;
    newdoc->safe = FALSE;
    return (0);
}

static void print_refs(FILE *fp, BOOLEAN titles, int refs)
{
    int cnt;
    char *address = NULL;
    const char *desc = gettext("unknown field or link");
    void *helper = NULL;	/* init */

    for (cnt = 1; cnt <= refs; cnt++) {
	HTChildAnchor *child = HText_childNextNumber(cnt, &helper);
	HTAnchor *dest;
	HTParentAnchor *parent;
	const char *title;

	if (child == 0) {
	    /*
	     * child should not be 0 unless form field numbering is on and
	     * cnt is the number of a form input field. 
	     * HText_FormDescNumber() will set desc to a description of
	     * what type of input field this is.  We'll create a
	     * within-document link to ensure that the link numbers on the
	     * list page match the numbering in the original document, but
	     * won't create a forward link to the form.  - FM && LE
	     */
	    if (fields_are_numbered()) {
		HText_FormDescNumber(cnt, &desc);
		fprintf(fp, "%4d. form field = %s\n", cnt, desc);
	    }
	    continue;
	}
	dest = HTAnchor_followLink(child);
	/*
	 * Ignore if child anchor points to itself, i.e., we had something
	 * like <A NAME=xyz HREF="#xyz"> and it is not treated as a hidden
	 * link.  Useful if someone 'P'rints the List Page (which isn't a
	 * very useful action to do, but anyway...) - kw
	 */
	if (dest == (HTAnchor *) child)
	    continue;
	parent = HTAnchor_parent(dest);
	title = titles ? HTAnchor_title(parent) : NULL;
	address = HTAnchor_address(dest);
	fprintf(fp, "%4d. %s%s\n", cnt,
		((HTAnchor *) parent != dest) && title ? "in " : "",
		(title ? title : address));
	FREE(address);
#ifdef VMS
	if (HadVMSInterrupt)
	    break;
#endif /* VMS */
    }
}

static void print_hidden_refs(FILE *fp, int refs, int hidden_links)
{
    int cnt;
    char *address = NULL;

    fprintf(fp, "%s   %s\n", ((refs > 0) ? "\n" : ""),
	    gettext("Hidden links:"));
    for (cnt = 0; cnt < hidden_links; cnt++) {
	StrAllocCopy(address, HText_HiddenLinkAt(HTMainText, cnt));
	if (isEmpty(address)) {
	    FREE(address);
	    continue;
	}
	fprintf(fp, "%4d. %s\n", ((cnt + 1) + refs), address);
	FREE(address);
#ifdef VMS
	if (HadVMSInterrupt)
	    break;
#endif /* VMS */
    }
}

/*	printlist - F.Macrides (macrides@sci.wfeb.edu)
 *	---------
 *	Print a text/plain list of HyperText References
 *	in the current document.
 *
 *  On entry
 *	titles		Set:	if we want titles where available
 *			Clear:	we only get addresses.
 */
void printlist(FILE *fp, BOOLEAN titles)
{
    int refs, hidden_links;

    refs = HText_sourceAnchors(HTMainText);
    if (refs > 0 || LYHiddenLinks == HIDDENLINKS_SEPARATE) {
	hidden_links = HText_HiddenLinkCount(HTMainText);
	if (refs > 0 || hidden_links > 0) {
	    fprintf(fp, "\n%s\n\n", gettext("References"));
	    if (LYHiddenLinks == HIDDENLINKS_IGNORE)
		hidden_links = 0;
	    if (hidden_links > 0) {
		fprintf(fp, "   %s\n", gettext("Visible links"));
	    }
	    print_refs(fp, titles, refs);

	    if (hidden_links > 0) {
		print_hidden_refs(fp, refs, hidden_links);
	    }
	}
    }
    LYPrintImgMaps(fp);
    return;
}
