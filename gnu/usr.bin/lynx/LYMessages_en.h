/*
 * Lynx - Hypertext navigation system
 *
 *   (c) Copyright 1992, 1993, 1994 University of Kansas
 *	 1995-1999: GNU General Public License
 */
#ifndef LYMESSAGES_EN_H
#define LYMESSAGES_EN_H

/*******************************************************************
 * The following definitions are for status line prompts, messages, or
 * warnings issued by Lynx during program execution.  You can modify
 * them to make them more appropriate for your site.  We recommend that
 * you extend these definitions to other languages using the gettext
 * library.  There are also scattered uses of 'gettext()' throughout the
 * Lynx source, covering all but those messages which (a) are used for
 * debugging (CTRACE) or (b) are constants used in interaction with
 * other programs.
 *
 * Links to collections of alternate definitions, developed by the Lynx
 * User Community, are maintained in Lynx links:
 *
 *    http://www.subir.com/lynx.html
 *
 * See ABOUT-NLS and po/readme for details and location of contributed
 * translations.  When no translation is available, the English default is
 * used.
 */
#define ALERT_FORMAT gettext("Alert!: %s")
#define WELCOME_MSG gettext("Welcome")
#define REALLY_QUIT gettext("Are you sure you want to quit?")
#ifdef VMS
#define REALLY_EXIT gettext("Really exit from Lynx?")
#endif /* VMS */
#define CONNECTION_INTERRUPTED gettext("Connection interrupted.")
#define TRANSFER_INTERRUPTED gettext("Data transfer interrupted.")
#define CANCELLED gettext("Cancelled!!!")
#define CANCELLING gettext("Cancelling!")
#define NO_CANCEL gettext("Excellent!!!")
#define OPERATION_OK gettext("OK")
#define OPERATION_DONE gettext("Done!")
#define BAD_REQUEST gettext("Bad request!")
#define PREVIOUS gettext("previous")
#define NEXT_SCREEN gettext("next screen")
#define TO_HELP gettext("HELP!")
#define HELP_ON_SEGMENT gettext(", help on ")
#define HELP \
 gettext("Commands: Use arrow keys to move, '?' for help, 'q' to quit, '<-' to go back.")
#define MOREHELP \
 gettext("-- press space for more, use arrow keys to move, '?' for help, 'q' to quit.")
#define MORE gettext("-- press space for next page --")
#define URL_TOO_LONG gettext("URL too long")

/* Forms messages */
#ifdef TEXTFIELDS_MAY_NEED_ACTIVATION
/* Inactive input fields, messages used with -tna option - kw */
#define FORM_LINK_TEXT_MESSAGE_INA \
 gettext("(Text entry field) Inactive.  Press <return> to activate.")
#define FORM_LINK_TEXTAREA_MESSAGE_INA \
 gettext("(Textarea) Inactive.  Press <return> to activate.")
#define FORM_LINK_TEXTAREA_MESSAGE_INA_E \
 gettext("(Textarea) Inactive.  Press <return> to activate (%s for editor).")
#define FORM_LINK_TEXT_SUBMIT_MESSAGE_INA \
 gettext("(Form field) Inactive.  Use <return> to edit.")
#define FORM_TEXT_SUBMIT_MESSAGE_INA_X \
 gettext("(Form field) Inactive.  Use <return> to edit (%s to submit with no cache).")
#define FORM_TEXT_RESUBMIT_MESSAGE_INA \
 gettext("(Form field) Inactive. Press <return> to edit, press <return> twice to submit.")
#define FORM_TEXT_SUBMIT_MAILTO_MSG_INA \
 gettext("(mailto form field) Inactive.  Press <return> to change.")
#define FORM_LINK_PASSWORD_MESSAGE_INA \
 gettext("(Password entry field) Inactive.  Press <return> to activate.")
#endif
#define FORM_LINK_FILE_UNM_MSG \
 gettext("UNMODIFIABLE file entry field.  Use UP or DOWN arrows or tab to move off.")
#define FORM_LINK_FILE_MESSAGE \
 gettext("(File entry field) Enter filename.  Use UP or DOWN arrows or tab to move off.")
#define FORM_LINK_TEXT_MESSAGE \
 gettext("(Text entry field) Enter text.  Use UP or DOWN arrows or tab to move off.")
#define FORM_LINK_TEXTAREA_MESSAGE \
 gettext("(Textarea) Enter text. Use UP/DOWN arrows or TAB to move off.")
#define FORM_LINK_TEXTAREA_MESSAGE_E \
 gettext("(Textarea) Enter text. Use UP/DOWN arrows or TAB to move off (%s for editor).")
#define FORM_LINK_TEXT_UNM_MSG \
 gettext("UNMODIFIABLE form text field.  Use UP or DOWN arrows or tab to move off.")
#define FORM_LINK_TEXT_SUBMIT_MESSAGE \
 gettext("(Form field) Enter text.  Use <return> to submit.")
#define FORM_LINK_TEXT_SUBMIT_MESSAGE_X \
 gettext("(Form field) Enter text.  Use <return> to submit (%s for no cache).")
#define FORM_LINK_TEXT_RESUBMIT_MESSAGE \
 gettext("(Form field) Enter text.  Use <return> to submit, arrows or tab to move off.")
#define FORM_LINK_TEXT_SUBMIT_UNM_MSG \
 gettext("UNMODIFIABLE form field.  Use UP or DOWN arrows or tab to move off.")
#define FORM_LINK_TEXT_SUBMIT_MAILTO_MSG \
 gettext("(mailto form field) Enter text.  Use <return> to submit, arrows to move off.")
#define FORM_LINK_TEXT_SUBMIT_MAILTO_DIS_MSG \
 gettext("(mailto form field) Mail is disallowed so you cannot submit.")
#define FORM_LINK_PASSWORD_MESSAGE \
 gettext("(Password entry field) Enter text.  Use UP or DOWN arrows or tab to move off.")
#define FORM_LINK_PASSWORD_UNM_MSG \
 gettext("UNMODIFIABLE form password.  Use UP or DOWN arrows or tab to move off.")
#define FORM_LINK_CHECKBOX_MESSAGE \
 gettext("(Checkbox Field)   Use right-arrow or <return> to toggle.")
#define FORM_LINK_CHECKBOX_UNM_MSG \
 gettext("UNMODIFIABLE form checkbox.  Use UP or DOWN arrows or tab to move off.")
#define FORM_LINK_RADIO_MESSAGE \
 gettext("(Radio Button)   Use right-arrow or <return> to toggle.")
#define FORM_LINK_RADIO_UNM_MSG \
 gettext("UNMODIFIABLE form radio button.  Use UP or DOWN arrows or tab to move off.")
#define FORM_LINK_SUBMIT_PREFIX \
 gettext("Submit ('x' for no cache) to ")
#define FORM_LINK_RESUBMIT_PREFIX \
 gettext("Submit to ")
#define FORM_LINK_SUBMIT_MESSAGE \
 gettext("(Form submit button) Use right-arrow or <return> to submit ('x' for no cache).")
#define FORM_LINK_RESUBMIT_MESSAGE \
 gettext("(Form submit button) Use right-arrow or <return> to submit.")
#define FORM_LINK_SUBMIT_DIS_MSG \
 gettext("DISABLED form submit button.  Use UP or DOWN arrows or tab to move off.")
#define FORM_LINK_SUBMIT_MAILTO_PREFIX \
 gettext("Submit mailto form to ")
#define FORM_LINK_SUBMIT_MAILTO_MSG \
 gettext("(mailto form submit button) Use right-arrow or <return> to submit.")
