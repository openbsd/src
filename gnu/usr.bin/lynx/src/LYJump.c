#include "HTUtils.h"
#include "tcp.h"
#include "HTAlert.h"
#include "LYUtils.h"
#include "LYStrings.h"
#include "LYGlobalDefs.h"
#include "LYJump.h"
#include "LYKeymap.h"
#include "LYSignal.h"
#include "GridText.h"

#include "LYLeaks.h"

#ifdef VMS
#include <fab.h>
#endif /* VMS */

#define FREE(x) if (x) {free(x); x = NULL;}

struct JumpTable *JThead = NULL;

PRIVATE int LYCompare PARAMS ((CONST void *e1, CONST void *e2));
PRIVATE unsigned LYRead_Jumpfile PARAMS ((struct JumpTable *jtp));

PUBLIC void LYJumpTable_free NOARGS
{
    struct JumpTable *cur = JThead;
    struct JumpTable *next;

    while (cur) {
	next = cur->next;
	FREE(cur->msg);
	FREE(cur->file);
	FREE(cur->shortcut);
	if (cur->history) {
	    char *shortcut;
	    HTList *current = cur->history;

	    while (NULL != (shortcut = (char *)HTList_nextObject(current))) {
		FREE(shortcut);
	    }
	    HTList_delete(cur->history);
	    cur->history = NULL;
	}
	FREE(cur->table);
	FREE(cur->mp);
	FREE(cur);
	cur = next;
    }
    JThead = NULL;
    return;
}

/*
 * Utility for listing shortcuts, making any repeated
 * shortcut the most current in the list. - FM
 */
PUBLIC void LYAddJumpShortcut ARGS2(HTList *, historyp, char *,shortcut)
{
    char *new;
    char *old;
    HTList *cur =  historyp;

    if (!historyp || !(shortcut && *shortcut))
	return;

    if ((new = (char *)calloc(1, (strlen(shortcut) + 1))) == NULL)
	outofmem(__FILE__, "HTAddJumpShortcut");
    strcpy(new, shortcut);

    while (NULL != (old = (char *)HTList_nextObject(cur))) {
	if (!strcmp(old, new)) {
	    HTList_removeObject(historyp, old);
	    FREE(old);
	    break;
	}
    }
    HTList_addObject(historyp, new);

    return;
}

PUBLIC BOOL LYJumpInit ARGS1 (char *, config)
{
    struct JumpTable *jtp;
    char *cp;

    /*
     * Create a JumpTable structure.
     */
    jtp = (struct JumpTable *) calloc(1, sizeof(*jtp));
    if (jtp == NULL) {
	perror("Out of memory in LYJumpInit");
	return FALSE;
    }

    /*
     * config is JUMPFILE:path[:optional_key[:optional_prompt]]
     *
     * Skip JUMPFILE.
     */
    cp = strtok(config, ":\n");
    if (!cp) {
	FREE(jtp);
	return FALSE;
    }

    /*
     * Get the path.
     */
    cp = strtok(NULL, ":\n");
    if (!cp) {
	FREE(jtp);
	return FALSE;
    }
    StrAllocCopy(jtp->file, cp);
    if (!JThead)
	atexit(LYJumpTable_free);

    /*
     * Get the key, if present.
     */
    cp = strtok(NULL, ":\n");

    /*
     * If no key, check whether we are resetting the default jumps file.
     */
    if (!cp && JThead) {
	struct JumpTable *jtptmp = JThead;
	jumpfile = jtp->file;
	FREE(jtp);
	while (jtptmp && jtptmp->key)
	    jtptmp = jtptmp->next;
	if (!jtptmp)
	    return FALSE;
	StrAllocCopy(jtptmp->file, jumpfile);
	StrAllocCopy(jtptmp->msg, jumpprompt);
	return TRUE;
    }

    /*
     * If a key is present and we have no default, create one,
     * using the path from config, and the current jumpprompt.
     */
    if (cp && !JThead) {
	JThead = jtp;
	StrAllocCopy(JThead->msg, jumpprompt);
	if (!jumpfile)
	    StrAllocCopy(jumpfile, JThead->file);
	jtp = (struct JumpTable *) calloc(1, sizeof(*jtp));
	if (jtp == NULL) {
	    perror("Out of memory in LYJumpInit");
	    return FALSE;
	}
	StrAllocCopy(jtp->file, JThead->file);
    }

    /*
     * Complete the initialization of config.
     */
    if (cp) {
	jtp->key = remap(cp, "JUMP");	    /* key is present, (re)map it */
	cp = strtok(NULL, "\n");	    /* get prompt, if present */
	if (cp && *cp)
	    StrAllocCopy(jtp->msg, cp);     /* prompt is present, load it */
	else
	    cp = NULL;
    }
    if (!cp)				     /* no prompt, use default */
	StrAllocCopy(jtp->msg, jumpprompt);
    if (jtp->msg[strlen(jtp->msg)-1] != ' ') /* ensure a trailing space */
	StrAllocCat(jtp->msg, " ");
    jtp->history = HTList_new();
    jtp->next = JThead;
    JThead = jtp;
    return TRUE;
}