#define FORM_LINK_SUBMIT_MAILTO_DIS_MSG \
 gettext("(mailto form submit button) Mail is disallowed so you cannot submit.")
#define FORM_LINK_RESET_MESSAGE \
 gettext("(Form reset button)   Use right-arrow or <return> to reset form to defaults.")
#define FORM_LINK_RESET_DIS_MSG \
 gettext("DISABLED form reset button.  Use UP or DOWN arrows or tab to move off.")
#define FORM_LINK_OPTION_LIST_MESSAGE \
 gettext("(Option list) Hit return and use arrow keys and return to select option.")
#define CHOICE_LIST_MESSAGE \
 gettext("(Choice list) Hit return and use arrow keys and return to select option.")
#define FORM_LINK_OPTION_LIST_UNM_MSG \
 gettext("UNMODIFIABLE option list.  Use return or arrow keys to review or leave.")
#define CHOICE_LIST_UNM_MSG \
 gettext("UNMODIFIABLE choice list.  Use return or arrow keys to review or leave.")
#define SUBMITTING_FORM gettext("Submitting form...")
#define RESETTING_FORM gettext("Resetting form...")
#define RELOADING_FORM \
 gettext("Reloading document.  Any form entries will be lost!")
#define CANNOT_TRANSCODE_FORM gettext("Warning: Cannot transcode form data to charset %s!")

#define NORMAL_LINK_MESSAGE \
 gettext("(NORMAL LINK)   Use right-arrow or <return> to activate.")
#define LINK_NOT_FOUND gettext("The resource requested is not available at this time.")
#define ENTER_LYNX_COMMAND gettext("Enter Lynx keystroke command: ")
#define WWW_FIND_MESSAGE gettext("Looking up ")
#define WWW_WAIT_MESSAGE gettext("Getting %s")
#define WWW_SKIP_MESSAGE gettext("Skipping %s")
#define WWW_USING_MESSAGE gettext("Using %s")
#define WWW_ILLEGAL_URL_MESSAGE gettext("Illegal URL: %s")
#define WWW_BAD_ADDR_MESSAGE gettext("Badly formed address %s")
#define ADVANCED_URL_MESSAGE gettext("URL: %s")
#define WWW_FAIL_MESSAGE gettext("Unable to access WWW file!!!")
#define WWW_INDEX_MESSAGE gettext("This is a searchable index.  Use %s to search.")
#define WWW_INDEX_MORE_MESSAGE \
 gettext("--More--  This is a searchable index.  Use %s to search.")
#define BAD_LINK_NUM_ENTERED gettext("You have entered an invalid link number.")
#define SOURCE_HELP \
 gettext("Currently viewing document source.  Press '\\' to return to rendered version.")
#define NOVICE_LINE_ONE \
 gettext("  Arrow keys: Up and Down to move.  Right to follow a link; Left to go back.  \n")
#define NOVICE_LINE_TWO \
 gettext(" H)elp O)ptions P)rint G)o M)ain screen Q)uit /=search [delete]=history list \n")
#define NOVICE_LINE_TWO_A \
 gettext("  O)ther cmds  H)elp  K)eymap  G)oto  P)rint  M)ain screen  o)ptions  Q)uit  \n")
#define NOVICE_LINE_TWO_B \
 gettext("  O)ther cmds  B)ack  E)dit  D)ownload ^R)eload ^W)ipe screen  search doc: / \n")
#define NOVICE_LINE_TWO_C \
 gettext("O)ther cmds  C)omment  History: <backspace>  Bookmarks: V)iew, A)dd, R)emove \n")
#define FORM_NOVICELINE_ONE \
 gettext("            Enter text into the field by typing on the keyboard              ")
#define FORM_NOVICELINE_TWO \
 gettext("    Ctrl-U to delete all text in field, [Backspace] to delete a character    ")
#define FORM_NOVICELINE_TWO_DELBL \
 gettext("      Ctrl-U to delete text in field, [Backspace] to delete a character    ")
#define FORM_NOVICELINE_TWO_VAR \
 gettext("    %s to delete all text in field, [Backspace] to delete a character    ")
#define FORM_NOVICELINE_TWO_DELBL_VAR \
 gettext("      %s to delete text in field, [Backspace] to delete a character    ")

/* mailto */
#define BAD_FORM_MAILTO gettext("Malformed mailto form submission!  Cancelled!")
#define MAILTO_SQUASH_CTL gettext("Warning!  Control codes in mail address replaced by ?")
#define FORM_MAILTO_DISALLOWED gettext("Mail disallowed!  Cannot submit.")
#define FORM_MAILTO_FAILED gettext("Mailto form submission failed!")
#define FORM_MAILTO_CANCELLED gettext("Mailto form submission Cancelled!!!")
#define SENDING_FORM_CONTENT gettext("Sending form content...")
#define NO_ADDRESS_IN_MAILTO_URL gettext("No email address is present in mailto URL!")
#define MAILTO_URL_TEMPOPEN_FAILED \
 gettext("Unable to open temporary file for mailto URL!")
#define INC_ORIG_MSG_PROMPT \
 gettext("Do you wish to include the original message?")
#define INC_PREPARSED_MSG_PROMPT \
 gettext("Do you wish to include the preparsed source?")
#define SPAWNING_EDITOR_FOR_MAIL \
 gettext("Spawning your selected editor to edit mail message")
#define ERROR_SPAWNING_EDITOR \
 gettext("Error spawning editor, check your editor definition in the options menu")
#define SEND_COMMENT_PROMPT gettext("Send this comment?")
#define SEND_MESSAGE_PROMPT gettext("Send this message?")
#define SENDING_YOUR_MSG gettext("Sending your message...")
#define SENDING_COMMENT gettext("Sending your comment:")

/* textarea */
#define NOT_IN_TEXTAREA_NOEDIT gettext("Not in a TEXTAREA; cannot use external editor.")
#define NOT_IN_TEXTAREA gettext("Not in a TEXTAREA; cannot use command.")


#define FILE_ACTIONS_DISALLOWED gettext("file: ACTIONs are disallowed!")
#define FILE_SERVED_LINKS_DISALLOWED \
 gettext("file: URLs via served links are disallowed!")
#define NOAUTH_TO_ACCESS_FILES gettext("Access to local files denied.")
#define FILE_BOOKMARKS_DISALLOWED gettext("file: URLs via bookmarks are disallowed!")
#define SPECIAL_VIA_EXTERNAL_DISALLOWED \
 gettext("This special URL is not allowed in external documents!")
#define RETURN_TO_LYNX gettext("Press <return> to return to Lynx.")
#ifdef VMS
#define SPAWNING_MSG \
 gettext("Spawning DCL subprocess.  Use 'logout' to return to Lynx.\n")
#else
#ifdef DOSPATH
#define SPAWNING_MSG \
 gettext("Type EXIT to return to Lynx.\n")
#else /* UNIX */
#define SPAWNING_MSG \
 gettext("Spawning your default shell.  Use 'exit' to return to Lynx.\n")
#endif
#endif /* VMS */
#define SPAWNING_DISABLED gettext("Spawning is currently disabled.")
#define DOWNLOAD_DISABLED gettext("The 'd'ownload command is currently disabled.")
#define NO_DOWNLOAD_INPUT gettext("You cannot download an input field.")
#define NO_DOWNLOAD_MAILTO_ACTION gettext("Form has a mailto action!  Cannot download.")
#define NO_DOWNLOAD_MAILTO_LINK gettext("You cannot download a mailto: link.")
#define NO_DOWNLOAD_COOKIES gettext("You cannot download cookies.")
#define NO_DOWNLOAD_PRINT_OP gettext("You cannot download a printing option.")
#define NO_DOWNLOAD_UPLOAD_OP gettext("You cannot download an upload option.")
#define NO_DOWNLOAD_PERMIT_OP gettext("You cannot download an permit option.")
#define NO_DOWNLOAD_SPECIAL gettext("This special URL cannot be downloaded!")
#define NO_DOWNLOAD_CHOICE gettext("Nothing to download.")
#define TRACE_ON gettext("Trace ON!")
#define TRACE_OFF gettext("Trace OFF!")
#define CLICKABLE_IMAGES_ON \
 gettext("Links will be included for all images!  Reloading...")
#define CLICKABLE_IMAGES_OFF \
 gettext("Standard image handling restored!  Reloading...")
#define PSEUDO_INLINE_ALTS_ON \
 gettext("Pseudo_ALTs will be inserted for inlines without ALT strings!  Reloading...")
#define PSEUDO_INLINE_ALTS_OFF \
 gettext("Inlines without an ALT string specified will be ignored!  Reloading...")
#define RAWMODE_OFF gettext("Raw 8-bit or CJK mode toggled OFF!  Reloading...")
#define RAWMODE_ON gettext("Raw 8-bit or CJK mode toggled ON!  Reloading...")
#define HEAD_D_L_OR_CANCEL \
 gettext("Send HEAD request for D)ocument or L)ink, or C)ancel? (d,l,c): ")
#define HEAD_D_OR_CANCEL \
 gettext("Send HEAD request for D)ocument, or C)ancel? (d,c): ")
#define DOC_NOT_HTTP_URL gettext("Sorry, the document is not an http URL.")
#define LINK_NOT_HTTP_URL gettext("Sorry, the link is not an http URL.")
#define FORM_ACTION_DISABLED gettext("Sorry, the ACTION for this form is disabled.")
#define FORM_ACTION_NOT_HTTP_URL \
 gettext("Sorry, the ACTION for this form is not an http URL.")
#define NOT_HTTP_URL_OR_ACTION gettext("Not an http URL or form ACTION!")
#define SPECIAL_ACTION_DISALLOWED gettext("This special URL cannot be a form ACTION!")
#define NOT_IN_STARTING_REALM gettext("URL is not in starting realm!")
#define NEWSPOSTING_DISABLED gettext("News posting is disabled!")
#define DIRED_DISABLED gettext("File management support is disabled!")
#define NO_JUMPFILE gettext("No jump file is currently available.")
#define JUMP_PROMPT gettext("Jump to (use '?' for list): ")
#define JUMP_DISALLOWED gettext("Jumping to a shortcut URL is disallowed!")
#define RANDOM_URL_DISALLOWED gettext("Random URL is disallowed!  Use a shortcut.")
#define NO_RANDOM_URLS_YET gettext("No random URLs have been used thus far.")
#define BOOKMARKS_DISABLED gettext("Bookmark features are currently disabled.")
#define BOOKMARK_EXEC_DISABLED gettext("Execution via bookmarks is disabled.")
#define BOOKMARK_FILE_NOT_DEFINED \
 gettext("Bookmark file is not defined. Use %s to see options.")
#define NO_TEMP_FOR_HOTLIST \
 gettext("Unable to open tempfile for X Mosaic hotlist conversion.")
#define BOOKMARK_OPEN_FAILED gettext("ERROR - unable to open bookmark file.")
#define BOOKMARK_OPEN_FAILED_FOR_DEL \
 gettext("Unable to open bookmark file for deletion of link.")
#define BOOKSCRA_OPEN_FAILED_FOR_DEL \
 gettext("Unable to open scratch file for deletion of link.")
#ifdef VMS
#define ERROR_RENAMING_SCRA gettext("Error renaming scratch file.")
#else
#define ERROR_RENAMING_TEMP gettext("Error renaming temporary file.")
#define BOOKTEMP_COPY_FAIL \
 gettext("Unable to copy temporary file for deletion of link.")
#define BOOKTEMP_REOPEN_FAIL_FOR_DEL \
 gettext("Unable to reopen temporary file for deletion of link.")
#endif /* VMS */
#define BOOKMARK_LINK_NOT_ONE_LINE \
 gettext("Link is not by itself all on one line in bookmark file.")
#define BOOKMARK_DEL_FAILED gettext("Bookmark deletion failed.")
#define BOOKMARKS_NOT_TRAVERSED \
 gettext("Bookmark files cannot be traversed (only http URLs).")
#define BOOKMARKS_NOT_OPEN \
 gettext("Unable to open bookmark file, use 'a' to save a link first")
#define BOOKMARKS_NOLINKS gettext("There are no links in this bookmark file!")
#define BOOK_D_L_OR_CANCEL \
 gettext("Save D)ocument or L)ink to bookmark file or C)ancel? (d,l,c): ")
#define BOOK_D_OR_CANCEL gettext("Save D)ocument to bookmark file or C)ancel? (d,c): ")
#define BOOK_L_OR_CANCEL gettext("Save L)ink to bookmark file or C)ancel? (l,c): ")
#define NOBOOK_POST_FORM \
 gettext("Documents from forms with POST content cannot be saved as bookmarks.")
#define NOBOOK_FORM_FIELD gettext("Cannot save form fields/links")
#define NOBOOK_HSML \
 gettext("History, showinfo, menu and list files cannot be saved as bookmarks.")
#define CONFIRM_BOOKMARK_DELETE \
 gettext("Do you really want to delete this link from your bookmark file?")
#define MALFORMED_ADDRESS gettext("Malformed address.")
#define HISTORICAL_ON_MINIMAL_OFF \
 gettext("Historical comment parsing ON (Minimal is overridden)!")
#define HISTORICAL_OFF_MINIMAL_ON \
 gettext("Historical comment parsing OFF (Minimal is in effect)!")
#define HISTORICAL_ON_VALID_OFF \
 gettext("Historical comment parsing ON (Valid is overridden)!")
#define HISTORICAL_OFF_VALID_ON \
 gettext("Historical comment parsing OFF (Valid is in effect)!")
#define MINIMAL_ON_IN_EFFECT \
 gettext("Minimal comment parsing ON (and in effect)!")
#define MINIMAL_OFF_VALID_ON \
 gettext("Minimal comment parsing OFF (Valid is in effect)!")
#define MINIMAL_ON_BUT_HISTORICAL \
 gettext("Minimal comment parsing ON (but Historical is in effect)!")
#define MINIMAL_OFF_HISTORICAL_ON \
 gettext("Minimal comment parsing OFF (Historical is in effect)!")
#define SOFT_DOUBLE_QUOTE_ON gettext("Soft double-quote parsing ON!")
#define SOFT_DOUBLE_QUOTE_OFF gettext("Soft double-quote parsing OFF!")
#define USING_DTD_0 gettext("Now using TagSoup parsing of HTML.")
#define USING_DTD_1 gettext("Now using SortaSGML parsing of HTML!")
#define ALREADY_AT_END gettext("You are already at the end of this document.")
#define ALREADY_AT_BEGIN gettext("You are already at the beginning of this document.")
#define ALREADY_AT_PAGE gettext("You are already at page %d of this document.")
#define LINK_ALREADY_CURRENT gettext("Link number %d already is current.")
#define ALREADY_AT_FIRST gettext("You are already at the first document")
#define NO_LINKS_ABOVE gettext("There are no links above this line of the document.")
#define NO_LINKS_BELOW gettext("There are no links below this line of the document.")
#define MAXLEN_REACHED_DEL_OR_MOV \
 gettext("Maximum length reached!  Delete text or move off field.")