PUBLIC char *LYJump ARGS1(int, key)
{
    JumpDatum seeking;
    JumpDatum *found;
    static char buf[124];
    char *bp, *cp;
    struct JumpTable *jtp;
    int ch, recall;
    int ShortcutTotal;
    int ShortcutNum;
    BOOLEAN FirstShortcutRecall = TRUE;

    if (!JThead)
	return NULL;
    jtp = JThead;
    while (jtp && jtp->key && jtp->key != key)
	jtp = jtp->next;
    if (!jtp) {
	char msg[40];
	sprintf(msg, KEY_NOT_MAPPED_TO_JUMP_FILE, key);
	HTAlert(msg);
	return NULL;
    }
    if (!jtp->table)
	jtp->nel = LYRead_Jumpfile(jtp);
    if (jtp->nel == 0)
	return NULL;

    if (!jump_buffer || !(jtp->shortcut && *jtp->shortcut))
	*buf = '\0';
    else if (jtp->shortcut && *jtp->shortcut) {
	if (strlen(jtp->shortcut) > 119)
	    jtp->shortcut[119] = '\0';
	strcpy(buf, jtp->shortcut);
    }

    ShortcutTotal = (jtp->history ? HTList_count(jtp->history) : 0);
    if (jump_buffer && *buf) {
	recall = ((ShortcutTotal > 1) ? RECALL : NORECALL);
	ShortcutNum = 0;
	FirstShortcutRecall = FALSE;
    } else {
	recall = ((ShortcutTotal >= 1) ? RECALL : NORECALL);
	ShortcutNum = ShortcutTotal;
	FirstShortcutRecall = TRUE;
    }

    statusline(jtp->msg);
    if ((ch=LYgetstr(buf, VISIBLE, (sizeof(buf) - 4), recall)) < 0) {
	/*
	 * User cancelled the Jump via ^G. - FM
	 */
	_statusline(CANCELLED);
	sleep(InfoSecs);
	return NULL;
    }

check_recall:
    bp = buf;
    if (toupper(key) == 'G' && strncmp(buf, "o ", 2) == 0)
	bp++;
    while (isspace(*bp))
	bp++;
    if (*bp == '\0' &&
	!(recall && (ch == UPARROW || ch == DNARROW))) {
	/*
	 * User cancelled the Jump via a zero-length string. - FM
	 */
	*buf = '\0';
	StrAllocCopy(jtp->shortcut, buf);
	_statusline(CANCELLED);
	sleep(InfoSecs);
	return NULL;
    }
#ifdef PERMIT_GOTO_FROM_JUMP
    if (strchr(bp, ':') || strchr(bp, '/')) {
	char *temp=NULL;

	LYJumpFileURL = FALSE;
	if (no_goto) {
	    *buf = '\0';
	    StrAllocCopy(jtp->shortcut, buf);
	    _statusline(RANDOM_URL_DISALLOWED);
	    sleep(MessageSecs);
	    return NULL;
	}
	StrAllocCopy(temp, "Go ");
	StrAllocCat(temp, bp);
	strcpy(buf, temp);
	FREE(temp);
	return(bp=buf);
    }
#endif /* PERMIT_GOTO_FROM_JUMP */

    if (recall && ch == UPARROW) {
	if (FirstShortcutRecall) {
	    /*
	     * Use last Shortcut in the list. - FM
	     */
	    FirstShortcutRecall = FALSE;
	    ShortcutNum = 0;
	} else {
	    /*
	     * Go back to the previous Shortcut in the list. - FM
	     */
	    ShortcutNum++;
	}
	if (ShortcutNum >= ShortcutTotal)
	    /*
	     * Roll around to the last Shortcut in the list. - FM
	     */
	    ShortcutNum = 0;
	if ((cp=(char *)HTList_objectAt(jtp->history,
					ShortcutNum)) != NULL) {
	    strcpy(buf, cp);
	    if (jump_buffer && jtp->shortcut &&
		!strcmp(buf, jtp->shortcut)) {
		_statusline(EDIT_CURRENT_SHORTCUT);
	    } else if ((jump_buffer && ShortcutTotal == 2) ||
		       (!jump_buffer && ShortcutTotal == 1)) {
		_statusline(EDIT_THE_PREV_SHORTCUT);
	    } else {
		_statusline(EDIT_A_PREV_SHORTCUT);
	    }
	    if ((ch=LYgetstr(buf, VISIBLE,
			     sizeof(buf), recall)) < 0) {
		/*
		 * User cancelled the jump via ^G.
		 */
		_statusline(CANCELLED);
		sleep(InfoSecs);
		return NULL;
	    }
	    goto check_recall;
	}
    } else if (recall && ch == DNARROW) {
	if (FirstShortcutRecall) {
	    /*
	     * Use the first Shortcut in the list. - FM
	     */
	    FirstShortcutRecall = FALSE;
	    ShortcutNum = ShortcutTotal - 1;
	} else {
	    /*
	     * Advance to the next Shortcut in the list. - FM
	     */
	    ShortcutNum--;
	}
	if (ShortcutNum < 0)
	    /*
	     * Roll around to the first Shortcut in the list. - FM
	     */
	    ShortcutNum = ShortcutTotal - 1;
	if ((cp=(char *)HTList_objectAt(jtp->history,
					ShortcutNum)) != NULL) {
	    strcpy(buf, cp);
	    if (jump_buffer && jtp->shortcut &&
		!strcmp(buf, jtp->shortcut)) {
		_statusline(EDIT_CURRENT_SHORTCUT);
	    } else if ((jump_buffer && ShortcutTotal == 2) ||
		       (!jump_buffer && ShortcutTotal == 1)) {
		_statusline(EDIT_THE_PREV_SHORTCUT);
	    } else {
		_statusline(EDIT_THE_PREV_SHORTCUT);
	    }
	    if ((ch=LYgetstr(buf, VISIBLE,
			     sizeof(buf), recall)) < 0) {
		/*
		 * User cancelled the jump via ^G.
		 */
		_statusline(CANCELLED);
		sleep(InfoSecs);
		return NULL;
	    }
	    goto check_recall;
	}
    }

    seeking.key = bp;
    found = (JumpDatum *)bsearch((char *)&seeking, (char *)jtp->table,
				 jtp->nel, sizeof(JumpDatum), LYCompare);
    if (!found) {
	user_message("Unknown target '%s'", buf);
	sleep(AlertSecs);
    }

    StrAllocCopy(jtp->shortcut, bp);
    LYAddJumpShortcut(jtp->history, jtp->shortcut);
    return found ? found->url : NULL;
}