#define NOT_ON_SUBMIT_OR_LINK \
 gettext("You are not on a form submission button or normal link.")
#define NEED_CHECKED_RADIO_BUTTON \
 gettext("One radio button must be checked at all times!")
#define NO_SUBMIT_BUTTON_QUERY gettext("No submit button for this form, submit single text field?")
#define PREV_DOC_QUERY gettext("Do you want to go back to the previous document?")
#define ARROWS_OR_TAB_TO_MOVE gettext("Use arrows or tab to move off of field.")
#define ENTER_TEXT_ARROWS_OR_TAB \
 gettext("Enter text.  Use arrows or tab to move off of field.")
#define NO_FORM_ACTION gettext("** Bad HTML!!  No form action defined. **")
#define BAD_HTML_NO_POPUP gettext("Bad HTML!!  Unable to create popup window!")
#define POPUP_FAILED gettext("Unable to create popup window!")
#define GOTO_DISALLOWED gettext("Goto a random URL is disallowed!")
#define GOTO_NON_HTTP_DISALLOWED gettext("Goto a non-http URL is disallowed!")
#define GOTO_XXXX_DISALLOWED gettext("You are not allowed to goto \"%s\" URLs")
#define URL_TO_OPEN gettext("URL to open: ")
#define EDIT_CURRENT_GOTO gettext("Edit the current Goto URL: ")
#define EDIT_THE_PREV_GOTO gettext("Edit the previous Goto URL: ")
#define EDIT_A_PREV_GOTO gettext("Edit a previous Goto URL: ")
#define CURRENT_DOC_HAS_POST_DATA gettext("Current document has POST data.")
#define EDIT_CURDOC_URL gettext("Edit this document's URL: ")
#define EDIT_CURLINK_URL gettext("Edit the current link's URL: ")
#define EDIT_FM_MENU_URLS_DISALLOWED gettext("You cannot edit File Management URLs")
#define ENTER_DATABASE_QUERY gettext("Enter a database query: ")
#define ENTER_WHEREIS_QUERY gettext("Enter a whereis query: ")
#define EDIT_CURRENT_QUERY gettext("Edit the current query: ")
#define EDIT_THE_PREV_QUERY gettext("Edit the previous query: ")
#define EDIT_A_PREV_QUERY gettext("Edit a previous query: ")
#define USE_C_R_TO_RESUB_CUR_QUERY \
 gettext("Use Control-R to resubmit the current query.")
#define EDIT_CURRENT_SHORTCUT gettext("Edit the current shortcut: ")
#define EDIT_THE_PREV_SHORTCUT gettext("Edit the previous shortcut: ")
#define EDIT_A_PREV_SHORTCUT gettext("Edit a previous shortcut: ")
#define KEY_NOT_MAPPED_TO_JUMP_FILE gettext("Key '%c' is not mapped to a jump file!")
#define CANNOT_LOCATE_JUMP_FILE gettext("Cannot locate jump file!")
#define CANNOT_OPEN_JUMP_FILE gettext("Cannot open jump file!")
#define ERROR_READING_JUMP_FILE gettext("Error reading jump file!")
#define OUTOF_MEM_FOR_JUMP_FILE gettext("Out of memory reading jump file!")
#define OUTOF_MEM_FOR_JUMP_TABLE gettext("Out of memory reading jump table!")
#define NO_INDEX_FILE gettext("No index is currently available.")
#define CONFIRM_MAIN_SCREEN \
 gettext("Do you really want to go to the Main screen?")
#define IN_MAIN_SCREEN gettext("You are already at main screen!")
#define NOT_ISINDEX \
 gettext("Not a searchable indexed document -- press '/' to search for a text string")
#define NO_OWNER \
 gettext("No owner is defined for this file so you cannot send a comment")
#define NO_OWNER_USE gettext("No owner is defined. Use %s?")
#define CONFIRM_COMMENT gettext("Do you wish to send a comment?")
#define MAIL_DISALLOWED gettext("Mail is disallowed so you cannot send a comment")
#define EDIT_DISABLED gettext("The 'e'dit command is currently disabled.")
#define ANYEDIT_DISABLED gettext("External editing is currently disabled.")
#define NO_STATUS gettext("System error - failure to get status.")
#define NO_EDITOR gettext("No editor is defined!")
#define PRINT_DISABLED gettext("The 'p'rint command is currently disabled.")
#define NO_TOOLBAR gettext("Document has no Toolbar links or Banner.")
#define CANNOT_OPEN_TRAV_FILE gettext("Unable to open traversal file.")
#define CANNOT_OPEN_TRAF_FILE gettext("Unable to open traversal found file.")
#define CANNOT_OPEN_REJ_FILE gettext("Unable to open reject file.")
#define NOOPEN_TRAV_ERR_FILE gettext("Unable to open traversal errors output file")
#define TRAV_WAS_INTERRUPTED gettext("TRAVERSAL WAS INTERRUPTED")
#define FOLLOW_LINK_NUMBER gettext("Follow link (or goto link or page) number: ")
#define SELECT_OPTION_NUMBER gettext("Select option (or page) number: ")
#define OPTION_ALREADY_CURRENT gettext("Option number %d already is current.")
#define ALREADY_AT_OPTION_END \
 gettext("You are already at the end of this option list.")
#define ALREADY_AT_OPTION_BEGIN \
 gettext("You are already at the beginning of this option list.")
#define ALREADY_AT_OPTION_PAGE \
 gettext("You are already at page %d of this option list.")
#define BAD_OPTION_NUM_ENTERED gettext("You have entered an invalid option number.")
#define BAD_HTML_USE_TRACE gettext("** Bad HTML!!  Use -trace to diagnose. **")
#define GIVE_FILENAME gettext("Give name of file to save in")
#define CANNOT_SAVE_REMOTE gettext("Can't save data to file -- please run WWW locally")
#define CANNOT_OPEN_TEMP gettext("Can't open temporary file!")
#define CANNOT_OPEN_OUTPUT gettext("Can't open output file!  Cancelling!")
#define EXECUTION_DISABLED gettext("Execution is disabled.")
#define EXECUTION_DISABLED_FOR_FILE \
 gettext("Execution is not enabled for this file.  See the Options menu (use %s).")
#define EXECUTION_NOT_COMPILED \
 gettext("Execution capabilities are not compiled into this version.")
#define CANNOT_DISPLAY_FILE gettext("This file cannot be displayed on this terminal.")
#define CANNOT_DISPLAY_FILE_D_OR_C \
 gettext("This file cannot be displayed on this terminal:  D)ownload, or C)ancel")
#define MSG_DOWNLOAD_OR_CANCEL gettext("%s  D)ownload, or C)ancel")
#define CANCELLING_FILE gettext("Cancelling file.")
#define RETRIEVING_FILE gettext("Retrieving file.  - PLEASE WAIT -")
#define FILENAME_PROMPT gettext("Enter a filename: ")
#define EDIT_THE_PREV_FILENAME gettext("Edit the previous filename: ")
#define EDIT_A_PREV_FILENAME gettext("Edit a previous filename: ")
#define NEW_FILENAME_PROMPT gettext("Enter a new filename: ")
#define FILENAME_CANNOT_BE_DOT gettext("File name may not begin with a dot.")
#ifdef VMS
#define FILE_EXISTS_HPROMPT gettext("File exists.  Create higher version?")
#else
#define FILE_EXISTS_OPROMPT gettext("File exists.  Overwrite?")
#endif /* VMS */
#define CANNOT_WRITE_TO_FILE gettext("Cannot write to file.")
#define MISCONF_DOWNLOAD_COMMAND gettext("ERROR! - download command is misconfigured.")
#define CANNOT_DOWNLOAD_FILE gettext("Unable to download file.")
#define READING_DIRECTORY gettext("Reading directory...")
#define BUILDING_DIR_LIST gettext("Building directory listing...")
#define SAVING gettext("Saving...")
#define COULD_NOT_EDIT_FILE gettext("Could not edit file '%s'.")
#define COULD_NOT_ACCESS_DOCUMENT gettext("Unable to access document!")
#define COULD_NOT_ACCESS_FILE gettext("Could not access file.")
#define COULD_NOT_ACCESS_DIR gettext("Could not access directory.")
#define COULD_NOT_LOAD_DATA gettext("Could not load data.")
#define CANNOT_EDIT_REMOTE_FILES \
 gettext("Lynx cannot currently (e)dit remote WWW files.")
#define CANNOT_EDIT_FIELD \
 gettext("This field cannot be (e)dited with an external editor.")
#define RULE_INCORRECT gettext("Bad rule")
#define RULE_NEEDS_DATA gettext("Insufficient operands:")
#define NOAUTH_TO_EDIT_FILE gettext("You are not authorized to edit this file.")
#define TITLE_PROMPT gettext("Title: ")
#define SUBJECT_PROMPT gettext("Subject: ")
#define USERNAME_PROMPT gettext("Username: ")
#define PASSWORD_PROMPT gettext("Password: ")
#define USERNAME_PASSWORD_REQUIRED gettext("lynx: Username and Password required!!!")
#define PASSWORD_REQUIRED gettext("lynx: Password required!!!")
#define CLEAR_ALL_AUTH_INFO gettext("Clear all authorization info for this session?")
#define AUTH_INFO_CLEARED gettext("Authorization info cleared.")
#define AUTH_FAILED_PROMPT gettext("Authorization failed.  Retry?")
#define CGI_DISABLED gettext("cgi support has been disabled.")
#define CGI_NOT_COMPILED \
 gettext("Lynxcgi capabilities are not compiled into this version.")
#define CANNOT_CONVERT_I_TO_O gettext("Sorry, no known way of converting %s to %s.")
#define CONNECT_SET_FAILED gettext("Unable to set up connection.")
#define CONNECT_FAILED gettext("Unable to make connection")
#define MALFORMED_EXEC_REQUEST \
 gettext("Executable link rejected due to malformed request.")
#define BADCHAR_IN_EXEC_LINK \
 gettext("Executable link rejected due to `%c' character.")
#define RELPATH_IN_EXEC_LINK \
 gettext("Executable link rejected due to relative path string ('../').")
#define BADLOCPATH_IN_EXEC_LINK \
 gettext("Executable link rejected due to location or path.")
#define MAIL_DISABLED gettext("Mail access is disabled!")
#define ACCESS_ONLY_LOCALHOST \
 gettext("Only files and servers on the local host can be accessed.")
#define TELNET_DISABLED gettext("Telnet access is disabled!")
#define TELNET_PORT_SPECS_DISABLED \
 gettext("Telnet port specifications are disabled.")
#define NEWS_DISABLED gettext("USENET news access is disabled!")
#define RLOGIN_DISABLED gettext("Rlogin access is disabled!")
#define FTP_DISABLED gettext("Ftp access is disabled!")
#define NO_REFS_FROM_DOC gettext("There are no references from this document.")
#define NO_VISIBLE_REFS_FROM_DOC gettext("There are only hidden links from this document.")
#ifdef VMS
#define CANNOT_OPEN_COMFILE gettext("Unable to open command file.")
#endif /* VMS */
#define NEWS_POST_CANCELLED gettext("News Post Cancelled!!!")
#define SPAWNING_EDITOR_FOR_NEWS \
 gettext("Spawning your selected editor to edit news message")
#define POST_MSG_PROMPT gettext("Post this message?")
#define APPEND_SIG_FILE gettext("Append '%s'?")
#define POSTING_TO_NEWS gettext("Posting to newsgroup(s)...")
#ifdef VMS
#define HAVE_UNREAD_MAIL_MSG gettext("*** You have unread mail. ***")
#else
#define HAVE_MAIL_MSG gettext("*** You have mail. ***")
#endif /* VMS */
#define HAVE_NEW_MAIL_MSG gettext("*** You have new mail. ***")
#define FILE_INSERT_CANCELLED gettext("File insert cancelled!!!")
#define MEMORY_EXHAUSTED_FILE gettext("Not enough memory for file!")
#define FILE_CANNOT_OPEN_R gettext("Can't open file for reading.")
#define FILE_DOES_NOT_EXIST gettext("File does not exist.")
#define FILE_DOES_NOT_EXIST_RE gettext("File does not exist - reenter or cancel:")
#define FILE_NOT_READABLE gettext("File is not readable.")
#define FILE_NOT_READABLE_RE gettext("File is not readable - reenter or cancel:")
#define FILE_INSERT_0_LENGTH gettext("Nothing to insert - file is 0-length.")
#define SAVE_REQUEST_CANCELLED gettext("Save request cancelled!!!")
#define MAIL_REQUEST_CANCELLED gettext("Mail request cancelled!!!")
#define CONFIRM_MAIL_SOURCE_PREPARSED \
 gettext("Viewing preparsed source.  Are you sure you want to mail it?")
#define PLEASE_WAIT gettext("Please wait...")
#define MAILING_FILE gettext("Mailing file.  Please wait...")
#define MAIL_REQUEST_FAILED gettext("ERROR - Unable to mail file")
#define CONFIRM_LONG_SCREEN_PRINT \
 gettext("File is %d screens long.  Are you sure you want to print?")
#define PRINT_REQUEST_CANCELLED gettext("Print request cancelled!!!")
#define PRESS_RETURN_TO_BEGIN gettext("Press <return> to begin: ")
#define PRESS_RETURN_TO_FINISH gettext("Press <return> to finish: ")
#define CONFIRM_LONG_PAGE_PRINT \
 gettext("File is %d pages long.  Are you sure you want to print?")
#define CHECK_PRINTER \
 gettext("Be sure your printer is on-line.  Press <return> to start printing:")
#define FILE_ALLOC_FAILED gettext("ERROR - Unable to allocate file space!!!")
#define UNABLE_TO_OPEN_TEMPFILE gettext("Unable to open tempfile")
#define UNABLE_TO_OPEN_PRINTOP_FILE gettext("Unable to open print options file")
#define PRINTING_FILE gettext("Printing file.  Please wait...")
#define MAIL_ADDRESS_PROMPT gettext("Please enter a valid internet mail address: ")
#define PRINTER_MISCONF_ERROR gettext("ERROR! - printer is misconfigured!")
#define FAILED_MAP_POST_REQUEST gettext("Image map from POST response not available!")
#define MISDIRECTED_MAP_REQUEST gettext("Misdirected client-side image MAP request!")
#define MAP_NOT_ACCESSIBLE gettext("Client-side image MAP is not accessible!")
#define MAPS_NOT_AVAILABLE gettext("No client-side image MAPs are available!")
#define MAP_NOT_AVAILABLE gettext("Client-side image MAP is not available!")
#ifndef NO_OPTION_MENU
#define OPTION_SCREEN_NEEDS_24 \
 gettext("Screen height must be at least 24 lines for the Options menu!")
#define OPTION_SCREEN_NEEDS_23 \
 gettext("Screen height must be at least 23 lines for the Options menu!")