PRIVATE unsigned LYRead_Jumpfile ARGS1(struct JumpTable *,jtp)
{
    struct stat st;
    unsigned int nel;
    char *mp;
    int fd;
#ifdef VMS
    FILE *fp;
    BOOL IsStream_LF = TRUE;
#endif /* VMS */
    char *cp;
    unsigned i;

    if (jtp->file == NULL || *(jtp->file) == '\0')
	return 0;
    if (stat(jtp->file, &st) < 0) {
	HTAlert(CANNOT_LOCATE_JUMP_FILE);
	return 0;
    }

    /* allocate storage to read entire file */
    if ((mp=(char *)calloc(1, st.st_size + 1)) == NULL) {
	HTAlert(OUTOF_MEM_FOR_JUMP_FILE);
	return 0;
    }

#ifdef VMS
    if (st.st_fab_rfm != (char)FAB$C_STMLF) {
	/** It's a record-oriented file. **/
	IsStream_LF = FALSE;
	if ((fp = fopen(jtp->file, "r", "mbc=32")) == NULL) {
	    HTAlert(CANNOT_OPEN_JUMP_FILE);
	    FREE(mp);
	    return 0;
	}
    } else
    if ((fd=open(jtp->file, O_RDONLY, "mbc=32")) < 0) {
#else
    if ((fd=open(jtp->file, O_RDONLY)) < 0) {
#endif /* VMS */
	HTAlert(CANNOT_OPEN_JUMP_FILE);
	FREE(mp);
	return 0;
    }

#ifdef VMS
    if (IsStream_LF) {
    /** Handle as a stream. **/
#endif /* VMS */
    if (read(fd, mp, st.st_size) < st.st_size) {
	HTAlert(ERROR_READING_JUMP_FILE);
	FREE(mp);
	return 0;
    }
    mp[st.st_size] = '\0';
    close(fd);
#ifdef VMS
    } else {
    /** Handle as a series of records. **/
    if(fgets(mp, 1024, fp) == NULL) {
	HTAlert(ERROR_READING_JUMP_FILE);
	FREE(mp);
	return 0;
    } else
	while(fgets(mp+strlen(mp), 1024, fp) != NULL) ;
    fclose(fp);
    }
#endif /* VMS */

    /* quick scan for approximate number of entries */
    nel = 0;
    cp = mp;
    while((cp = strchr(cp, '\n')) != NULL) {
	nel++;
	cp++;
    }

    jtp->table = (JumpDatum *)malloc(nel * sizeof(JumpDatum));
    if (jtp->table == NULL) {
	HTAlert(OUTOF_MEM_FOR_JUMP_TABLE);
	FREE(mp);
	return 0;
    }

    cp = jtp->mp = mp;
    for (i = 0; i < nel; ) {
	if (strncmp(cp, "<!--", 4) == 0 || strncmp(cp, "<dl>", 4) == 0) {
	    cp = strchr(cp, '\n');
	    if (cp == NULL)
		break;
	    cp++;
	    continue;
	}
	cp = LYstrstr(cp, "<dt>");
	if (cp == NULL)
	    break;
	cp += 4;
	jtp->table[i].key = cp;
	cp = LYstrstr(cp, "<dd>");
	if (cp == NULL)
	    break;
	*cp = '\0';
	cp += 4;
	cp = LYstrstr(cp, "href=\"");
	if (cp == NULL)
	    break;
	cp += 6;
	jtp->table[i].url = cp;
	cp = strchr(cp, '"');
	if (cp == NULL)
	    break;
	*cp = '\0';
	cp++;
	cp = strchr(cp, '\n');
	if (cp == NULL)
	    break;
	cp++;
	i++;
	if (!cp)
	    break;
    }

    return i;
}

PRIVATE int LYCompare ARGS2 (CONST void *, e1, CONST void *, e2)
{
    return strcasecomp(((CONST JumpDatum *)e1)->key, ((CONST JumpDatum *)e2)->key);
}