#define OPTION_SCREEN_NEEDS_22 \
 gettext("Screen height must be at least 22 lines for the Options menu!")
#endif /* !NO_OPTION_MENU */
#define NEED_ADVANCED_USER_MODE gettext("That key requires Advanced User mode.")
#define CONTENT_TYPE_MSG gettext("Content-type: %s")
#define COMMAND_PROMPT gettext("Command: ")
#define COMMAND_UNKNOWN gettext("Unknown or ambiguous command")
#define VERSION_SEGMENT gettext(" Version ")
#define FIRST_SEGMENT gettext(" first")
#define GUESSING_SEGMENT gettext(", guessing...")
#define PERMISSIONS_SEGMENT gettext("Permissions for ")
#define SELECT_SEGMENT gettext("Select ")
#define CAP_LETT_SEGMENT gettext("capital letter")
#define OF_OPT_LINE_SEGMENT gettext(" of option line,")
#define TO_SAVE_SEGMENT gettext(" to save,")
#define TO_SEGMENT gettext(" to ")
#define OR_SEGMENT gettext(" or ")
#define INDEX_SEGMENT gettext(" index")
#define TO_RETURN_SEGMENT gettext(" to return to Lynx.")
#define ACCEPT_CHANGES gettext("Accept Changes")
#define RESET_CHANGES gettext("Reset Changes")
#define CANCEL_CHANGES gettext("Left Arrow cancels changes")
#define SAVE_OPTIONS gettext("Save options to disk")
#define ACCEPT_DATA gettext("Hit RETURN to accept entered data.")
#define ACCEPT_DATA_OR_DEFAULT \
 gettext("Hit RETURN to accept entered data.  Delete data to invoke the default.")
#define VALUE_ACCEPTED gettext("Value accepted!")
#define VALUE_ACCEPTED_WARNING_X \
 gettext("Value accepted! -- WARNING: Lynx is configured for XWINDOWS!")
#define VALUE_ACCEPTED_WARNING_NONX \
 gettext("Value accepted! -- WARNING: Lynx is NOT configured for XWINDOWS!")
#define EDITOR_LOCKED gettext("You are not allowed to change which editor to use!")
#define FAILED_TO_SET_DISPLAY gettext("Failed to set DISPLAY variable!")
#define FAILED_CLEAR_SET_DISPLAY gettext("Failed to clear DISPLAY variable!")
#define BOOKMARK_CHANGE_DISALLOWED \
 gettext("You are not allowed to change the bookmark file!")
#define COLOR_TOGGLE_DISABLED gettext("Terminal does not support color")
#define COLOR_TOGGLE_DISABLED_FOR_TERM gettext("Your '%s' terminal does not support color.")
#define DOTFILE_ACCESS_DISABLED gettext("Access to dot files is disabled!")
#define UA_NO_LYNX_WARNING \
 gettext("User-Agent string does not contain \"Lynx\" or \"L_y_n_x\"")
#define UA_PLEASE_USE_LYNX \
 gettext("Use \"L_y_n_x\" or \"Lynx\" in User-Agent, or it looks like intentional deception!")
#define UA_CHANGE_DISABLED \
 gettext("Changing of the User-Agent string is disabled!")
#define CHANGE_OF_SETTING_DISALLOWED \
 gettext("You are not allowed to change this setting.")
#define SAVING_OPTIONS gettext("Saving Options...")
#define OPTIONS_SAVED gettext("Options saved!")
#define OPTIONS_NOT_SAVED gettext("Unable to save Options!")
#define R_TO_RETURN_TO_LYNX gettext(" 'r' to return to Lynx ")
#define SAVE_OR_R_TO_RETURN_TO_LYNX gettext(" '>' to save, or 'r' to return to Lynx ")
#define ANY_KEY_CHANGE_RET_ACCEPT \
 gettext("Hit any key to change value; RETURN to accept.")
#define ERROR_UNCOMPRESSING_TEMP gettext("Error uncompressing temporary file!")
#define UNSUPPORTED_URL_SCHEME gettext("Unsupported URL scheme!")
#define UNSUPPORTED_DATA_URL gettext("Unsupported data: URL!  Use SHOWINFO, for now.")
#define TOO_MANY_REDIRECTIONS gettext("Redirection limit of 10 URL's reached.")
#define ILLEGAL_REDIRECTION_URL gettext("Illegal redirection URL received from server!")
#define	SERVER_ASKED_FOR_REDIRECTION \
 gettext("Server asked for %d redirection of POST content to")
#define REDIRECTION_WITH_BAD_LOCATION "Got redirection with a bad Location header."
#define REDIRECTION_WITH_NO_LOCATION "Got redirection with no Location header."
#define	PROCEED_GET_CANCEL gettext("P)roceed, use G)ET or C)ancel ")
#define	PROCEED_OR_CANCEL gettext("P)roceed, or C)ancel ")
#define	ADVANCED_POST_GET_REDIRECT \
 gettext("Redirection of POST content.  P)roceed, see U)RL, use G)ET or C)ancel")
#define	ADVANCED_POST_REDIRECT \
 gettext("Redirection of POST content.  P)roceed, see U)RL, or C)ancel")
#define CONFIRM_POST_RESUBMISSION \
 gettext("Document from Form with POST content.  Resubmit?")
#define CONFIRM_POST_RESUBMISSION_TO \
 gettext("Resubmit POST content to %s ?")
#define CONFIRM_POST_LIST_RELOAD \
 gettext("List from document with POST data.  Reload %s ?")
#define CONFIRM_POST_DOC_HEAD \
 gettext("Document from POST action, HEAD may not be understood.  Proceed?")
#define CONFIRM_POST_LINK_HEAD \
 gettext("Form submit action is POST, HEAD may not be understood.  Proceed?")
#define CONFIRM_WO_PASSWORD gettext("Proceed without a username and password?")
#define CONFIRM_PROCEED gettext("Proceed (%s)?")
#define CANNOT_POST gettext("Cannot POST to this host.")
#define IGNORED_POST gettext("POST not supported for this URL - ignoring POST data!")
#define DISCARDING_POST_DATA gettext("Discarding POST data...")
#define WILL_NOT_RELOAD_DOC gettext("Document will not be reloaded!")
#define	LOCATION_HEADER gettext("Location: ")
#define STRING_NOT_FOUND gettext("'%s' not found!")
#define MULTIBOOKMARKS_DEFAULT gettext("Default Bookmark File")
#define MULTIBOOKMARKS_SMALL gettext("Screen too small! (8x35 min)")
#define MULTIBOOKMARKS_SAVE gettext("Select destination or ^G to Cancel: ")
#define MULTIBOOKMARKS_SELECT \
 gettext("Select subbookmark, '=' for menu, or ^G to cancel: ")
#define MULTIBOOKMARKS_SELF \
 gettext("Reproduce L)ink in this bookmark file or C)ancel? (l,c): ")
#define MULTIBOOKMARKS_DISALLOWED gettext("Multiple bookmark support is not available.")
#define MULTIBOOKMARKS_SHEAD_MASK gettext(" Select Bookmark (screen %d of %d)")
#define MULTIBOOKMARKS_SHEAD gettext("       Select Bookmark")
#define MULTIBOOKMARKS_EHEAD_MASK \
 gettext("Editing Bookmark DESCRIPTION and FILEPATH (%d of 2)")
#define MULTIBOOKMARKS_EHEAD \
 gettext("         Editing Bookmark DESCRIPTION and FILEPATH")
#define MULTIBOOKMARKS_LETTER gettext("Letter: ")
#ifdef VMS
#define USE_PATH_OFF_HOME \
 gettext("Use a filepath off your login directory in SHELL syntax!")
#else
#define USE_PATH_OFF_HOME gettext("Use a filepath off your home directory!")
#endif /* VMS */
#define MAXLINKS_REACHED \
 gettext("Maximum links per page exceeded!  Use half-page or two-line scrolling.")
#define MAXHIST_REACHED \
 gettext("History List maximum reached!  Document not pushed.")
#define VISITED_LINKS_EMPTY gettext("No previously visited links available!")
#define MEMORY_EXHAUSTED_ABORT gettext("Memory exhausted!  Program aborted!")
#define MEMORY_EXHAUSTED_ABORTING gettext("Memory exhausted!  Aborting...")
#define NOT_ENOUGH_MEMORY gettext("Not enough memory!")
#define DFM_NOT_AVAILABLE gettext("Directory/File Manager not available")
#define BASE_NOT_ABSOLUTE gettext("HREF in BASE tag is not an absolute URL.")
#define LOCATION_NOT_ABSOLUTE gettext("Location URL is not absolute.")
#define REFRESH_URL_NOT_ABSOLUTE gettext("Refresh URL is not absolute.")
#define SENDING_MESSAGE_WITH_BODY_TO \
 gettext("You are sending a message with body to:\n  ")
#define SENDING_COMMENT_TO gettext("You are sending a comment to:\n  ")
#define WITH_COPY_TO gettext("\n With copy to:\n  ")
#define WITH_COPIES_TO gettext("\n With copies to:\n  ")
#define CTRL_G_TO_CANCEL_SEND \
 gettext("\n\nUse Ctrl-G to cancel if you do not want to send a message\n")
#define ENTER_NAME_OR_BLANK \
 gettext("\n Please enter your name, or leave it blank to remain anonymous\n")
#define ENTER_MAIL_ADDRESS_OR_OTHER \
 gettext("\n Please enter a mail address or some other\n")
#define MEANS_TO_CONTACT_FOR_RESPONSE \
 gettext(" means to contact you, if you desire a response.\n")
#define ENTER_SUBJECT_LINE gettext("\n Please enter a subject line.\n")
#define ENTER_ADDRESS_FOR_CC \
 gettext("\n Enter a mail address for a CC of your message.\n")
#define BLANK_FOR_NO_COPY gettext(" (Leave blank if you don't want a copy.)\n")
#define REVIEW_MESSAGE_BODY gettext("\n Please review the message body:\n\n")
#define RETURN_TO_CONTINUE gettext("\nPress RETURN to continue: ")
#define RETURN_TO_CLEANUP gettext("\nPress RETURN to clean up: ")
#define CTRL_U_TO_ERASE gettext(" Use Control-U to erase the default.\n")
#define ENTER_MESSAGE_BELOW gettext("\n Please enter your message below.")
#define ENTER_PERIOD_WHEN_DONE_A \
 gettext("\n When you are done, press enter and put a single period (.)")
#define ENTER_PERIOD_WHEN_DONE_B \
 gettext("\n on a line and press enter again.")

/* Cookies messages */
#define ADVANCED_COOKIE_CONFIRMATION \
 gettext("%s cookie: %.*s=%.*s  Allow? (Y/N/Always/neVer)")
#define INVALID_COOKIE_DOMAIN_CONFIRMATION \
 gettext("Accept invalid cookie domain=%s for '%s'?")
#define INVALID_COOKIE_PATH_CONFIRMATION \
 gettext("Accept invalid cookie path=%s as a prefix of '%s'?")
#define ALLOWING_COOKIE gettext("Allowing this cookie.")
#define REJECTING_COOKIE gettext("Rejecting this cookie.")
#define COOKIE_JAR_IS_EMPTY gettext("The Cookie Jar is empty.")
#define ACTIVATE_TO_GOBBLE \
 gettext("Activate links to gobble up cookies or entire domains,")
#define OR_CHANGE_ALLOW gettext("or to change a domain's 'allow' setting.")
#define COOKIES_NEVER_ALLOWED gettext("(Cookies never allowed.)")
#define COOKIES_ALWAYS_ALLOWED gettext("(Cookies always allowed.)")
#define COOKIES_ALLOWED_VIA_PROMPT gettext("(Cookies allowed via prompt.)")
#define COOKIES_READ_FROM_FILE gettext("(Persistent Cookies.)")
#define NO_TITLE gettext("(No title.)")
#define NO_NAME gettext("(No name.)")
#define NO_VALUE gettext("(No value.)")
#define NO_NOTHING gettext("None")
#define END_OF_SESSION gettext("(End of session.)")
#define DELETE_COOKIE_CONFIRMATION gettext("Delete this cookie?")
#define COOKIE_EATEN gettext("The cookie has been eaten!")
#define DELETE_EMPTY_DOMAIN_CONFIRMATION gettext("Delete this empty domain?")
#define DOMAIN_EATEN gettext("The domain has been eaten!")
#define DELETE_COOKIES_SET_ALLOW_OR_CANCEL \
 gettext("D)elete domain's cookies, set allow A)lways/P)rompt/neV)er, or C)ancel? ")
#define DELETE_DOMAIN_SET_ALLOW_OR_CANCEL \
 gettext("D)elete domain, set allow A)lways/P)rompt/neV)er, or C)ancel? ")
#define DOMAIN_COOKIES_EATEN gettext("All cookies in the domain have been eaten!")
#define ALWAYS_ALLOWING_COOKIES gettext("'A'lways allowing from domain '%s'.")
#define NEVER_ALLOWING_COOKIES gettext("ne'V'er allowing from domain '%s'.")
#define PROMPTING_TO_ALLOW_COOKIES gettext("'P'rompting to allow from domain '%s'.")
#define DELETE_ALL_COOKIES_IN_DOMAIN gettext("Delete all cookies in this domain?")
#define ALL_COOKIES_EATEN gettext("All of the cookies in the jar have been eaten!")

#define PORT_NINETEEN_INVALID gettext("Port 19 not permitted in URLs.")
#define PORT_TWENTYFIVE_INVALID gettext("Port 25 not permitted in URLs.")
#define PORT_INVALID gettext("Port %lu not permitted in URLs.")
#define URL_PORT_BAD gettext("URL has a bad port field.")
#define HTML_STACK_OVERRUN gettext("Maximum nesting of HTML elements exceeded.")
#define BAD_PARTIAL_REFERENCE gettext("Bad partial reference!  Stripping lead dots.")
#define TRACELOG_OPEN_FAILED gettext("Trace Log open failed.  Trace off!")
#define LYNX_TRACELOG_TITLE gettext("Lynx Trace Log")
#define NO_TRACELOG_STARTED gettext("No trace log has been started for this session.")
#define MAX_TEMPCOUNT_REACHED \
 gettext("The maximum temporary file count has been reached!")
#define FORM_VALUE_TOO_LONG \
 gettext("Form field value exceeds buffer length!  Trim the tail.")
#define FORM_TAIL_COMBINED_WITH_HEAD \
 gettext("Modified tail combined with head of form field value.")

/* HTFile.c */
#define ENTRY_IS_DIRECTORY      gettext("Directory")
#define DISALLOWED_DIR_SCAN     gettext("Directory browsing is not allowed.")
#define DISALLOWED_SELECTIVE_ACCESS gettext("Selective access is not enabled for this directory")
#define FAILED_DIR_SCAN         gettext("Multiformat: directory scan failed.")
#define FAILED_DIR_UNREADABLE   gettext("This directory is not readable.")
#define FAILED_FILE_UNREADABLE  gettext("Can't access requested file.")
#define FAILED_NO_REPRESENTATION gettext("Could not find suitable representation for transmission.")
#define FAILED_OPEN_COMPRESSED_FILE gettext("Could not open file for decompression!")
#define LABEL_FILES             gettext("Files:")
#define LABEL_SUBDIRECTORIES    gettext("Subdirectories:")
#define SEGMENT_DIRECTORY       gettext(" directory")
#define SEGMENT_UP_TO           gettext("Up to ")
#define SEGMENT_CURRENT_DIR     gettext("Current directory is ")

/* HTGopher.c */
#define FAILED_NO_RESPONSE      gettext("No response from server!")
#define GOPHER_CSO_INDEX        gettext("CSO index")
#define GOPHER_CSO_INDEX_SUBTITLE gettext("\nThis is a searchable index of a CSO database.\n")
#define GOPHER_CSO_SEARCH_RESULTS gettext("CSO Search Results")
#define GOPHER_CSO_SEEK_FAILED  gettext("Seek fail on %s\n")
#define GOPHER_CSO_SOLICIT_KEYWORDS gettext("\nPress the 's' key and enter search keywords.\n")
#define GOPHER_INDEX_SUBTITLE   gettext("\nThis is a searchable Gopher index.\n")
#define GOPHER_INDEX_TITLE      gettext("Gopher index")
#define GOPHER_MENU_TITLE       gettext("Gopher Menu")
#define GOPHER_SEARCH_RESULTS   gettext(" Search Results")
#define GOPHER_SENDING_CSO_REQUEST gettext("Sending CSO/PH request.")
#define GOPHER_SENDING_REQUEST  gettext("Sending Gopher request.")
#define GOPHER_SENT_CSO_REQUEST gettext("CSO/PH request sent; waiting for response.")
#define GOPHER_SENT_REQUEST     gettext("Gopher request sent; waiting for response.")
#define GOPHER_SOLICIT_KEYWORDS gettext("\nPlease enter search keywords.\n")
#define SEGMENT_KEYWORDS_WILL   gettext("\nThe keywords that you enter will allow you to search on a")
#define SEGMENT_PERSONS_DB_NAME gettext(" person's name in the database.\n")

/* HTNews.c */
#define FAILED_CONNECTION_CLOSED gettext("Connection closed ???")
#define FAILED_CANNOT_OPEN_POST gettext("Cannot open temporary file for news POST.")
#define FAILED_CANNOT_POST_SSL  gettext("This client does not contain support for posting to news with SSL.")

/* HTStyle.c */
#define STYLE_DUMP_FONT         gettext("Style %d `%s' SGML:%s.  Font %s %.1f point.\n")
#define STYLE_DUMP_INDENT       gettext("\tIndents: first=%.0f others=%.0f, Height=%.1f Desc=%.1f\n"
#define STYLE_DUMP_ALIGN        gettext("\tAlign=%d, %d tabs. (%.0f before, %.0f after)\n")
#define STYLE_DUMP_TAB          gettext("\t\tTab kind=%d at %.0f\n")

/* HTTP.c */
#define FAILED_NEED_PASSWD      gettext("Can't proceed without a username and password.")
#define FAILED_RETRY_WITH_AUTH  gettext("Can't retry with authorization!  Contact the server's WebMaster.")
#define FAILED_RETRY_WITH_PROXY gettext("Can't retry with proxy authorization!  Contact the server's WebMaster.")
#define HTTP_RETRY_WITH_PROXY   gettext("Retrying with proxy authorization information.")

/* HTWAIS.c */
#define HTWAIS_MESSAGE_TOO_BIG  gettext("HTWAIS: Return message too large.")
#define HTWAIS_SOLICIT_QUERY    gettext("Enter WAIS query: ")

/* Miscellaneous status */
#define RETRYING_AS_HTTP0       gettext("Retrying as HTTP0 request.")
#define TRANSFERRED_X_BYTES     gettext("Transferred %d bytes")
#define TRANSFER_COMPLETE       gettext("Data transfer complete")
#define FAILED_READING_KEYMAP   gettext("Error processing line %d of %s\n")

/* Lynx internal page titles */
#define ADDRLIST_PAGE_TITLE	gettext("Address List Page")
#define BOOKMARK_TITLE		gettext("Bookmark file")
#define CONFIG_DEF_TITLE	gettext("Configuration Definitions")
#define COOKIE_JAR_TITLE	gettext("Cookie Jar")
#define CURRENT_KEYMAP_TITLE	gettext("Current Key Map")
#define DIRED_MENU_TITLE	gettext("File Management Options")
#define DOWNLOAD_OPTIONS_TITLE	gettext("Download Options")
#define HISTORY_PAGE_TITLE	gettext("History Page")
#define LIST_PAGE_TITLE		gettext("List Page")
#define LYNXCFG_TITLE		gettext("Lynx.cfg Information")
#define MOSAIC_BOOKMARK_TITLE	gettext("Converted Mosaic Hotlist")
#define OPTIONS_TITLE		gettext("Options Menu")
#define PERMIT_OPTIONS_TITLE	gettext("File Permission Options")
#define PRINT_OPTIONS_TITLE	gettext("Printing Options")
#define SHOWINFO_TITLE		gettext("Information about the current document")
#define STATUSLINES_TITLE	gettext("Your recent statusline messages")
#define UPLOAD_OPTIONS_TITLE	gettext("Upload Options")
#define VISITED_LINKS_TITLE	gettext("Visited Links Page")

/* CONFIG_DEF_TITLE subtitles */
#define SEE_ALSO gettext("See also")
#define YOUR_SEGMENT gettext("your")
#define RUNTIME_OPT_SEGMENT gettext("for runtime options")
#define COMPILE_OPT_SEGMENT gettext("compile time options")
#define COLOR_STYLE_SEGMENT gettext("color-style configuration")
#define REL_VERSION gettext("latest release")
#define PRE_VERSION gettext("pre-release version")
#define DEV_VERSION gettext("development version")
#define AUTOCONF_CONFIG_CACHE \
 gettext("The following data were derived during the automatic configuration/build\n\
process of this copy of Lynx.  When reporting a bug, please include a copy\n\
of this page.")
#define AUTOCONF_LYNXCFG_H \
 gettext("The following data were used as automatically-configured compile-time\n\
definitions when this copy of Lynx was built.")

#ifdef DIRED_SUPPORT
#define DIRED_NOVICELINE \
 gettext("  C)reate  D)ownload  E)dit  F)ull menu  M)odify  R)emove  T)ag  U)pload     \n")
#define CURRENT_LINK_STATUS_FAILED gettext("Failed to obtain status of current link!")

#define INVALID_PERMIT_URL \
 gettext("Special URL only valid from current File Permission menu!")
#endif /* DIRED_SUPPORT */

#ifdef USE_EXTERNALS
#define EXTERNALS_DISABLED gettext("External support is currently disabled.")
#endif /* USE_EXTERNALS */

/* new with 2.8.4dev.21 */
#define CHDIR_DISABLED gettext("Changing working-directory is currently disabled.")
#define LINEWRAP_OFF gettext("Linewrap OFF!")
#define LINEWRAP_ON gettext("Linewrap ON!")
#define NESTED_TABLES_OFF gettext("Parsing nested-tables toggled OFF!  Reloading...")
#define NESTED_TABLES_ON gettext("Parsing nested-tables toggled ON!  Reloading...")
#define SHIFT_VS_LINEWRAP gettext("Shifting is disabled while line-wrap is in effect")
#define TRACE_DISABLED gettext("Trace not supported")

#endif /* LYMESSAGES_EN_H */
