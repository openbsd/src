;;; This code started from the following message of long time ago (IZ):

;;; From: olson@mcs.anl.gov (Bob Olson)
;;; Newsgroups: comp.lang.perl
;;; Subject: cperl-mode: Another perl mode for Gnuemacs
;;; Date: 14 Aug 91 15:20:01 GMT

;; Perl code editing commands for Emacs
;; Copyright (C) 1985, 1986, 1987 Free Software Foundation, Inc.

;; This file is not (yet) part of GNU Emacs.

;; GNU Emacs is free software; you can redistribute it and/or modify
;; it under the terms of the GNU General Public License as published by
;; the Free Software Foundation; either version 2, or (at your option)
;; any later version.

;; GNU Emacs is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;; GNU General Public License for more details.

;; You should have received a copy of the GNU General Public License
;; along with GNU Emacs; see the file COPYING.  If not, write to
;; the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.

;;; Corrections made by Ilya Zakharevich ilya@math.mps.ohio-state.edu
;;; XEmacs changes by Peter Arius arius@informatik.uni-erlangen.de

;; $Id: cperl-mode.el,v 1.20 1996/02/09 03:40:01 ilya Exp ilya $

;;; To use this mode put the following into your .emacs file:

;; (autoload 'perl-mode "cperl-mode" "alternate mode for editing Perl programs" t)

;;; You can either fine-tune the bells and whistles of this mode or
;;; bulk enable them by putting

;; (setq cperl-hairy t)

;;; in your .emacs file. (Emacs rulers do not consider it politically
;;; correct to make whistles enabled by default.)

;;; Additional useful commands to put into your .emacs file:

;; (setq auto-mode-alist
;;       (append '(("\\.[pP][Llm]$" . perl-mode))  auto-mode-alist ))
;; (setq interpreter-mode-alist (append interpreter-mode-alist
;; 				        '(("miniperl" . perl-mode))))

;;; The mode information (on C-h m) provides customization help.
;;; If you use font-lock feature of this mode, it is advisable to use
;;; eather lazy-lock-mode or fast-lock-mode (available on ELisp
;;; archive in files lazy-lock.el and fast-lock.el). I prefer lazy-lock.

;;; Faces used now: three faces for first-class and second-class keywords
;;; and control flow words, one for each: comments, string, labels,
;;; functions definitions and packages, arrays, hashes, and variable
;;; definitions. If you do not see all these faces, your font-lock does
;;; not define them, so you need to define them manually. Maybe you have 
;;; an obsolete font-lock from 19.28 or earlier. Upgrade.

;;; If you have grayscale monitor, and do not have the variable
;;; font-lock-display-type bound to 'grayscale, insert 

;;; (setq font-lock-display-type 'grayscale)

;;; to your .emacs file.

;;;; This mode supports font-lock, imenu and mode-compile. In the
;;;; hairy version font-lock is on, but you should activate imenu
;;;; yourself (note that mode-compile is not standard yet). Well, you
;;;; can use imenu from keyboard anyway (M-x imenu), but it is better
;;;; to bind it like that:

;; (define-key global-map [M-S-down-mouse-3] 'imenu)

;;; In fact the version of font-lock that this version supports can be
;;; much newer than the version you actually have. This means that a
;;; lot of faces can be set up, but are not visible on your screen
;;; since the coloring rules for this faces are not defined.

;;; Updates: ========================================

;;; Made less hairy by default: parentheses not electric, 
;;; linefeed not magic. Bug with abbrev-mode corrected.

;;;; After 1.4:
;;;  Better indentation:
;;;  subs inside braces should work now, 
;;;  Toplevel braces obey customization.
;;;  indent-for-comment knows about bad cases, cperl-indent-for-comment
;;;  moves cursor to a correct place.
;;;  cperl-indent-exp written from the scratch! Slow... (quadratic!) :-( 
;;;        (50 secs on DB::DB (sub of 430 lines), 486/66)
;;;  Minor documentation fixes.
;;;  Imenu understands packages as prefixes (including nested).
;;;  Hairy options can be switched off one-by-one by setting to null.
;;;  Names of functions and variables changed to conform to `cperl-' style.

;;;; After 1.5:
;;;  Some bugs with indentation of labels (and embedded subs) corrected.
;;;  `cperl-indent-region' done (slow :-()).
;;;  `cperl-fill-paragraph' done.
;;;  Better package support for `imenu'.
;;;  Progress indicator for indentation (with `imenu' loaded).
;;;  `Cperl-set' was busted, now setting the individual hairy option 
;;;     should be better.

;;;; After 1.6:
;;; `cperl-set-style' done.
;;; `cperl-check-syntax' done.
;;; Menu done.
;;; New config variables `cperl-close-paren-offset' and `cperl-comment-column'.
;;; Bugs with `cperl-auto-newline' corrected.
;;; `cperl-electric-lbrace' can work with `cperl-auto-newline' in situation 
;;; like $hash{.

;;;; 1.7 XEmacs (arius@informatik.uni-erlangen.de):
;;; - use `next-command-event', if `next-command-events' does not exist
;;; - use `find-face' as def. of `is-face'
;;; - corrected def. of `x-color-defined-p'
;;; - added const defs for font-lock-comment-face,
;;;   font-lock-keyword-face and font-lock-function-name-face
;;; - added def. of font-lock-variable-name-face
;;; - added (require 'easymenu) inside an `eval-when-compile'
;;; - replaced 4-argument `substitute-key-definition' with ordinary
;;;   `define-key's
;;; - replaced `mark-active' in menu definition by `cperl-use-region-p'.
;;; Todo (at least):
;;; - use emacs-vers.el (http://www.cs.utah.edu/~eeide/emacs/emacs-vers.el.gz)
;;;   for portable code?
;;; - should `cperl-mode' do a 
;;;	(if (featurep 'easymenu) (easy-menu-add cperl-menu))
;;;   or should this be left to the user's `cperl-mode-hook'?

;;; Some bugs introduced by the above fix corrected (IZ ;-).
;;; Some bugs under XEmacs introduced by the correction corrected.

;;; Some more can remain since there are two many different variants. 
;;; Please feedback!

;;; We do not support fontification of arrays and hashes under 
;;; obsolete font-lock any more. Upgrade.

;;;; after 1.8 Minor bug with parentheses.
;;;; after 1.9 Improvements from Joe Marzot.
;;;; after 1.10
;;;  Does not need easymenu to compile under XEmacs.
;;;  `vc-insert-headers' should work better.
;;;  Should work with 19.29 and 19.12.
;;;  Small improvements to fontification.
;;;  Expansion of keywords does not depend on C-? being backspace.

;;; after 1.10+
;;; 19.29 and 19.12 supported.
;;; `cperl-font-lock-enhanced' deprecated. Use font-lock-extra.el.
;;; Support for font-lock-extra.el.

;;;; After 1.11:
;;; Tools submenu.
;;; Support for perl5-info.
;;; `imenu-go-find-at-position' in Tools requires imenu-go.el (see hints above)
;;; Imenu entries do not work with stock imenu.el. Patch sent to maintainers.
;;; Fontifies `require a if b;', __DATA__.
;;; Arglist for auto-fill-mode was incorrect.

;;;; After 1.12:
;;; `cperl-lineup-step' and `cperl-lineup' added: lineup constructions 
;;; vertically.
;;; `cperl-do-auto-fill' updated for 19.29 style.
;;; `cperl-info-on-command' now has a default.
;;; Workaround for broken C-h on XEmacs.
;;; VC strings escaped.
;;; C-h f now may prompt for function name instead of going on,
;;; controlled by `cperl-info-on-command-no-prompt'.

;;;; After 1.13:
;;; Msb buffer list includes perl files
;;; Indent-for-comment uses indent-to
;;; Can write tag files using etags.

;;;; After 1.14:
;;; Recognizes (tries to ;-) {...} which are not blocks during indentation.
;;; `cperl-close-paren-offset' affects ?\] too (and ?\} if not block)
;;; Bug with auto-filling comments started with "##" corrected.

;;;; Very slow now: on DB::DB 0.91, 486/66:

;;;Function Name                             Call Count  Elapsed Time  Average Time
;;;========================================  ==========  ============  ============
;;;cperl-block-p                             469         3.7799999999  0.0080597014
;;;cperl-get-state                           505         163.39000000  0.3235445544
;;;cperl-comment-indent                      12          0.0299999999  0.0024999999
;;;cperl-backward-to-noncomment              939         4.4599999999  0.0047497337
;;;cperl-calculate-indent                    505         172.22000000  0.3410297029
;;;cperl-indent-line                         505         172.88000000  0.3423366336
;;;cperl-use-region-p                        40          0.0299999999  0.0007499999
;;;cperl-indent-exp                          1           177.97000000  177.97000000
;;;cperl-to-comment-or-eol                   1453        3.9800000000  0.0027391603
;;;cperl-backward-to-start-of-continued-exp  9           0.0300000000  0.0033333333
;;;cperl-indent-region                       1           177.94000000  177.94000000

;;;; After 1.15:
;;; Takes into account white space after opening parentheses during indent.
;;; May highlight pods and here-documents: see `cperl-pod-here-scan',
;;; `cperl-pod-here-fontify', `cperl-pod-face'. Does not use this info
;;; for indentation so far.
;;; Fontification updated to 19.30 style. 
;;; The change 19.29->30 did not add all the required functionality,
;;;     but broke "font-lock-extra.el". Get "choose-color.el" from
;;;       ftp://ftp.math.ohio-state.edu/pub/users/ilya/emacs

;;;; After 1.16:
;;;       else # comment
;;;    recognized as a start of a block.
;;;  Two different font-lock-levels provided.
;;;  `cperl-pod-head-face' introduced. Used for highlighting.
;;;  `imenu' marks pods, +Packages moved to the head. 

;;;; After 1.17:
;;;  Scan for pods highlights here-docs too.
;;;  Note that the tag of here-doc may be rehighlighted later by lazy-lock.
;;;  Only one here-doc-tag per line is supported, and one in comment
;;;  or a string may break fontification.
;;;  POD headers were supposed to fill one line only.

;;;; After 1.18:
;;;  `font-lock-keywords' were set in 19.30 style _always_. Current scheme 
;;;    may  break under XEmacs.
;;;  `cperl-calculate-indent' dis suppose that `parse-start' was defined.
;;;  `fontified' tag is added to fontified text as well as `lazy-lock' (for
;;;    compatibility with older lazy-lock.el) (older one overfontifies
;;;    something nevertheless :-().
;;;  Will not indent something inside pod and here-documents.
;;;  Fontifies the package name after import/no/bootstrap.
;;;  Added new entry to menu with meta-info about the mode.

;;;; After 1.19:
;;;  Prefontification works much better with 19.29. Should be checked
;;;   with 19.30 as well.
;;;  Some misprints in docs corrected.
;;;  Now $a{-text} and -text => "blah" are fontified as strings too.
;;;  Now the pod search is much stricter, so it can help you to find
;;;    pod sections which are broken because of whitespace before =blah
;;;    - just observe the fontification.

(defvar cperl-extra-newline-before-brace nil
  "*Non-nil means that if, elsif, while, until, else, for, foreach
and do constructs look like:

	if ()
	{
	}

instead of:

	if () {
	}
")

(defvar cperl-indent-level 2
  "*Indentation of CPerl statements with respect to containing block.")
(defvar cperl-lineup-step nil
  "*`cperl-lineup' will always lineup at multiple of this number.
If `nil', the value of `cperl-indent-level' will be used.")
(defvar cperl-brace-imaginary-offset 0
  "*Imagined indentation of a Perl open brace that actually follows a statement.
An open brace following other text is treated as if it were this far
to the right of the start of its line.")
(defvar cperl-brace-offset 0
  "*Extra indentation for braces, compared with other text in same context.")
(defvar cperl-label-offset -2
  "*Offset of CPerl label lines relative to usual indentation.")
(defvar cperl-min-label-indent 1
  "*Minimal offset of CPerl label lines.")
(defvar cperl-continued-statement-offset 2
  "*Extra indent for lines not starting new statements.")
(defvar cperl-continued-brace-offset 0
  "*Extra indent for substatements that start with open-braces.
This is in addition to cperl-continued-statement-offset.")
(defvar cperl-close-paren-offset -1
  "*Extra indent for substatements that start with close-parenthesis.")

(defvar cperl-auto-newline nil
  "*Non-nil means automatically newline before and after braces,
and after colons and semicolons, inserted in CPerl code.")

(defvar cperl-tab-always-indent t
  "*Non-nil means TAB in CPerl mode should always reindent the current line,
regardless of where in the line point is when the TAB command is used.")

(defvar cperl-font-lock nil
  "*Non-nil (and non-null) means CPerl buffers will use font-lock-mode.
Can be overwritten by `cperl-hairy' if nil.")

(defvar cperl-electric-lbrace-space nil
  "*Non-nil (and non-null) means { after $ in CPerl buffers should be preceeded by ` '.
Can be overwritten by `cperl-hairy' if nil.")

(defvar cperl-electric-parens ""
  "*List of parentheses that should be electric in CPerl, or null.
Can be overwritten by `cperl-hairy' to \"({[<\" if not 'null.")

(defvar cperl-electric-linefeed nil
  "*If true, LFD should be hairy in CPerl, otherwise C-c LFD is hairy.
In any case these two mean plain and hairy linefeeds together.
Can be overwritten by `cperl-hairy' if nil.")

(defvar cperl-electric-keywords nil
  "*Not-nil (and non-null) means keywords are electric in CPerl.
Can be overwritten by `cperl-hairy' if nil.")

(defvar cperl-hairy nil
  "*Not-nil means all the bells and whistles are enabled in CPerl.")

(defvar cperl-comment-column 32
  "*Column to put comments in CPerl (use \\[cperl-indent]' to lineup with code).")

(defvar cperl-vc-header-alist '((SCCS "$sccs = '%W\%' ;")
				(RCS "$rcs = ' $Id\$ ' ;"))
  "*What to use as `vc-header-alist' in CPerl.")

(defvar cperl-info-on-command-no-prompt nil
  "*Not-nil (and non-null) means not to prompt on C-h f.
The opposite behaviour is always available if prefixed with C-c.
Can be overwritten by `cperl-hairy' if nil.")

(defvar cperl-pod-face 'font-lock-comment-face
  "*The result of evaluation of this expression is used for pod highlighting.")

(defvar cperl-pod-head-face 'font-lock-variable-name-face
  "*The result of evaluation of this expression is used for pod highlighting.
Font for POD headers.")

(defvar cperl-here-face 'font-lock-string-face
  "*The result of evaluation of this expression is used for here-docs highlighting.")

(defvar cperl-pod-here-fontify '(featurep 'font-lock)
  "*Not-nil after evaluation means to highlight pod and here-docs sections.")

(defvar cperl-pod-here-scan t
  "*Not-nil means look for pod and here-docs sections during startup.
You can always make lookup from menu or using \\[cperl-find-pods-heres].")



;;; Short extra-docs.

(defvar cperl-tips 'please-ignore-this-line
  "Get newest version of this package from
  ftp://ftp.math.ohio-state.edu/pub/users/ilya/emacs
and/or
  ftp://ftp.math.ohio-state.edu/pub/users/ilya/perl

Get support packages font-lock-extra.el, imenu-go.el from the same place.
\(Look for other files there too... ;-) Get a patch for imenu.el in 19.29.
Note that for 19.30 you should use choose-color.el *instead* of 
font-lock-extra.el (and you will not get smart highlighting in C :-().

Note that to enable Compile choices in the menu you need to install
mode-compile.el.

Get perl5-info from 
  http://www.metronet.com:70/9/perlinfo/perl5/manual/perl5-info.tar.gz
\(may be quite obsolete, but still useful).

If you use imenu-go, run imenu on perl5-info buffer (you can do it from
CPerl menu).

Before reporting (non-)problems look in the problem section on what I
know about them.")

(defvar cperl-problems 'please-ignore-this-line
"Emacs has a _very_ restricted syntax parsing engine. 

It may be corrected on the level of C ocde, please look in the
`non-problems' section if you want to volonteer.

CPerl mode tries to corrects some Emacs misunderstandings, however,
for effeciency reasons the degree of correction is different for
different operations. The partially corrected problems are: POD
sections, here-documents, regexps. The operations are: highlighting,
indentation, electric keywords, electric braces. 

This may be confusing, since the regexp s#//#/#\; may be highlighted
as a comment, but it will recognized as a regexp by the indentation
code. Or the opposite case, when a pod section is highlighted, but
breaks the indentation of the following code.

The main trick (to make $ a \"backslash\") makes constructions like
${aaa} look like unbalanced braces. The only trick I can think out is
to insert it as $ {aaa} (legal in perl5, not in perl4). 

Similar problems arise in regexps, when /(\\s|$)/ should be rewritten
as /($|\\s)/. Note that such a transpositinon is not always possible
:-(.  " )

(defvar cperl-non-problems 'please-ignore-this-line
"As you know from `problems' section, Perl syntax too hard for CPerl.

Most the time, if you write your own code, you may find an equivalent
\(and almost as readable) expression.

Try to help it: add comments with embedded quotes to fix CPerl
misunderstandings about the end of quotation:

$a='500$';      # ';

You won't need it too often. The reason: $ \"quotes\" the following
character (this saves a life a lot of times in CPerl), thus due to
Emacs parsing rules it does not consider tick after the dollar as a
closing one, but as a usual character.

Now the indentation code is pretty wise. The only drawback is that it
relies on Emacs parsing to find matching parentheses. And Emacs
*cannot* match parentheses in Perl 100% correctly. So
	1 if s#//#/#;
will not break indentation, but
	1 if ( s#//#/# );
will.

If you still get wrong indentation in situation that you think the
code should be able to parse, try:

a) Check what Emacs thinks about balance of your parentheses.
b) Supply the code to me (IZ).

Pods are treated _very_ rudimentally. Here-documents are not treated
at all (except highlighting and inhibiting indentation). (This may
change some time. RMS approved making syntax lookup recognize text
attributes, but volonteers are needed to change Emacs C code.)

To speed up coloring the following compromises exist:
   a) sub in $mypackage::sub may be highlighted.
   b) -z in [a-z] may be highlighted.
   c) if your regexp contains a keyword (like \"s\"), it may be highlighted.
")



;;; Portability stuff:

(defsubst cperl-xemacs-p ()
  (string-match "XEmacs\\|Lucid" emacs-version))

(defvar del-back-ch (car (append (where-is-internal 'delete-backward-char)
				 (where-is-internal 'backward-delete-char-untabify)))
  "Character generated by key bound to delete-backward-char.")

(and (vectorp del-back-ch) (= (length del-back-ch) 1) 
     (setq del-back-ch (aref del-back-ch 0)))

(if (cperl-xemacs-p)
    ;; "Active regions" are on: use region only if active
    ;; "Active regions" are off: use region unconditionally
    (defun cperl-use-region-p ()
      (if zmacs-regions (mark) t))
  (defun cperl-use-region-p ()
    (if transient-mark-mode mark-active t)))

(defsubst cperl-enable-font-lock ()
  (or (cperl-xemacs-p) window-system))

(if (boundp 'unread-command-events)
    (if (cperl-xemacs-p)
	(defun cperl-putback-char (c)	; XEmacs >= 19.12
	  (setq unread-command-events (list (character-to-event c))))
      (defun cperl-putback-char (c)	; Emacs 19
	(setq unread-command-events (list c))))
  (defun cperl-putback-char (c)		; XEmacs <= 19.11
    (setq unread-command-event (character-to-event c))))

(or (fboundp 'uncomment-region)
    (defun uncomment-region (beg end)
      (interactive "r")
      (comment-region beg end -1)))

(defvar cperl-do-not-fontify
  (if (string< emacs-version "19.30")
      'fontified
    'lazy-lock)
  "Text property which inhibits refontification.")


;;; Probably it is too late to set these guys already, but it can help later:

(setq auto-mode-alist
      (append '(("\\.[pP][Llm]$" . perl-mode))  auto-mode-alist ))
(and (boundp 'interpreter-mode-alist)
     (setq interpreter-mode-alist (append interpreter-mode-alist
					  '(("miniperl" . perl-mode)))))
(if (fboundp 'eval-when-compile)
    (eval-when-compile
      (condition-case nil
	  (require 'imenu)
	(error nil))
      (condition-case nil
	  (require 'easymenu)
	(error nil))
      ;; Calling `cperl-enable-font-lock' below doesn't compile on XEmacs,
      ;; macros instead of defsubsts don't work on Emacs, so we do the
      ;; expansion manually. Any other suggestions?
      (if (or (string-match "XEmacs\\|Lucid" emacs-version)
	      window-system)
	  (require 'font-lock))
      (require 'cl)
      ))

(defvar cperl-mode-abbrev-table nil
  "Abbrev table in use in Cperl-mode buffers.")

(add-hook 'edit-var-mode-alist '(perl-mode (regexp . "^cperl-")))

(defvar cperl-mode-map () "Keymap used in CPerl mode.")

(if cperl-mode-map nil
  (setq cperl-mode-map (make-sparse-keymap))
  (define-key cperl-mode-map "{" 'cperl-electric-lbrace)
  (define-key cperl-mode-map "[" 'cperl-electric-paren)
  (define-key cperl-mode-map "(" 'cperl-electric-paren)
  (define-key cperl-mode-map "<" 'cperl-electric-paren)
  (define-key cperl-mode-map "}" 'cperl-electric-brace)
  (define-key cperl-mode-map ";" 'cperl-electric-semi)
  (define-key cperl-mode-map ":" 'cperl-electric-terminator)
  (define-key cperl-mode-map "\C-j" 'newline-and-indent)
  (define-key cperl-mode-map "\C-c\C-j" 'cperl-linefeed)
  (define-key cperl-mode-map "\e\C-q" 'cperl-indent-exp) ; Usually not bound
  ;;(define-key cperl-mode-map "\M-q" 'cperl-fill-paragraph)
  ;;(define-key cperl-mode-map "\e;" 'cperl-indent-for-comment)
  (define-key cperl-mode-map "\177" 'backward-delete-char-untabify)
  (define-key cperl-mode-map "\t" 'cperl-indent-command)
  (if (cperl-xemacs-p)
      ;; don't clobber the backspace binding:
      (define-key cperl-mode-map [(control h) f] 'cperl-info-on-command)
    (define-key cperl-mode-map "\C-hf" 'cperl-info-on-command))
  (if (cperl-xemacs-p)
      ;; don't clobber the backspace binding:
      (define-key cperl-mode-map [(control c) (control h) f]
	'cperl-info-on-current-command)
    (define-key cperl-mode-map "\C-c\C-hf" 'cperl-info-on-current-command))
  (if (and (cperl-xemacs-p) 
	   (<= emacs-minor-version 11) (<= emacs-major-version 19))
      (progn
	;; substitute-key-definition is usefulness-deenhanced...
	(define-key cperl-mode-map "\M-q" 'cperl-fill-paragraph)
	(define-key cperl-mode-map "\e;" 'cperl-indent-for-comment)
	(define-key cperl-mode-map "\e\C-\\" 'cperl-indent-region))
    (substitute-key-definition
     'indent-sexp 'cperl-indent-exp
     cperl-mode-map global-map)
    (substitute-key-definition
     'fill-paragraph 'cperl-fill-paragraph
     cperl-mode-map global-map)
    (substitute-key-definition
     'indent-region 'cperl-indent-region
     cperl-mode-map global-map)
    (substitute-key-definition
     'indent-for-comment 'cperl-indent-for-comment
     cperl-mode-map global-map)))

(condition-case nil
    (progn
      (require 'easymenu)
      (easy-menu-define cperl-menu cperl-mode-map "Menu for CPerl mode"
         '("Perl"
	   ["Beginning of function" beginning-of-defun t]
	   ["End of function" end-of-defun t]
	   ["Mark function" mark-defun t]
	   ["Indent expression" cperl-indent-exp t]
	   ["Fill paragraph/comment" cperl-fill-paragraph t]
	   ["Line up a construction" cperl-lineup (cperl-use-region-p)]
	   "----"
	   ["Indent region" cperl-indent-region (cperl-use-region-p)]
	   ["Comment region" comment-region (cperl-use-region-p)]
	   ["Uncomment region" uncomment-region (cperl-use-region-p)]
	   "----"
	   ["Run" mode-compile (fboundp 'mode-compile)]
	   ["Kill" mode-compile-kill (and (fboundp 'mode-compile-kill)
					  (get-buffer "*compilation*"))]
	   ["Next error" next-error (get-buffer "*compilation*")]
	   ["Check syntax" cperl-check-syntax (fboundp 'mode-compile)]
	   "----"
	   ["Debugger" perldb t]
	   "----"
	   ("Tools"
	    ["Imenu" imenu (fboundp 'imenu)]
	    ["Imenu on info" cperl-imenu-on-info (featurep 'imenu)]
	    ("Tags"
	     ["Create tags for current file" cperl-etags t]
	     ["Add tags for current file" (cperl-etags t) t]
	     ["Create tags for Perl files in directory" (cperl-etags nil t) t]
	     ["Add tags for Perl files in directory" (cperl-etags t t) t]
	     ["Create tags for Perl files in (sub)directories" 
	      (cperl-etags nil 'recursive) t]
	     ["Add tags for Perl files in (sub)directories"
	      (cperl-etags t 'recursive) t])
	    ["Recalculate PODs" cperl-find-pods-heres t]
	    ["Define word at point" imenu-go-find-at-position 
	     (fboundp 'imenu-go-find-at-position)]
	    ["Help on function" cperl-info-on-command t]
	    ["Help on function at point" cperl-info-on-current-command t])
	   ("Indent styles..."
	    ["GNU" (cperl-set-style "GNU") t]
	    ["C++" (cperl-set-style "C++") t]
	    ["FSF" (cperl-set-style "FSF") t]
	    ["BSD" (cperl-set-style "BSD") t]
	    ["Whitesmith" (cperl-set-style "Whitesmith") t])
	   ("Micro-docs"
	    ["Tips" (describe-variable 'cperl-tips) t]
	    ["Problems" (describe-variable 'cperl-problems) t]
	    ["Non-problems" (describe-variable 'cperl-non-problems) t]))))
  (error nil))

(autoload 'c-macro-expand "cmacexp"
  "Display the result of expanding all C macros occurring in the region.
The expansion is entirely correct because it uses the C preprocessor."
  t)

(defvar cperl-mode-syntax-table nil
  "Syntax table in use in Cperl-mode buffers.")

(if cperl-mode-syntax-table
    ()
  (setq cperl-mode-syntax-table (make-syntax-table))
  (modify-syntax-entry ?\\ "\\" cperl-mode-syntax-table)
  (modify-syntax-entry ?/ "." cperl-mode-syntax-table)
  (modify-syntax-entry ?* "." cperl-mode-syntax-table)
  (modify-syntax-entry ?+ "." cperl-mode-syntax-table)
  (modify-syntax-entry ?- "." cperl-mode-syntax-table)
  (modify-syntax-entry ?= "." cperl-mode-syntax-table)
  (modify-syntax-entry ?% "." cperl-mode-syntax-table)
  (modify-syntax-entry ?< "." cperl-mode-syntax-table)
  (modify-syntax-entry ?> "." cperl-mode-syntax-table)
  (modify-syntax-entry ?& "." cperl-mode-syntax-table)
  (modify-syntax-entry ?$ "\\" cperl-mode-syntax-table)
  (modify-syntax-entry ?\n ">" cperl-mode-syntax-table)
  (modify-syntax-entry ?# "<" cperl-mode-syntax-table)
  (modify-syntax-entry ?' "\"" cperl-mode-syntax-table)
  (modify-syntax-entry ?` "\"" cperl-mode-syntax-table)
  (modify-syntax-entry ?_ "w" cperl-mode-syntax-table)
  (modify-syntax-entry ?| "." cperl-mode-syntax-table))



;; Make customization possible "in reverse"
;;(defun cperl-set (symbol to)
;;  (or (eq (symbol-value symbol) 'null) (set symbol to)))
(defsubst cperl-val (symbol &optional default hairy)
  (cond
   ((eq (symbol-value symbol) 'null) default)
   (cperl-hairy (or hairy t))
   (t (symbol-value symbol))))

;; provide an alias for working with emacs 19.  the perl-mode that comes
;; with it is really bad, and this lets us seamlessly replace it.
(fset 'perl-mode 'cperl-mode)
(defun cperl-mode ()
  "Major mode for editing Perl code.
Expression and list commands understand all C brackets.
Tab indents for Perl code.
Paragraphs are separated by blank lines only.
Delete converts tabs to spaces as it moves back.

Various characters in Perl almost always come in pairs: {}, (), [],
sometimes <>. When the user types the first, she gets the second as
well, with optional special formatting done on {}.  (Disabled by
default.)  You can always quote (with \\[quoted-insert]) the left
\"paren\" to avoid the expansion. The processing of < is special,
since most the time you mean \"less\". Cperl mode tries to guess
whether you want to type pair <>, and inserts is if it
appropriate. You can set `cperl-electric-parens' to the string that
contains the parenths from the above list you want to be electrical.

CPerl mode provides expansion of the Perl control constructs:
   if, else, elsif, unless, while, until, for, and foreach.
=========(Disabled by default, see `cperl-electric-keywords'.)
The user types the keyword immediately followed by a space, which causes
the construct to be expanded, and the user is positioned where she is most
likely to want to be.
eg. when the user types a space following \"if\" the following appears in
the buffer:
            if () {     or   if ()
            }                 {
                              }
and the cursor is between the parentheses.  The user can then type some
boolean expression within the parens.  Having done that, typing
\\[cperl-linefeed] places you, appropriately indented on a new line
between the braces. If CPerl decides that you want to insert
\"English\" style construct like
            bite if angry;
it will not do any expansion. See also help on variable 
`cperl-extra-newline-before-brace'.

\\[cperl-linefeed] is a convinience replacement for typing carriage
return. It places you in the next line with proper indentation, or if
you type it inside the inline block of control construct, like
            foreach (@lines) {print; print}
and you are on a boundary of a statement inside braces, it will
transform the construct into a multiline and will place you into an
apporpriately indented blank line. If you need a usual 
`newline-and-indent' behaviour, it is on \\[newline-and-indent], 
see documentation on `cperl-electric-linefeed'.

\\{cperl-mode-map}

Setting the variable `cperl-font-lock' to t switches on
font-lock-mode, `cperl-electric-lbrace-space' to t switches on
electric space between $ and {, `cperl-electric-parens' is the string
that contains parentheses that should be electric in CPerl, setting
`cperl-electric-keywords' enables electric expansion of control
structures in CPerl. `cperl-electric-linefeed' governs which one of
two linefeed behavior is preferable. You can enable all these options
simultaneously (recommended mode of use) by setting `cperl-hairy' to
t. In this case you can switch separate options off by setting them 
to `null'.

If your site has perl5 documentation in info format, you can use commands
\\[cperl-info-on-current-command] and \\[cperl-info-on-command] to access it.
These keys run commands `cperl-info-on-current-command' and
`cperl-info-on-command', which one is which is controlled by variable
`cperl-info-on-command-no-prompt' (in turn affected by `cperl-hairy').

Variables `cperl-pod-here-scan', `cperl-pod-here-fontify',
`cperl-pod-face', `cperl-pod-head-face' control processing of pod and
here-docs sections. In a future version results of scan may be used
for indentation too, currently they are used for highlighting only.

Variables controlling indentation style:
 `cperl-tab-always-indent'
    Non-nil means TAB in CPerl mode should always reindent the current line,
    regardless of where in the line point is when the TAB command is used.
 `cperl-auto-newline'
    Non-nil means automatically newline before and after braces,
    and after colons and semicolons, inserted in Perl code.
 `cperl-indent-level'
    Indentation of Perl statements within surrounding block.
    The surrounding block's indentation is the indentation
    of the line on which the open-brace appears.
 `cperl-continued-statement-offset'
    Extra indentation given to a substatement, such as the
    then-clause of an if, or body of a while, or just a statement continuation.
 `cperl-continued-brace-offset'
    Extra indentation given to a brace that starts a substatement.
    This is in addition to `cperl-continued-statement-offset'.
 `cperl-brace-offset'
    Extra indentation for line if it starts with an open brace.
 `cperl-brace-imaginary-offset'
    An open brace following other text is treated as if it the line started
    this far to the right of the actual line indentation.
 `cperl-label-offset'
    Extra indentation for line that is a label.
 `cperl-min-label-indent'
    Minimal indentation for line that is a label.

Settings for K&R and BSD indentation styles are
  `cperl-indent-level'                5    8
  `cperl-continued-statement-offset'  5    8
  `cperl-brace-offset'               -5   -8
  `cperl-label-offset'               -5   -8

If `cperl-indent-level' is 0, the statement after opening brace in column 0 is indented on `cperl-brace-offset'+`cperl-continued-statement-offset'.

Turning on CPerl mode calls the hooks in the variable `cperl-mode-hook'
with no args."
  (interactive)
  (kill-all-local-variables)
  ;;(if cperl-hairy
  ;;    (progn
  ;;	(cperl-set 'cperl-font-lock cperl-hairy)
  ;;	(cperl-set 'cperl-electric-lbrace-space cperl-hairy)
  ;;	(cperl-set 'cperl-electric-parens "{[(<")
  ;;	(cperl-set 'cperl-electric-keywords cperl-hairy)
  ;;	(cperl-set 'cperl-electric-linefeed cperl-hairy)))
  (use-local-map cperl-mode-map)
  (if (cperl-val 'cperl-electric-linefeed)
      (progn
	(local-set-key "\C-J" 'cperl-linefeed)
	(local-set-key "\C-C\C-J" 'newline-and-indent)))
  (if (cperl-val 'cperl-info-on-command-no-prompt)
      (progn
	(if (cperl-xemacs-p)
	    ;; don't clobber the backspace binding:
	    (local-set-key [(control h) f] 'cperl-info-on-current-command)
	  (local-set-key "\C-hf" 'cperl-info-on-current-command))
	(if (cperl-xemacs-p)
	    ;; don't clobber the backspace binding:
	    (local-set-key [(control c) (control h) f]
			   'cperl-info-on-command)
	  (local-set-key "\C-c\C-hf" 'cperl-info-on-command))))
  (setq major-mode 'perl-mode)
  (setq mode-name "CPerl")
  (if (not cperl-mode-abbrev-table)
      (let ((prev-a-c abbrevs-changed))
	(define-abbrev-table 'cperl-mode-abbrev-table '(
		("if" "if" cperl-electric-keyword 0)
		("elsif" "elsif" cperl-electric-keyword 0)
		("while" "while" cperl-electric-keyword 0)
		("until" "until" cperl-electric-keyword 0)
		("unless" "unless" cperl-electric-keyword 0)
		("else" "else" cperl-electric-else 0)
		("for" "for" cperl-electric-keyword 0)
		("foreach" "foreach" cperl-electric-keyword 0)
		("do" "do" cperl-electric-keyword 0)))
	(setq abbrevs-changed prev-a-c)))
  (setq local-abbrev-table cperl-mode-abbrev-table)
  (abbrev-mode (if (cperl-val 'cperl-electric-keywords) 1 0))
  (set-syntax-table cperl-mode-syntax-table)
  (make-local-variable 'paragraph-start)
  (setq paragraph-start (concat "^$\\|" page-delimiter))
  (make-local-variable 'paragraph-separate)
  (setq paragraph-separate paragraph-start)
  (make-local-variable 'paragraph-ignore-fill-prefix)
  (setq paragraph-ignore-fill-prefix t)
  (make-local-variable 'indent-line-function)
  (setq indent-line-function 'cperl-indent-line)
  (make-local-variable 'require-final-newline)
  (setq require-final-newline t)
  (make-local-variable 'comment-start)
  (setq comment-start "# ")
  (make-local-variable 'comment-end)
  (setq comment-end "")
  (make-local-variable 'comment-column)
  (setq comment-column cperl-comment-column)
  (make-local-variable 'comment-start-skip)
  (setq comment-start-skip "#+ *")
  (make-local-variable 'defun-prompt-regexp)
  (setq defun-prompt-regexp "[ \t]*sub\\s +\\([^ \t\n{;]+\\)\\s *")
  (make-local-variable 'comment-indent-function)
  (setq comment-indent-function 'cperl-comment-indent)
  (make-local-variable 'parse-sexp-ignore-comments)
  (setq parse-sexp-ignore-comments t)
  (make-local-variable 'indent-region-function)
  (setq indent-region-function 'cperl-indent-region)
  ;;(setq auto-fill-function 'cperl-do-auto-fill) ; Need to switch on and off!
  (make-local-variable 'imenu-create-index-function)
  (setq imenu-create-index-function
	(function imenu-example--create-perl-index))
  (make-local-variable 'imenu-sort-function)
  (setq imenu-sort-function nil)
  (make-local-variable 'vc-header-alist)
  (setq vc-header-alist cperl-vc-header-alist)
  (make-local-variable 'font-lock-defaults)
  (setq	font-lock-defaults
	(if (string< emacs-version "19.30")
	    '(perl-font-lock-keywords-2)
	  '((perl-font-lock-keywords
	     perl-font-lock-keywords-1
	     perl-font-lock-keywords-2))))
  (or (fboundp 'cperl-old-auto-fill-mode)
      (progn
	(fset 'cperl-old-auto-fill-mode (symbol-function 'auto-fill-mode))
	(defun auto-fill-mode (&optional arg)
	  (interactive "P")
	  (cperl-old-auto-fill-mode arg)
	  (and auto-fill-function (eq major-mode 'perl-mode)
	       (setq auto-fill-function 'cperl-do-auto-fill)))))
  (if (cperl-enable-font-lock)
      (if (cperl-val 'cperl-font-lock) 
	  (progn (or cperl-faces-init (cperl-init-faces))
		 (font-lock-mode 1))))
  (and (boundp 'msb-menu-cond)
       (not cperl-msb-fixed)
       (cperl-msb-fix))
  (run-hooks 'cperl-mode-hook)
  ;; After hooks since fontification will break this
  (if cperl-pod-here-scan (cperl-find-pods-heres)))

;; Fix for msb.el
(defvar cperl-msb-fixed nil)

(defun cperl-msb-fix ()
  ;; Adds perl files to msb menu, supposes that msb is already loaded
  (setq cperl-msb-fixed t)
  (let* ((l (length msb-menu-cond))
	 (last (nth (1- l) msb-menu-cond))
	 (precdr (nthcdr (- l 2) msb-menu-cond)) ; cdr of this is last
	 (handle (1- (nth 1 last))))
    (setcdr precdr (list
		    (list
		     '(eq major-mode 'perl-mode)
		     handle
		     "Perl Files (%d)")
		    last))))

;; This is used by indent-for-comment
;; to decide how much to indent a comment in CPerl code
;; based on its context. Do fallback if comment is found wrong.

(defvar cperl-wrong-comment)

(defun cperl-comment-indent ()
  (let ((p (point)) (c (current-column)) was)
    (if (looking-at "^#") 0		; Existing comment at bol stays there.
      ;; Wrong comment found
      (save-excursion
	(setq was (cperl-to-comment-or-eol))
	(if (= (point) p)
	    (progn
	      (skip-chars-backward " \t")
	      (max (1+ (current-column)) ; Else indent at comment column
		   comment-column))
	  (if was nil
	    (insert comment-start)
	    (backward-char (length comment-start)))
	  (setq cperl-wrong-comment t)
	  (indent-to comment-column 1)	; Indent minimum 1
	  c)))))			; except leave at least one space.

;;;(defun cperl-comment-indent-fallback ()
;;;  "Is called if the standard comment-search procedure fails.
;;;Point is at start of real comment."
;;;  (let ((c (current-column)) target cnt prevc)
;;;    (if (= c comment-column) nil
;;;      (setq cnt (skip-chars-backward "[ \t]"))
;;;      (setq target (max (1+ (setq prevc 
;;;			     (current-column))) ; Else indent at comment column
;;;		   comment-column))
;;;      (if (= c comment-column) nil
;;;	(delete-backward-char cnt)
;;;	(while (< prevc target)
;;;	  (insert "\t")
;;;	  (setq prevc (current-column)))
;;;	(if (> prevc target) (progn (delete-char -1) (setq prevc (current-column))))
;;;	(while (< prevc target)
;;;	  (insert " ")
;;;	  (setq prevc (current-column)))))))

(defun cperl-indent-for-comment ()
  "Substite for `indent-for-comment' in CPerl."
  (interactive)
  (let (cperl-wrong-comment)
    (indent-for-comment)
    (if cperl-wrong-comment
	(progn (cperl-to-comment-or-eol)
	       (forward-char (length comment-start))))))

(defun cperl-electric-brace (arg &optional only-before)
  "Insert character and correct line's indentation.
If ONLY-BEFORE and `cperl-auto-newline', will insert newline before the
place (even in empty line), but not after."
  (interactive "P")
  (let (insertpos)
    (if (and (not arg)			; No args, end (of empty line or auto)
	     (eolp)
	     (or (and (null only-before)
		      (save-excursion
			(skip-chars-backward " \t")
			(bolp)))
		 (if cperl-auto-newline 
		     (progn (cperl-indent-line) (newline) t) nil)))
	(progn
	  (if cperl-auto-newline
	      (setq insertpos (point)))
	  (insert last-command-char)
	  (cperl-indent-line)
	  (if (and cperl-auto-newline (null only-before))
	      (progn
		(newline)
		(cperl-indent-line)))
	  (save-excursion
	    (if insertpos (progn (goto-char insertpos)
				 (search-forward (make-string 
						  1 last-command-char))
				 (setq insertpos (1- (point)))))
	    (delete-char -1))))
    (if insertpos
	(save-excursion
	  (goto-char insertpos)
	  (self-insert-command (prefix-numeric-value arg)))
      (self-insert-command (prefix-numeric-value arg)))))

(defun cperl-electric-lbrace (arg)
  "Insert character, correct line's indentation, correct quoting by space."
  (interactive "P")
  (let (pos after (cperl-auto-newline cperl-auto-newline))
    (and (cperl-val 'cperl-electric-lbrace-space)
	 (eq (preceding-char) ?$)
	 (save-excursion
	   (skip-chars-backward "$")
	   (looking-at "\\(\\$\\$\\)*\\$\\([^\\$]\\|$\\)"))
	 (insert ? ))
    (if (cperl-after-expr-p) nil (setq cperl-auto-newline nil))
    (cperl-electric-brace arg)
    (and (eq last-command-char ?{)
	 (memq last-command-char 
	       (append (cperl-val 'cperl-electric-parens "" "([{<") nil))
	 (setq last-command-char ?} pos (point))
	 (progn (cperl-electric-brace arg t)
		(goto-char pos)))))

(defun cperl-electric-paren (arg)
  "Insert a matching pair of parentheses."
  (interactive "P")
  (let ((beg (save-excursion (beginning-of-line) (point))))
    (if (and (memq last-command-char
		   (append (cperl-val 'cperl-electric-parens "" "([{<") nil))
	     (>= (save-excursion (cperl-to-comment-or-eol) (point)) (point))
	     ;;(not (save-excursion (search-backward "#" beg t)))
	     (if (eq last-command-char ?<)
		 (cperl-after-expr-p nil "{};(,:=")
	       1))
	(progn
	  (insert last-command-char)
	  (insert (cdr (assoc last-command-char '((?{ .?})
						  (?[ . ?])
						  (?( . ?))
						  (?< . ?>)))))
	  (forward-char -1))
      (insert last-command-char)
      )))

(defun cperl-electric-keyword ()
  "Insert a construction appropriate after a keyword."
  (let ((beg (save-excursion (beginning-of-line) (point))))
    (and (save-excursion
	   (backward-sexp 1)
	   (cperl-after-expr-p nil "{};:"))
	 (save-excursion 
	   (not 
	    (re-search-backward
	     "[#\"'`]\\|\\<q\\(\\|[wqx]\\)\\>"
	     beg t)))
	 (save-excursion (or (not (re-search-backward "^=" nil t))
			     (looking-at "=cut")))
	 (progn
	   (cperl-indent-line)
	   ;;(insert " () {\n}")
 	   (cond
 	    (cperl-extra-newline-before-brace
 	     (insert " ()\n")
 	     (insert "{")
 	     (cperl-indent-line)
 	     (insert "\n")
 	     (cperl-indent-line)
 	     (insert "\n}"))
 	    (t
 	     (insert " () {\n}"))
 	    )
	   (or (looking-at "[ \t]\\|$") (insert " "))
	   (cperl-indent-line)
	   (search-backward ")")
	   (cperl-putback-char del-back-ch)))))

(defun cperl-electric-else ()
  "Insert a construction appropriate after a keyword."
  (let ((beg (save-excursion (beginning-of-line) (point))))
    (and (save-excursion
	   (backward-sexp 1)
	   (cperl-after-expr-p nil "{};:"))
	 (save-excursion 
	   (not 
	    (re-search-backward
	     "[#\"'`]\\|\\<q\\(\\|[wqx]\\)\\>"
	     beg t)))
	 (save-excursion (or (not (re-search-backward "^=" nil t))
			     (looking-at "=cut")))
	 (progn
	   (cperl-indent-line)
	   ;;(insert " {\n\n}")
 	   (cond
 	    (cperl-extra-newline-before-brace
 	     (insert "\n")
 	     (insert "{")
 	     (cperl-indent-line)
 	     (insert "\n\n}"))
 	    (t
 	     (insert " {\n\n}"))
 	    )
	   (or (looking-at "[ \t]\\|$") (insert " "))
	   (cperl-indent-line)
	   (forward-line -1)
	   (cperl-indent-line)
	   (cperl-putback-char del-back-ch)))))

(defun cperl-linefeed ()
  "Go to end of line, open a new line and indent appropriately."
  (interactive)
  (let ((beg (save-excursion (beginning-of-line) (point)))
	(end (save-excursion (end-of-line) (point)))
	(pos (point)) start)
    (if (and				; Check if we need to split:
					; i.e., on a boundary and inside "{...}" 
	 ;;(not (search-backward "\\(^\\|[^$\\\\]\\)#" beg t))
	 (save-excursion (cperl-to-comment-or-eol)
	   (>= (point) pos))
	 (or (save-excursion
	       (skip-chars-backward " \t" beg)
	       (forward-char -1)
	       (looking-at "[;{]"))
	     (looking-at "[ \t]*}")
	     (re-search-forward "\\=[ \t]*;" end t))
	 (save-excursion
	   (and
	    (eq (car (parse-partial-sexp pos end -1)) -1)
	    (looking-at "[ \t]*\\($\\|#\\)")
	    ;;(setq finish (point-marker))
	    (progn
	      (backward-sexp 1)
	      (setq start (point-marker))
	      (<= start pos))
	    ;;(looking-at "[^{}\n]*}[ \t]*$") ; Will fail if there are intervening {}'s
	    ;;(search-backward "{" beg t)
	    ;;(looking-at "{[^{}\n]*}[ \t]*$")
	    )))
	 ;;(or (looking-at "[ \t]*}")	; and on a boundary of statements
	 ;;    (save-excursion
	 ;;      (skip-chars-backward " \t")
	 ;;      (forward-char -1)
	 ;;      (looking-at "[{;]"))))
	(progn
	  (skip-chars-backward " \t")
	  (or (memq (preceding-char) (append ";{" nil))
	      (insert ";"))
	  (insert "\n")
	  (forward-line -1)
	  (cperl-indent-line)
	  ;;(end-of-line)
	  ;;(search-backward "{" beg)
	  (goto-char start)
	  (or (looking-at "{[ \t]*$")	; If there is a statement
					; before, move it to separate line
	      (progn
		(forward-char 1)
		(insert "\n")
		(cperl-indent-line)))
	  (forward-line 1)		; We are on the target line
	  (cperl-indent-line)
	  (beginning-of-line)
	  (or (looking-at "[ \t]*}[ \t]*$") ; If there is a statement
					    ; after, move it to separate line
	      (progn
		(end-of-line)
		(search-backward "}" beg)
		(skip-chars-backward " \t")
		(or (memq (preceding-char) (append ";{" nil))
		    (insert ";"))
		(insert "\n")
		(cperl-indent-line)
		(forward-line -1)))
	  (forward-line -1)		; We are on the line before target 
	  (end-of-line)
	  (newline-and-indent))
      (end-of-line)			; else
      (if (not (looking-at "\n[ \t]*$"))
	  (newline-and-indent)
	(forward-line 1)
	(cperl-indent-line)))))

(defun cperl-electric-semi (arg)
  "Insert character and correct line's indentation."
  (interactive "P")
  (if cperl-auto-newline
      (cperl-electric-terminator arg)
    (self-insert-command (prefix-numeric-value arg))))

(defun cperl-electric-terminator (arg)
  "Insert character and correct line's indentation."
  (interactive "P")
  (let (insertpos (end (point)))
    (if (and (not arg) (eolp)
	     (not (save-excursion
		    (beginning-of-line)
		    (skip-chars-forward " \t")
		    (or
		     ;; Ignore in comment lines
		     (= (following-char) ?#)
		     ;; Colon is special only after a label
		     ;; So quickly rule out most other uses of colon
		     ;; and do no indentation for them.
		     (and (eq last-command-char ?:)
			  (save-excursion
			    (forward-word 1)
			    (skip-chars-forward " \t")
			    (and (< (point) end)
				 (progn (goto-char (- end 1))
					(not (looking-at ":"))))))
		     (progn
		       (beginning-of-defun)
		       (let ((pps (parse-partial-sexp (point) end)))
			 (or (nth 3 pps) (nth 4 pps) (nth 5 pps))))))))
	(progn
	  (if cperl-auto-newline
	      (setq insertpos (point)))
	  (insert last-command-char)
	  (cperl-indent-line)
	  (if cperl-auto-newline
	      (progn
		(newline)
		(cperl-indent-line)))
	  (save-excursion
	    (if insertpos (progn (goto-char insertpos)
				 (search-forward (make-string 
						  1 last-command-char))
				 (setq insertpos (1- (point)))))
	    (delete-char -1))))
    (if insertpos
	(save-excursion
	  (goto-char insertpos)
	  (self-insert-command (prefix-numeric-value arg)))
      (self-insert-command (prefix-numeric-value arg)))))

(defun cperl-inside-parens-p ()
  (condition-case ()
      (save-excursion
	(save-restriction
	  (narrow-to-region (point)
			    (progn (beginning-of-defun) (point)))
	  (goto-char (point-max))
	  (= (char-after (or (scan-lists (point) -1 1) (point-min))) ?\()))
    (error nil)))

(defun cperl-indent-command (&optional whole-exp)
  (interactive "P")
  "Indent current line as Perl code, or in some cases insert a tab character.
If `cperl-tab-always-indent' is non-nil (the default), always indent current line.
Otherwise, indent the current line only if point is at the left margin
or in the line's indentation; otherwise insert a tab.

A numeric argument, regardless of its value,
means indent rigidly all the lines of the expression starting after point
so that this line becomes properly indented.
The relative indentation among the lines of the expression are preserved."
  (if whole-exp
      ;; If arg, always indent this line as Perl
      ;; and shift remaining lines of expression the same amount.
      (let ((shift-amt (cperl-indent-line))
	    beg end)
	(save-excursion
	  (if cperl-tab-always-indent
	      (beginning-of-line))
	  (setq beg (point))
	  (forward-sexp 1)
	  (setq end (point))
	  (goto-char beg)
	  (forward-line 1)
	  (setq beg (point)))
	(if (> end beg)
	    (indent-code-rigidly beg end shift-amt "#")))
    (if (and (not cperl-tab-always-indent)
	     (save-excursion
	       (skip-chars-backward " \t")
	       (not (bolp))))
	(insert-tab)
      (cperl-indent-line))))

(defun cperl-indent-line (&optional symbol)
  "Indent current line as Perl code.
Return the amount the indentation changed by."
  (let (indent
	beg shift-amt
	(case-fold-search nil)
	(pos (- (point-max) (point))))
    (setq indent (cperl-calculate-indent nil symbol))
    (beginning-of-line)
    (setq beg (point))
    (cond ((eq indent nil)
	   (setq indent (current-indentation)))
	  ;;((eq indent t)    ; Never?
	  ;; (setq indent (cperl-calculate-indent-within-comment)))
	  ;;((looking-at "[ \t]*#")
	  ;; (setq indent 0))
	  (t
	   (skip-chars-forward " \t")
	   (if (listp indent) (setq indent (car indent)))
	   (cond ((looking-at "[A-Za-z]+:[^:]")
		  (and (> indent 0)
		       (setq indent (max cperl-min-label-indent
					 (+ indent cperl-label-offset)))))
		 ((= (following-char) ?})
		  (setq indent (- indent cperl-indent-level)))
		 ((memq (following-char) '(?\) ?\])) ; To line up with opening paren.
		  (setq indent (+ indent cperl-close-paren-offset)))
		 ((= (following-char) ?{)
		  (setq indent (+ indent cperl-brace-offset))))))
    (skip-chars-forward " \t")
    (setq shift-amt (- indent (current-column)))
    (if (zerop shift-amt)
	(if (> (- (point-max) pos) (point))
	    (goto-char (- (point-max) pos)))
      (delete-region beg (point))
      (indent-to indent)
      ;; If initial point was within line's indentation,
      ;; position after the indentation.  Else stay at same point in text.
      (if (> (- (point-max) pos) (point))
	  (goto-char (- (point-max) pos))))
    shift-amt))

(defun cperl-after-label ()
  ;; Returns true if the point is after label. Does not do save-excursion.
  (and (eq (preceding-char) ?:)
       (memq (char-syntax (char-after (- (point) 2)))
	     '(?w ?_))
       (progn
	 (backward-sexp)
	 (looking-at "[a-zA-Z_][a-zA-Z0-9_]*:"))))

(defun cperl-get-state (&optional parse-start start-state)
  ;; returns list (START STATE DEPTH PRESTART), START is a good place
  ;; to start parsing, STATE is what is returned by
  ;; `parse-partial-sexp'. DEPTH is true is we are immediately after
  ;; end of block which contains START. PRESTART is the position
  ;; basing on which START was found.
  (save-excursion
    (let ((start-point (point)) depth state start prestart)
      (if parse-start
	  (goto-char parse-start)
	(beginning-of-defun))
      (setq prestart (point))
      (if start-state nil
	;; Try to go out, if sub is not on the outermost level
	(while (< (point) start-point)
	  (setq start (point) parse-start start depth nil
		state (parse-partial-sexp start start-point -1))
	  (if (> (car state) -1) nil
	    ;; The current line could start like }}}, so the indentation
	    ;; corresponds to a different level than what we reached
	    (setq depth t)
	    (beginning-of-line 2)))	; Go to the next line.
	(if start (goto-char start)))	; Not at the start of file
      (setq start (point))
      (if (< start start-point) (setq parse-start start))
      (or state (setq state (parse-partial-sexp start start-point -1 nil start-state)))
      (list start state depth prestart))))

(defun cperl-block-p ()			; Do not C-M-q ! One string contains ";" !
  ;; Positions is before ?\{. Checks whether it starts a block.
  ;; No save-excursion!
  (cperl-backward-to-noncomment (point-min))
  ;;(skip-chars-backward " \t\n\f")
  (or (memq (preceding-char) (append ";){}$@&%\C-@" nil)) ; Or label! \C-@ at bobp
					; Label may be mixed up with `$blah :'
      (save-excursion (cperl-after-label))
      (and (eq (char-syntax (preceding-char)) ?w)
	   (progn
	     (backward-sexp)
	     (or (looking-at "\\sw+[ \t\n\f]*[{#]") ; Method call syntax
		 (progn
		   (skip-chars-backward " \t\n\f")
		   (and (eq (char-syntax (preceding-char)) ?w)
			(progn
			  (backward-sexp)
			  (looking-at 
			   "sub[ \t]+\\sw+[ \t\n\f]*[#{]")))))))))

(defun cperl-calculate-indent (&optional parse-start symbol)
  "Return appropriate indentation for current line as Perl code.
In usual case returns an integer: the column to indent to.
Returns nil if line starts inside a string, t if in a comment."
  (save-excursion
    (if (memq (get-text-property (point) 'syntax-type) '(pod here-doc)) nil
      (beginning-of-line)
      (let* ((indent-point (point))
	     (case-fold-search nil)
	     (s-s (cperl-get-state))
	     (start (nth 0 s-s))
	     (state (nth 1 s-s))
	     (containing-sexp (car (cdr state)))
	     (char-after (save-excursion
			   (skip-chars-forward " \t")
			   (following-char)))
	     (start-indent (save-excursion
			     (goto-char start)
			     (- (current-indentation)
				(if (nth 2 s-s) cperl-indent-level 0))))
	     old-indent)
	;;      (or parse-start (null symbol)
	;;	  (setq parse-start (symbol-value symbol) 
	;;		start-indent (nth 2 parse-start) 
	;;		parse-start (car parse-start)))
	;;      (if parse-start
	;;	  (goto-char parse-start)
	;;	(beginning-of-defun))
	;;      ;; Try to go out
	;;      (while (< (point) indent-point)
	;;	(setq start (point) parse-start start moved nil
	;;	      state (parse-partial-sexp start indent-point -1))
	;;	(if (> (car state) -1) nil
	;;	  ;; The current line could start like }}}, so the indentation
	;;	  ;; corresponds to a different level than what we reached
	;;	  (setq moved t)
	;;	  (beginning-of-line 2)))	; Go to the next line.
	;;      (if start				; Not at the start of file
	;;	  (progn
	;;	    (goto-char start)
	;;	    (setq start-indent (current-indentation))
	;;	    (if moved			; Should correct...
	;;		(setq start-indent (- start-indent cperl-indent-level))))
	;;	(setq start-indent 0))
	;;      (if (< (point) indent-point) (setq parse-start (point)))
	;;      (or state (setq state (parse-partial-sexp 
	;;			     (point) indent-point -1 nil start-state)))
	;;      (setq containing-sexp 
	;;	    (or (car (cdr state)) 
	;;		(and (>= (nth 6 state) 0) old-containing-sexp))
	;;	    old-containing-sexp nil start-state nil)
;;;;      (while (< (point) indent-point)
;;;;	(setq parse-start (point))
;;;;	(setq state (parse-partial-sexp (point) indent-point -1 nil start-state))
;;;;	(setq containing-sexp 
;;;;	      (or (car (cdr state)) 
;;;;		  (and (>= (nth 6 state) 0) old-containing-sexp))
;;;;	      old-containing-sexp nil start-state nil))
	;;      (if symbol (set symbol (list indent-point state start-indent)))
	;;      (goto-char indent-point)
	(cond ((or (nth 3 state) (nth 4 state))
	       ;; return nil or t if should not change this line
	       (nth 4 state))
	      ((null containing-sexp)
	       ;; Line is at top level.  May be data or function definition,
	       ;; or may be function argument declaration.
	       ;; Indent like the previous top level line
	       ;; unless that ends in a closeparen without semicolon,
	       ;; in which case this line is the first argument decl.
	       (skip-chars-forward " \t")
	       (+ start-indent
		  (if (= (following-char) ?{) cperl-continued-brace-offset 0)
		  (progn
		    (cperl-backward-to-noncomment (or parse-start (point-min)))
		    ;;(skip-chars-backward " \t\f\n")
		    ;; Look at previous line that's at column 0
		    ;; to determine whether we are in top-level decls
		    ;; or function's arg decls.  Set basic-indent accordingly.
		    ;; Now add a little if this is a continuation line.
		    (if (or (bobp)
			    (memq (preceding-char) (append " ;}" nil)) ; Was ?\)
			    (memq char-after (append ")]}" nil))) 
			0
		      cperl-continued-statement-offset))))
	      ((/= (char-after containing-sexp) ?{)
	       ;; line is expression, not statement:
	       ;; indent to just after the surrounding open,
	       ;; skip blanks if we do not close the expression.
	       (goto-char (1+ containing-sexp))
	       (or (memq char-after (append ")]}" nil))
		   (looking-at "[ \t]*\\(#\\|$\\)")
		   (skip-chars-forward " \t"))
	       (current-column))
	      ((progn
		 ;; Containing-expr starts with \{. Check whether it is a hash.
		 (goto-char containing-sexp)
		 (not (cperl-block-p)))
	       (goto-char (1+ containing-sexp))
	       (or (eq char-after ?\})
		   (looking-at "[ \t]*\\(#\\|$\\)")
		   (skip-chars-forward " \t"))
	       (+ (current-column)	; Correct indentation of trailing ?\}
		  (if (eq char-after ?\}) (+ cperl-indent-level
					     cperl-close-paren-offset) 
		    0)))
	      (t
	       ;; Statement level.  Is it a continuation or a new statement?
	       ;; Find previous non-comment character.
	       (goto-char indent-point)
	       (cperl-backward-to-noncomment containing-sexp)
	       ;; Back up over label lines, since they don't
	       ;; affect whether our line is a continuation.
	       (while (or (eq (preceding-char) ?\,)
			  (and (eq (preceding-char) ?:)
			       (or;;(eq (char-after (- (point) 2)) ?\') ; ????
				(memq (char-syntax (char-after (- (point) 2)))
				      '(?w ?_)))))
		 (if (eq (preceding-char) ?\,)
		     ;; Will go to beginning of line, essentially.
		     ;; Will ignore embedded sexpr XXXX.
		     (cperl-backward-to-start-of-continued-exp containing-sexp))
		 (beginning-of-line)
		 (cperl-backward-to-noncomment containing-sexp))
	       ;; Now we get the answer.
	       (if (not (memq (preceding-char) (append ", ;}{" '(nil)))) ; Was ?\,
		   ;; This line is continuation of preceding line's statement;
		   ;; indent  `cperl-continued-statement-offset'  more than the
		   ;; previous line of the statement.
		   (progn
		     (cperl-backward-to-start-of-continued-exp containing-sexp)
		     (+ (if (memq char-after (append "}])" nil))
			    0		; Closing parenth
			  cperl-continued-statement-offset)
			(current-column)
			(if (eq char-after ?\{)
			    cperl-continued-brace-offset 0)))
		 ;; This line starts a new statement.
		 ;; Position following last unclosed open.
		 (goto-char containing-sexp)
		 ;; Is line first statement after an open-brace?
		 (or
		  ;; If no, find that first statement and indent like
		  ;; it.  If the first statement begins with label, do
		  ;; not belive when the indentation of the label is too
		  ;; small.
		  (save-excursion
		    (forward-char 1)
		    (setq old-indent (current-indentation))
		    (let ((colon-line-end 0))
		      (while (progn (skip-chars-forward " \t\n")
				    (looking-at "#\\|[a-zA-Z0-9_$]*:[^:]"))
			;; Skip over comments and labels following openbrace.
			(cond ((= (following-char) ?\#)
			       (forward-line 1))
			      ;; label:
			      (t
			       (save-excursion (end-of-line)
					       (setq colon-line-end (point)))
			       (search-forward ":"))))
		      ;; The first following code counts
		      ;; if it is before the line we want to indent.
		      (and (< (point) indent-point)
			   (if (> colon-line-end (point)) ; After label
			       (if (> (current-indentation) 
				      cperl-min-label-indent)
				   (- (current-indentation) cperl-label-offset)
				 ;; Do not belive: `max' is involved
				 (+ old-indent cperl-indent-level))
			     (current-column)))))
		  ;; If no previous statement,
		  ;; indent it relative to line brace is on.
		  ;; For open brace in column zero, don't let statement
		  ;; start there too.  If cperl-indent-level is zero,
		  ;; use cperl-brace-offset + cperl-continued-statement-offset instead.
		  ;; For open-braces not the first thing in a line,
		  ;; add in cperl-brace-imaginary-offset.

		  ;; If first thing on a line:  ?????
		  (+ (if (and (bolp) (zerop cperl-indent-level))
			 (+ cperl-brace-offset cperl-continued-statement-offset)
		       cperl-indent-level)
		     ;; Move back over whitespace before the openbrace.
		     ;; If openbrace is not first nonwhite thing on the line,
		     ;; add the cperl-brace-imaginary-offset.
		     (progn (skip-chars-backward " \t")
			    (if (bolp) 0 cperl-brace-imaginary-offset))
		     ;; If the openbrace is preceded by a parenthesized exp,
		     ;; move to the beginning of that;
		     ;; possibly a different line
		     (progn
		       (if (eq (preceding-char) ?\))
			   (forward-sexp -1))
		       ;; Get initial indentation of the line we are on.
		       ;; If line starts with label, calculate label indentation
		       (if (save-excursion
			     (beginning-of-line)
			     (looking-at "[ \t]*[a-zA-Z_][a-zA-Z_]*:[^:]"))
			   (if (> (current-indentation) cperl-min-label-indent)
			       (- (current-indentation) cperl-label-offset)
			     (cperl-calculate-indent 
			      (if (and parse-start (<= parse-start (point)))
				  parse-start)))
			 (current-indentation))))))))))))

(defvar cperl-indent-alist
  '((string nil)
    (comment nil)
    (toplevel 0)
    (toplevel-after-parenth 2)
    (toplevel-continued 2)
    (expression 1))
  "Alist of indentation rules for CPerl mode.
The values mean:
  nil: do not indent;
  number: add this amount of indentation.")

(defun cperl-where-am-i (&optional parse-start start-state)
  ;; Unfinished
  "Return a list of lists ((TYPE POS)...) of good points before the point.
POS may be nil if it is hard to find, say, when TYPE is `string' or `comment'."
  (save-excursion
    (let* ((start-point (point))
	   (s-s (cperl-get-state))
	   (start (nth 0 s-s))
	   (state (nth 1 s-s))
	   (prestart (nth 3 s-s))
	   (containing-sexp (car (cdr state)))
	   (case-fold-search nil)
	   (res (list (list 'parse-start start) (list 'parse-prestart prestart))))
      (cond ((nth 3 state)		; In string
	     (setq res (cons (list 'string nil (nth 3 state)) res))) ; What started string
	    ((nth 4 state)		; In comment
	     (setq res (cons '(comment) res)))
	    ((null containing-sexp)
	     ;; Line is at top level.  
	     ;; Indent like the previous top level line
	     ;; unless that ends in a closeparen without semicolon,
	     ;; in which case this line is the first argument decl.
	     (cperl-backward-to-noncomment (or parse-start (point-min)))
	     ;;(skip-chars-backward " \t\f\n")
	     (cond
	      ((or (bobp)
		   (memq (preceding-char) (append ";}" nil)))
	       (setq res (cons (list 'toplevel start) res)))
	      ((eq (preceding-char) ?\) )
	       (setq res (cons (list 'toplevel-after-parenth start) res)))
	      (t 
	       (setq res (cons (list 'toplevel-continued start) res)))))
	    ((/= (char-after containing-sexp) ?{)
	     ;; line is expression, not statement:
	     ;; indent to just after the surrounding open.
	     ;; skip blanks if we do not close the expression.
	     (setq res (cons (list 'expression-blanks
				   (progn
				     (goto-char (1+ containing-sexp))
				     (or (looking-at "[ \t]*\\(#\\|$\\)")
					 (skip-chars-forward " \t"))
				     (point)))
			     (cons (list 'expression containing-sexp) res))))
	    ((progn
	      ;; Containing-expr starts with \{. Check whether it is a hash.
	      (goto-char containing-sexp)
	      (not (cperl-block-p)))
	     (setq res (cons (list 'expression-blanks
				   (progn
				     (goto-char (1+ containing-sexp))
				     (or (looking-at "[ \t]*\\(#\\|$\\)")
					 (skip-chars-forward " \t"))
				     (point)))
			     (cons (list 'expression containing-sexp) res))))
	    (t
	     ;; Statement level.
	     (setq res (cons (list 'in-block containing-sexp) res))
	     ;; Is it a continuation or a new statement?
	     ;; Find previous non-comment character.
	     (cperl-backward-to-noncomment containing-sexp)
	     ;; Back up over label lines, since they don't
	     ;; affect whether our line is a continuation.
	     ;; Back up comma-delimited lines too ?????
	     (while (or (eq (preceding-char) ?\,)
			(save-excursion (cperl-after-label)))
	       (if (eq (preceding-char) ?\,)
		   ;; Will go to beginning of line, essentially
		     ;; Will ignore embedded sexpr XXXX.
		   (cperl-backward-to-start-of-continued-exp containing-sexp))
	       (beginning-of-line)
	       (cperl-backward-to-noncomment containing-sexp))
	     ;; Now we get the answer.
	     (if (not (memq (preceding-char) (append ";}{" '(nil)))) ; Was ?\,
		 ;; This line is continuation of preceding line's statement.
		 (list (list 'statement-continued containing-sexp))
	       ;; This line starts a new statement.
	       ;; Position following last unclosed open.
	       (goto-char containing-sexp)
	       ;; Is line first statement after an open-brace?
	       (or
		;; If no, find that first statement and indent like
		;; it.  If the first statement begins with label, do
		;; not belive when the indentation of the label is too
		;; small.
		(save-excursion
		  (forward-char 1)
		  (let ((colon-line-end 0))
		    (while (progn (skip-chars-forward " \t\n" start-point)
				  (and (< (point) start-point)
				       (looking-at
					"#\\|[a-zA-Z_][a-zA-Z0-9_]*:[^:]")))
		      ;; Skip over comments and labels following openbrace.
		      (cond ((= (following-char) ?\#)
			     ;;(forward-line 1)
			     (end-of-line))
			    ;; label:
			    (t
			     (save-excursion (end-of-line)
					     (setq colon-line-end (point)))
			     (search-forward ":"))))
		    ;; Now at the point, after label, or at start 
		    ;; of first statement in the block.
		    (and (< (point) start-point)
			 (if (> colon-line-end (point)) 
			     ;; Before statement after label
			     (if (> (current-indentation) 
				    cperl-min-label-indent)
				 (list (list 'label-in-block (point)))
			       ;; Do not belive: `max' is involved
			       (list
				(list 'label-in-block-min-indent (point))))
			   ;; Before statement
			   (list 'statement-in-block (point))))))
		;; If no previous statement,
		;; indent it relative to line brace is on.
		;; For open brace in column zero, don't let statement
		;; start there too.  If cperl-indent-level is zero,
		;; use cperl-brace-offset + cperl-continued-statement-offset instead.
		;; For open-braces not the first thing in a line,
		;; add in cperl-brace-imaginary-offset.

		;; If first thing on a line:  ?????
		(+ (if (and (bolp) (zerop cperl-indent-level))
		       (+ cperl-brace-offset cperl-continued-statement-offset)
		     cperl-indent-level)
		   ;; Move back over whitespace before the openbrace.
		   ;; If openbrace is not first nonwhite thing on the line,
		   ;; add the cperl-brace-imaginary-offset.
		   (progn (skip-chars-backward " \t")
			  (if (bolp) 0 cperl-brace-imaginary-offset))
		   ;; If the openbrace is preceded by a parenthesized exp,
		   ;; move to the beginning of that;
		   ;; possibly a different line
		   (progn
		     (if (eq (preceding-char) ?\))
			 (forward-sexp -1))
		     ;; Get initial indentation of the line we are on.
		     ;; If line starts with label, calculate label indentation
		     (if (save-excursion
			   (beginning-of-line)
			   (looking-at "[ \t]*[a-zA-Z_][a-zA-Z_]*:[^:]"))
			 (if (> (current-indentation) cperl-min-label-indent)
			     (- (current-indentation) cperl-label-offset)
			   (cperl-calculate-indent 
			    (if (and parse-start (<= parse-start (point)))
				parse-start)))
		       (current-indentation))))))))
      res)))

(defun cperl-calculate-indent-within-comment ()
  "Return the indentation amount for line, assuming that
the current line is to be regarded as part of a block comment."
  (let (end star-start)
    (save-excursion
      (beginning-of-line)
      (skip-chars-forward " \t")
      (setq end (point))
      (and (= (following-char) ?#)
	   (forward-line -1)
	   (cperl-to-comment-or-eol)
	   (setq end (point)))
      (goto-char end)
      (current-column))))


(defun cperl-to-comment-or-eol ()
  "Goes to position before comment on the current line, or to end of line.
Returns true if comment is found."
  (let (state stop-in cpoint (lim (progn (end-of-line) (point))))
      (beginning-of-line)
      (if (re-search-forward "\\=[ \t]*\\(#\\|$\\)" lim t) 
	  (if (eq (preceding-char) ?\#) (progn (backward-char 1) t))
	;; Else
	(while (not stop-in)
	  (setq state (parse-partial-sexp (point) lim nil nil nil t))
					; stop at comment
	  ;; If fails (beginning-of-line inside sexp), then contains not-comment
	  ;; Do simplified processing
	  ;;(if (re-search-forward "[^$]#" lim 1)
	  ;;      (progn
	  ;;	(forward-char -1)
	  ;;	(skip-chars-backward " \t\n\f" lim))
	  ;;    (goto-char lim))		; No `#' at all
	  ;;)
	  (if (nth 4 state)		; After `#';
					; (nth 2 state) can be
					; beginning of m,s,qq and so
					; on
	      (if (nth 2 state)
		  (progn
		    (setq cpoint (point))
		    (goto-char (nth 2 state))
		    (cond
		     ((looking-at "\\(s\\|tr\\)\\>")
		      (or (re-search-forward
			   "\\=\\w+[ \t]*#\\([^\n\\\\#]\\|\\\\[\\\\#]\\)*#\\([^\n\\\\#]\\|\\\\[\\\\#]\\)*"
			   lim 'move)
			  (setq stop-in t)))
		     ((looking-at "\\(m\\|q\\([qxw]\\)?\\)\\>")
		      (or (re-search-forward
			   "\\=\\w+[ \t]*#\\([^\n\\\\#]\\|\\\\[\\\\#]\\)*#"
			   lim 'move)
			  (setq stop-in t)))
		     (t			; It was fair comment
		      (setq stop-in t)	; Finish
		      (goto-char (1- cpoint)))))
		(setq stop-in t)	; Finish
		(forward-char -1))
	    (setq stop-in t))		; Finish
	  )
	(nth 4 state))))

(defun cperl-find-pods-heres (&optional min max)
  "Scans the buffer for POD sections and here-documents.
If `cperl-pod-here-fontify' is not-nil after evaluation, will fontify 
the sections using `cperl-pod-head-face', `cperl-pod-face', 
`cperl-here-face'."
  (interactive)
  (or min (setq min (point-min)))
  (or max (setq max (point-max)))
  (let (face head-face here-face b e bb tag err
	     (cperl-pod-here-fontify (eval cperl-pod-here-fontify))
	     (case-fold-search nil) (inhibit-read-only t) (buffer-undo-list t)
	     (modified (buffer-modified-p)))
    (unwind-protect
	(progn
	  (save-excursion
	    (message "Scanning for pods and here-docs...")
	    (if cperl-pod-here-fontify
		(setq face (eval cperl-pod-face) 
		      head-face (eval cperl-pod-head-face)
		      here-face (eval cperl-here-face)))
	    (remove-text-properties min max '(syntax-type t))
	    ;; Need to remove face as well...
	    (goto-char min)
	    (while (re-search-forward "\\(\\`\n?\\|\n\n\\)=" max t)
	      (if (looking-at "\n*cut\\>")
		  (progn
		    (message "=cut is not preceeded by a pod section")
		    (setq err (point)))
		(beginning-of-line)
		(setq b (point) bb b)
		(or (re-search-forward "\n\n=cut\\>" max 'toend)
		    (message "Cannot find the end of a pod section"))
		(beginning-of-line 4)
		(setq e (point))
		(put-text-property b e 'in-pod t)
		(goto-char b)
		(while (re-search-forward "\n\n[ \t]" e t)
		  (beginning-of-line)
		  (put-text-property b (point) 'syntax-type 'pod)
		  (put-text-property (max (point-min) (1- b))
				     (point) cperl-do-not-fontify t)
		  (if cperl-pod-here-fontify (put-text-property b (point) 'face face))
		  (re-search-forward "\n\n[^ \t\f]" e 'toend)
		  (beginning-of-line)
		  (setq b (point)))
		(put-text-property (point) e 'syntax-type 'pod)
		(put-text-property (max (point-min) (1- (point)))
				   e cperl-do-not-fontify t)
		(if cperl-pod-here-fontify 
		    (progn (put-text-property (point) e 'face face)
			   (goto-char bb)
			   (while (re-search-forward
				   ;; One paragraph
				   "\n\n=[a-zA-Z0-9]+\\>[ \t]*\\(\\(\n?[^\n]\\)+\\)$"
				   e 'toend)
			     (put-text-property 
			      (match-beginning 1) (match-end 1)
			      'face head-face))))
		(goto-char e)))
	    (goto-char min)
	    (while (re-search-forward 
		    "<<\\(\\([\"'`]\\)?\\)\\([a-zA-Z_][a-zA-Z_0-9]*\\)\\1"
		    max t)
	      (setq tag (buffer-substring (match-beginning 3)
					  (match-end 3)))
	      (if cperl-pod-here-fontify 
		  (put-text-property (match-beginning 3) (match-end 3) 
				     'face font-lock-reference-face))
	      (forward-line)
	      (setq b (point))
	      (and (re-search-forward (concat "^" tag "$") max 'toend)
		   (progn
		     (if cperl-pod-here-fontify 
			 (progn
			   (put-text-property (match-beginning 0) (match-end 0) 
					      'face font-lock-reference-face)
			   (put-text-property (max (point-min) (1- b))
					      (min (point-mox)
						   (1+ (match-end 0)))
					      cperl-do-not-fontify t)
			   (put-text-property b (match-beginning 0) 
					      'face here-face)))
		     (put-text-property b (match-beginning 0) 
					'syntax-type 'here-doc)))))
	  (if err (goto-char err)
	    (message "Scan for pods and here-docs completed.")))
      (and (buffer-modified-p)
	   (not modified)
	   (set-buffer-modified-p nil)))))

(defun cperl-backward-to-noncomment (lim)
  ;; Stops at lim or after non-whitespace that is not in comment
  (let (stop p)
    (while (and (not stop) (> (point) (or lim 1)))
      (skip-chars-backward " \t\n\f" lim)
      (setq p (point))
      (beginning-of-line)
      (if (looking-at "^[ \t]*\\(#\\|$\\)") nil	; Only comment, skip
	;; Else
	(cperl-to-comment-or-eol) 
	(skip-chars-backward " \t")
	(if (< p (point)) (goto-char p))
	(setq stop t)))))

(defun cperl-after-expr-p (&optional lim chars test)
  "Returns true if the position is good for start of expression.
TEST is the expression to evaluate at the found position. If absent,
CHARS is a string that contains good characters to have before us."
  (let (stop p)
    (save-excursion
      (while (and (not stop) (> (point) (or lim 1)))
	(skip-chars-backward " \t\n\f" lim)
	(setq p (point))
	(beginning-of-line)
	(if (looking-at "^[ \t]*\\(#\\|$\\)") nil ; Only comment, skip
	  ;; Else: last iteration (What to do with labels?)
	  (cperl-to-comment-or-eol) 
	  (skip-chars-backward " \t")
	  (if (< p (point)) (goto-char p))
	  (setq stop t)))
      (or (bobp)
	  (progn
	    (backward-char 1)
	    (if test (eval test)
	      (memq (following-char) (append (or chars "{};") nil))))))))

(defun cperl-backward-to-start-of-continued-exp (lim)
  (if (memq (preceding-char) (append ")]}\"'`" nil))
      (forward-sexp -1))
  (beginning-of-line)
  (if (<= (point) lim)
      (goto-char (1+ lim)))
  (skip-chars-forward " \t"))


(defvar innerloop-done nil)
(defvar last-depth nil)

(defun cperl-indent-exp ()
  "Simple variant of indentation of continued-sexp.
Should be slow. Will not indent comment if it starts at `comment-indent'
or looks like continuation of the comment on the previous line."
  (interactive)
  (save-excursion
    (let ((tmp-end (progn (end-of-line) (point))) top done)
      (save-excursion
	(while (null done)
	  (beginning-of-line)
	  (setq top (point))
	  (while (= (nth 0 (parse-partial-sexp (point) tmp-end
					       -1)) -1)
	    (setq top (point)))		; Get the outermost parenths in line
	  (goto-char top)
	  (while (< (point) tmp-end)
	    (parse-partial-sexp (point) tmp-end nil t) ; To start-sexp or eol
	    (or (eolp) (forward-sexp 1)))
	  (if (> (point) tmp-end) (progn (end-of-line) (setq tmp-end (point)))
	    (setq done t)))
	(goto-char tmp-end)
	(setq tmp-end (point-marker)))
      (cperl-indent-region (point) tmp-end))))

(defun cperl-indent-region (start end)
  "Simple variant of indentation of region in CPerl mode.
Should be slow. Will not indent comment if it starts at `comment-indent' 
or looks like continuation of the comment on the previous line.
Indents all the lines whose first character is between START and END 
inclusive."
  (interactive "r")
  (save-excursion
    (let (st comm indent-info old-comm-indent new-comm-indent 
	     (pm 0) (imenu-scanning-message "Indenting... (%3d%%)"))
      (goto-char start)
      (setq old-comm-indent (and (cperl-to-comment-or-eol)
				 (current-column))
	    new-comm-indent old-comm-indent)
      (goto-char start)
      (or (bolp) (beginning-of-line 2))
      (or (fboundp 'imenu-progress-message)
	  (message "Indenting... For feedback load `imenu'..."))
      (while (and (<= (point) end) (not (eobp))) ; bol to check start
	(and (fboundp 'imenu-progress-message)
	     (imenu-progress-message 
	      pm (/ (* 100 (- (point) start)) (- end start -1))))
	(setq st (point) 
	      indent-info nil
	      ) ; Believe indentation of the current
	(if (and (setq comm (looking-at "[ \t]*#"))
		 (or (eq (current-indentation) (or old-comm-indent 
						   comment-column))
		     (setq old-comm-indent nil)))
	    (if (and old-comm-indent
		     (= (current-indentation) old-comm-indent))
		(let ((comment-column new-comm-indent))
		  (indent-for-comment)))
	  (progn 
	    (cperl-indent-line 'indent-info)
	    (or comm
		(progn
		  (if (setq old-comm-indent (and (cperl-to-comment-or-eol)
						 (current-column)))
		      (progn (indent-for-comment)
			     (skip-chars-backward " \t")
			     (skip-chars-backward "#")
			     (setq new-comm-indent (current-column))))))))
	(beginning-of-line 2))
      	(if (fboundp 'imenu-progress-message)
	     (imenu-progress-message pm 100)
	  (message nil)))))

(defun cperl-slash-is-regexp (&optional pos)
  (save-excursion
    (goto-char (if pos pos (1- (point))))
    (and
     (not (memq (get-text-property (point) 'face)
		'(font-lock-string-face font-lock-comment-face)))
     (cperl-after-expr-p nil nil '
		       (or (looking-at "[^]a-zA-Z0-9_)}]")
			   (eq (get-text-property (point) 'face)
			       'font-lock-keyword-face))))))

;; Stolen from lisp-mode with a lot of improvements

(defun cperl-fill-paragraph (&optional justify iteration)
  "Like \\[fill-paragraph], but handle CPerl comments.
If any of the current line is a comment, fill the comment or the
block of it that point is in, preserving the comment's initial
indentation and initial hashes. Behaves usually outside of comment."
  (interactive "P")
  (let (
	;; Non-nil if the current line contains a comment.
	has-comment

	;; If has-comment, the appropriate fill-prefix for the comment.
	comment-fill-prefix
	;; Line that contains code and comment (or nil)
	start
	c spaces len dc (comment-column comment-column))
    ;; Figure out what kind of comment we are looking at.
    (save-excursion
      (beginning-of-line)
      (cond

       ;; A line with nothing but a comment on it?
       ((looking-at "[ \t]*#[# \t]*")
	(setq has-comment t
	      comment-fill-prefix (buffer-substring (match-beginning 0)
						    (match-end 0))))

       ;; A line with some code, followed by a comment?  Remember that the
       ;; semi which starts the comment shouldn't be part of a string or
       ;; character.
       ((cperl-to-comment-or-eol)
	(setq has-comment t)
	(looking-at "#+[ \t]*")
	(setq start (point) c (current-column) 
	      comment-fill-prefix
	      (concat (make-string (current-column) ?\ )
		      (buffer-substring (match-beginning 0) (match-end 0)))
	      spaces (progn (skip-chars-backward " \t") 
			    (buffer-substring (point) start))
	      dc (- c (current-column)) len (- start (point)) 
	      start (point-marker))
	(delete-char len)
	(insert (make-string dc ?-)))))
    (if (not has-comment)
	(fill-paragraph justify)	; Do the usual thing outside of comment
      ;; Narrow to include only the comment, and then fill the region.
      (save-restriction
	(narrow-to-region
	 ;; Find the first line we should include in the region to fill.
	 (if start (progn (beginning-of-line) (point))
	   (save-excursion
	     (while (and (zerop (forward-line -1))
			 (looking-at "^[ \t]*#+[ \t]*[^ \t\n#]")))
	     ;; We may have gone to far.  Go forward again.
	     (or (looking-at "^[ \t]*#+[ \t]*[^ \t\n#]")
		 (forward-line 1))
	     (point)))
	 ;; Find the beginning of the first line past the region to fill.
	 (save-excursion
	   (while (progn (forward-line 1)
			 (looking-at "^[ \t]*#+[ \t]*[^ \t\n#]")))
	   (point)))
	;; Remove existing hashes
	(goto-char (point-min))
	(while (progn (forward-line 1) (< (point) (point-max)))
	  (skip-chars-forward " \t")
	  (and (looking-at "#+") 
	       (delete-char (- (match-end 0) (match-beginning 0)))))

	;; Lines with only hashes on them can be paragraph boundaries.
	(let ((paragraph-start (concat paragraph-start "\\|^[ \t#]*$"))
	      (paragraph-separate (concat paragraph-start "\\|^[ \t#]*$"))
	      (fill-prefix comment-fill-prefix))
	  (fill-paragraph justify)))
      (if (and start)
	  (progn 
	    (goto-char start)
	    (if (> dc 0)
	      (progn (delete-char dc) (insert spaces)))
	    (if (or (= (current-column) c) iteration) nil
	      (setq comment-column c)
	      (indent-for-comment)
	      ;; Repeat once more, flagging as iteration
	      (cperl-fill-paragraph justify t)))))))

(defun cperl-do-auto-fill ()
  ;; Break out if the line is short enough
  (if (> (save-excursion
	   (end-of-line)
	   (current-column))
	 fill-column)
  (let ((c (save-excursion (beginning-of-line)
			   (cperl-to-comment-or-eol) (point)))
	(s (memq (following-char) '(?\ ?\t))) marker)
    (if (>= c (point)) nil
      (setq marker (point-marker))
      (cperl-fill-paragraph)
      (goto-char marker)
      ;; Is not enough, sometimes marker is a start of line
      (if (bolp) (progn (re-search-forward "#+[ \t]*") 
			(goto-char (match-end 0))))
      ;; Following space could have gone:
      (if (or (not s) (memq (following-char) '(?\ ?\t))) nil
	(insert " ")
	(backward-char 1))
      ;; Previous space could have gone:
      (or (memq (preceding-char) '(?\ ?\t)) (insert " "))))))

(defvar imenu-example--function-name-regexp-perl
      "^\\([ \t]*\\(sub\\|package\\)[ \t\n]+\\([a-zA-Z_0-9:']+\\)[ \t]*\\|=head\\([12]\\)[ \t]+\\([^\n]+\\)$\\)")

(defun imenu-example--create-perl-index (&optional regexp)
  (require 'cl)
  (let ((index-alist '()) (index-pack-alist '()) (index-pod-alist '()) 
	(index-unsorted-alist '()) (i-s-f (default-value 'imenu-sort-function))
	packages ends-ranges p
	(prev-pos 0) char fchar index index1 name (end-range 0) package)
    (goto-char (point-min))
    (imenu-progress-message prev-pos 0)
    ;; Search for the function
    (save-match-data
      (while (re-search-forward
	      (or regexp imenu-example--function-name-regexp-perl)
	      nil t)
	(imenu-progress-message prev-pos)
	;;(backward-up-list 1)
	(cond
	 ((match-beginning 2)		; package or sub
	  (save-excursion
	    (goto-char (match-beginning 2))
	    (setq fchar (following-char))
	    )
	  (setq char (following-char))
	  (setq p (point))
	  (while (and ends-ranges (>= p (car ends-ranges)))
	    ;; delete obsolete entries
	    (setq ends-ranges (cdr ends-ranges) packages (cdr packages)))
	  (setq package (or (car packages) "")
		end-range (or (car ends-ranges) 0))
	  (if (eq fchar ?p)
	      (progn 
		(setq name (buffer-substring (match-beginning 3) (match-end 3))
		      package (concat name "::") 
		      name (concat "package " name)
		      end-range 
		      (save-excursion
			(parse-partial-sexp (point) (point-max) -1) (point))
		      ends-ranges (cons end-range ends-ranges)
		      packages (cons package packages))))
	  ;;   )
	  ;; Skip this function name if it is a prototype declaration.
	  (if (and (eq fchar ?s) (eq char ?\;)) nil
	    (if (eq fchar ?p) nil
	      (setq name (buffer-substring (match-beginning 3) (match-end 3)))
	      (if (or (> p end-range) (string-match "[:']" name)) nil
		(setq name (concat package name))))
	    (setq index (imenu-example--name-and-position))
	    (setcar index name)
	    (if (eq fchar ?p) 
		(push index index-pack-alist)
	      (push index index-alist))
	    (push index index-unsorted-alist)))
	 (t				; Pod section
	  ;; (beginning-of-line)
	  (setq index (imenu-example--name-and-position)
		name (buffer-substring (match-beginning 5) (match-end 5)))
	  (if (eq (char-after (match-beginning 4)) ?2)
	      (setq name (concat "   " name)))
	  (setcar index name)
	  (setq index1 (cons (concat "=" name) (cdr index)))
	  (push index index-pod-alist)
	  (push index1 index-unsorted-alist)))))
    (imenu-progress-message prev-pos 100)
    (setq index-alist 
	  (if (default-value 'imenu-sort-function)
	      (sort index-alist (default-value 'imenu-sort-function))
	      (nreverse index-alist)))
    (and index-pod-alist
	 (push (cons (imenu-create-submenu-name "+POD headers+") 
		     (nreverse index-pod-alist))
	       index-alist))
    (and index-pack-alist
	 (push (cons (imenu-create-submenu-name "+Packages+") 
		     (nreverse index-pack-alist))
	       index-alist))
    (and (or index-pack-alist index-pod-alist 
	     (default-value 'imenu-sort-function))
	 index-unsorted-alist
	 (push (cons (imenu-create-submenu-name "+Unsorted List+") 
		     (nreverse index-unsorted-alist))
	       index-alist))
    index-alist))

(defvar cperl-compilation-error-regexp-alist 
  ;; This look like a paranoiac regexp: could anybody find a better one? (which WORK).
  '(("^[^\n]* \\(file\\|at\\) \\([^ \t\n]+\\) [^\n]*line \\([0-9]+\\)[\\., \n]"
     2 3))
  "Alist that specifies how to match errors in perl output.")

(if (fboundp 'eval-after-load)
    (eval-after-load
     "mode-compile"
     '(setq perl-compilation-error-regexp-alist
	   cperl-compilation-error-regexp-alist)))


(defvar cperl-faces-init nil)

(defun cperl-windowed-init ()
  "Initialization under windowed version."
  (add-hook 'font-lock-mode-hook
	    (function
	     (lambda ()
	       (if (or
		    (eq major-mode 'perl-mode)
		    (eq major-mode 'cperl-mode))
		   (progn
		     (or cperl-faces-init (cperl-init-faces))))))))

(defvar perl-font-lock-keywords-1 nil
  "Additional expressions to highlight in Perl mode. Minimal set.")
(defvar perl-font-lock-keywords nil
  "Additional expressions to highlight in Perl mode. Default set.")
(defvar perl-font-lock-keywords-2 nil
  "Additional expressions to highlight in Perl mode. Maximal set")

(defun cperl-init-faces ()
  (condition-case nil
      (progn
	(require 'font-lock)
	(and (fboundp 'font-lock-fontify-anchored-keywords)
	     (featurep 'font-lock-extra)
	     (message "You have an obsolete package `font-lock-extra'. Install `choose-color'."))
	(let (t-font-lock-keywords t-font-lock-keywords-1 font-lock-anchored)
	  ;;(defvar cperl-font-lock-enhanced nil
	  ;;  "Set to be non-nil if font-lock allows active highlights.")
	  (if (fboundp 'font-lock-fontify-anchored-keywords)
	      (setq font-lock-anchored t))
	  (setq 
	   t-font-lock-keywords
	   (list
	    (cons
	     (concat
	      "\\(^\\|[^$@%&\\]\\)\\<\\("
	      (mapconcat
	       'identity
	       '("if" "until" "while" "elsif" "else" "unless" "for"
		 "foreach" "continue" "exit" "die" "last" "goto" "next"
		 "redo" "return" "local" "exec" "sub" "do" "dump" "use"
		 "require" "package" "eval" "my" "BEGIN" "END")
	       "\\|")			; Flow control
	      "\\)\\>") 2)		; was "\\)[ \n\t;():,\|&]"
					; In what follows we use `type' style
					; for overwritable buildins
	    (list
	     (concat
	      "\\(^\\|[^$@%&\\]\\)\\<\\("
	      ;; "CORE" "__FILE__" "__LINE__" "abs" "accept" "alarm" "and" "atan2"
	      ;; "bind" "binmode" "bless" "caller" "chdir" "chmod" "chown" "chr"
	      ;; "chroot" "close" "closedir" "cmp" "connect" "continue" "cos"
	      ;; "crypt" "dbmclose" "dbmopen" "die" "dump" "endgrent" "endhostent"
	      ;; "endnetent" "endprotoent" "endpwent" "endservent" "eof" "eq" "exec"
	      ;; "exit" "exp" "fcntl" "fileno" "flock" "fork" "formline" "ge" "getc"
	      ;; "getgrent" "getgrgid" "getgrnam" "gethostbyaddr" "gethostbyname"
	      ;; "gethostent" "getlogin" "getnetbyaddr" "getnetbyname" "getnetent"
	      ;; "getpeername" "getpgrp" "getppid" "getpriority" "getprotobyname"
	      ;; "getprotobynumber" "getprotoent" "getpwent" "getpwnam" "getpwuid"
	      ;; "getservbyname" "getservbyport" "getservent" "getsockname"
	      ;; "getsockopt" "glob" "gmtime" "gt" "hex" "index" "int" "ioctl"
	      ;; "join" "kill" "lc" "lcfirst" "le" "length" "link" "listen"
	      ;; "localtime" "log" "lstat" "lt" "mkdir" "msgctl" "msgget" "msgrcv"
	      ;; "msgsnd" "ne" "not" "oct" "open" "opendir" "or" "ord" "pack" "pipe"
	      ;; "quotemeta" "rand" "read" "readdir" "readline" "readlink"
	      ;; "readpipe" "recv" "ref" "rename" "require" "reset" "reverse"
	      ;; "rewinddir" "rindex" "rmdir" "seek" "seekdir" "select" "semctl"
	      ;; "semget" "semop" "send" "setgrent" "sethostent" "setnetent"
	      ;; "setpgrp" "setpriority" "setprotoent" "setpwent" "setservent"
	      ;; "setsockopt" "shmctl" "shmget" "shmread" "shmwrite" "shutdown"
	      ;; "sin" "sleep" "socket" "socketpair" "sprintf" "sqrt" "srand" "stat"
	      ;; "substr" "symlink" "syscall" "sysread" "system" "syswrite" "tell"
	      ;; "telldir" "time" "times" "truncate" "uc" "ucfirst" "umask" "unlink"
	      ;; "unpack" "utime" "values" "vec" "wait" "waitpid" "wantarray" "warn"
	      ;; "write" "x" "xor"
	      "a\\(bs\\|ccept\\|tan2\\|larm\\|nd\\)\\|" 
	      "b\\(in\\(d\\|mode\\)\\|less\\)\\|"
	      "c\\(h\\(r\\(\\|oot\\)\\|dir\\|mod\\|own\\)\\|aller\\|rypt\\|"
	      "lose\\(\\|dir\\)\\|mp\\|o\\(s\\|n\\(tinue\\|nect\\)\\)\\)\\|"
	      "CORE\\|d\\(ie\\|bm\\(close\\|open\\)\\|ump\\)\\|"
	      "e\\(x\\(p\\|it\\|ec\\)\\|q\\|nd\\(p\\(rotoent\\|went\\)\\|"
	      "hostent\\|servent\\|netent\\|grent\\)\\|of\\)\\|"
	      "f\\(ileno\\|cntl\\|lock\\|or\\(k\\|mline\\)\\)\\|"
	      "g\\(t\\|lob\\|mtime\\|e\\(\\|t\\(p\\(pid\\|r\\(iority\\|"
	      "oto\\(byn\\(ame\\|umber\\)\\|ent\\)\\)\\|eername\\|w"
	      "\\(uid\\|ent\\|nam\\)\\|grp\\)\\|host\\(by\\(addr\\|name\\)\\|"
	      "ent\\)\\|s\\(erv\\(by\\(port\\|name\\)\\|ent\\)\\|"
	      "ock\\(name\\|opt\\)\\)\\|c\\|login\\|net\\(by\\(addr\\|name\\)\\|"
	      "ent\\)\\|gr\\(ent\\|nam\\|gid\\)\\)\\)\\)\\|"
	      "hex\\|i\\(n\\(t\\|dex\\)\\|octl\\)\\|join\\|kill\\|"
	      "l\\(i\\(sten\\|nk\\)\\|stat\\|c\\(\\|first\\)\\|t\\|e"
	      "\\(\\|ngth\\)\\|o\\(caltime\\|g\\)\\)\\|m\\(sg\\(rcv\\|snd\\|"
	      "ctl\\|get\\)\\|kdir\\)\\|n\\(e\\|ot\\)\\|o\\(pen\\(\\|dir\\)\\|"
	      "r\\(\\|d\\)\\|ct\\)\\|p\\(ipe\\|ack\\)\\|quotemeta\\|"
	      "r\\(index\\|and\\|mdir\\|e\\(quire\\|ad\\(pipe\\|\\|lin"
	      "\\(k\\|e\\)\\|dir\\)\\|set\\|cv\\|verse\\|f\\|winddir\\|name"
	      "\\)\\)\\|s\\(printf\\|qrt\\|rand\\|tat\\|ubstr\\|e\\(t\\(p\\(r"
	      "\\(iority\\|otoent\\)\\|went\\|grp\\)\\|hostent\\|s\\(ervent\\|"
	      "ockopt\\)\\|netent\\|grent\\)\\|ek\\(\\|dir\\)\\|lect\\|"
	      "m\\(ctl\\|op\\|get\\)\\|nd\\)\\|h\\(utdown\\|m\\(read\\|ctl\\|"
	      "write\\|get\\)\\)\\|y\\(s\\(read\\|call\\|tem\\|write\\)\\|"
	      "mlink\\)\\|in\\|leep\\|ocket\\(pair\\|\\)\\)\\|t\\(runcate\\|"
	      "ell\\(\\|dir\\)\\|ime\\(\\|s\\)\\)\\|u\\(c\\(\\|first\\)\\|"
	      "time\\|mask\\|n\\(pack\\|link\\)\\)\\|v\\(alues\\|ec\\)\\|"
	      "w\\(a\\(rn\\|it\\(pid\\|\\)\\|ntarray\\)\\|rite\\)\\|"
	      "x\\(\\|or\\)\\|__\\(FILE__\\|LINE__\\)"
	      "\\)\\>") 2 'font-lock-type-face)
	    ;; In what follows we use `other' style
	    ;; for nonoverwritable buildins
	    ;; Somehow 's', 'm' are not autogenerated???
	    (list
	     (concat
	      "\\(^\\|[^$@%&\\]\\)\\<\\("
	      ;; "AUTOLOAD" "BEGIN" "DESTROY" "END" "__END__" "chomp" "chop"
	      ;; "defined" "delete" "do" "each" "else" "elsif" "eval" "exists" "for"
	      ;; "foreach" "format" "goto" "grep" "if" "keys" "last" "local" "map"
	      ;; "my" "next" "no" "package" "pop" "pos" "print" "printf" "push" "q"
	      ;; "qq" "qw" "qx" "redo" "return" "scalar" "shift" "sort" "splice"
	      ;; "split" "study" "sub" "tie" "tr" "undef" "unless" "unshift" "untie"
	      ;; "until" "use" "while" "y"
	      "AUTOLOAD\\|BEGIN\\|cho\\(p\\|mp\\)\\|d\\(e\\(fined\\|lete\\)\\|"
	      "o\\)\\|DESTROY\\|e\\(ach\\|val\\|xists\\|ls\\(e\\|if\\)\\)\\|"
	      "END\\|for\\(\\|each\\|mat\\)\\|g\\(rep\\|oto\\)\\|if\\|keys\\|"
	      "l\\(ast\\|ocal\\)\\|m\\(ap\\|y\\)\\|n\\(ext\\|o\\)\\|"
	      "p\\(ackage\\|rint\\(\\|f\\)\\|ush\\|o\\(p\\|s\\)\\)\\|"
	      "q\\(\\|q\\|w\\|x\\)\\|re\\(turn\\|do\\)\\|s\\(pli\\(ce\\|t\\)\\|"
	      "calar\\|tudy\\|ub\\|hift\\|ort\\)\\|t\\(r\\|ie\\)\\|"
	      "u\\(se\\|n\\(shift\\|ti\\(l\\|e\\)\\|def\\|less\\)\\)\\|"
	      "while\\|y\\|__\\(END\\|DATA\\)__" ;__DATA__ added manually
	      "\\|[sm]"			; Added manually
	      "\\)\\>") 2 'font-lock-other-type-face)
	    ;;		(mapconcat 'identity
	    ;;			   '("#endif" "#else" "#ifdef" "#ifndef" "#if"
	    ;;			     "#include" "#define" "#undef")
	    ;;			   "\\|")
	    '("-[rwxoRWXOezsfdlpSbctugkTBMAC]\\>\\([ \t]+_\\>\\)?" 0
	      font-lock-function-name-face) ; Not very good, triggers at "[a-z]"
	    '("\\<sub[ \t]+\\([^ \t{;]+\\)[ \t]*[{\n]" 1
	      font-lock-function-name-face)
	    '("\\<\\(package\\|require\\|use\\|import\\|no\\|bootstrap\\)[ \t]+\\([a-zA-z_][a-zA-z_0-9:]*\\)[ \t;]" ; require A if B;
	      2 font-lock-function-name-face)
	    (cond ((featurep 'font-lock-extra)
		   '("\\([]}\\\\%@>*&]\\|\\$[a-zA-Z0-9_:]*\\)[ \t]*{[ \t]*\\(-?[a-zA-Z0-9_:]+\\)[ \t]*}" 
		     (2 font-lock-string-face t)
		     (0 '(restart 2 t)))) ; To highlight $a{bc}{ef}
		  (font-lock-anchored
		   '("\\([]}\\\\%@>*&]\\|\\$[a-zA-Z0-9_:]*\\)[ \t]*{[ \t]*\\(-?[a-zA-Z0-9_:]+\\)[ \t]*}"
		     (2 font-lock-string-face t)
		     ("\\=[ \t]*{[ \t]*\\(-?[a-zA-Z0-9_:]+\\)[ \t]*}"
		      nil nil
		      (1 font-lock-string-face t))))
		  (t '("\\([]}\\\\%@>*&]\\|\\$[a-zA-Z0-9_:]*\\)[ \t]*{[ \t]*\\(-?[a-zA-Z0-9_:]+\\)[ \t]*}"
		       2 font-lock-string-face t)))
	    '("[ \t{,(]\\(-?[a-zA-Z0-9_:]+\\)[ \t]*=>" 1
	      font-lock-string-face t)
	    '("^[ \t]*\\([a-zA-Z0-9_]+[ \t]*:\\)[ \t]*\\($\\|{\\|\\<\\(until\\|while\\|for\\(each\\)?\\|do\\)\\>\\)" 1 
	      font-lock-reference-face) ; labels
	    '("\\<\\(continue\\|next\\|last\\|redo\\|goto\\)\\>[ \t]+\\([a-zA-Z0-9_:]+\\)" ; labels as targets
	      2 font-lock-reference-face)
	    (cond ((featurep 'font-lock-extra)
		   '("^[ \t]*\\(my\\|local\\)[ \t]*\\(([ \t]*\\)?\\([$@%*][a-zA-Z0-9_:]+\\)\\([ \t]*,\\)?"
		     (3 font-lock-variable-name-face)
		     (4 '(another 4 nil
				  ("\\=[ \t]*,[ \t]*\\([$@%*][a-zA-Z0-9_:]+\\)\\([ \t]*,\\)?"
				   (1 font-lock-variable-name-face)
				   (2 '(restart 2 nil) nil t))) 
			nil t)))	; local variables, multiple
		  (font-lock-anchored
		   '("^[ \t]*\\(my\\|local\\)[ \t]*\\(([ \t]*\\)?\\([$@%*][a-zA-Z0-9_:]+\\)"
		     (3 font-lock-variable-name-face)
		     ("\\=[ \t]*,[ \t]*\\([$@%*][a-zA-Z0-9_:]+\\)"
		      nil nil
		      (1 font-lock-variable-name-face))))
		  (t '("^[ \t]*\\(my\\|local\\)[ \t]*\\(([ \t]*\\)?\\([$@%*][a-zA-Z0-9_:]+\\)"
		       3 font-lock-variable-name-face)))
	    '("\\<for\\(each\\)?[ \t]*\\(\\$[a-zA-Z_][a-zA-Z_0-9]*\\)[ \t]*("
	      2 font-lock-variable-name-face)))
	  (setq 
	   t-font-lock-keywords-1
	   (and (fboundp 'turn-on-font-lock) ; Check for newer font-lock
		(not (cperl-xemacs-p)) ; not yet as of XEmacs 19.12
		'(("\\(\\([$@]+\\)[a-zA-Z_:][a-zA-Z0-9_:]*\\)[ \t]*\\([[{]\\)"
		   1
		   (if (= (- (match-end 2) (match-beginning 2)) 1) 
		       (if (eq (char-after (match-beginning 3)) ?{)
			   font-lock-other-emphasized-face
			 font-lock-emphasized-face) ; arrays and hashes
		     font-lock-variable-name-face) ; Just to put something
		   t)
		  ("\\(\\([@%]\\|\$#\\)[a-zA-Z_:][a-zA-Z0-9_:]*\\)" 1
		   (if (eq (char-after (match-beginning 2)) ?%)
		       font-lock-other-emphasized-face
		     font-lock-emphasized-face)
		   t)			; arrays and hashes
		  ;;("\\([smy]\\|tr\\)\\([^a-z_A-Z0-9]\\)\\(\\([^\n\\]*||\\)\\)\\2")
		       ;;; Too much noise from \s* @s[ and friends
		  ;;("\\(\\<\\([msy]\\|tr\\)[ \t]*\\([^ \t\na-zA-Z0-9_]\\)\\|\\(/\\)\\)" 
		  ;;(3 font-lock-function-name-face t t)
		  ;;(4
		  ;; (if (cperl-slash-is-regexp)
		  ;;    font-lock-function-name-face 'default) nil t))
		  )))
	  (setq perl-font-lock-keywords-1 t-font-lock-keywords
		perl-font-lock-keywords perl-font-lock-keywords-1
		perl-font-lock-keywords-2 (append
					   t-font-lock-keywords
					   t-font-lock-keywords-1)))
	(if (fboundp 'ps-print-buffer) (cperl-ps-print-init))
	(if (or (featurep 'choose-color) (featurep 'font-lock-extra))
	    (font-lock-require-faces
	     (list
	      ;; Color-light    Color-dark      Gray-light      Gray-dark Mono
	      (list 'font-lock-comment-face
		    ["Firebrick"	"OrangeRed" 	"DimGray"	"Gray80"]
		    nil
		    [nil		nil		t		t	t]
		    [nil		nil		t		t	t]
		    nil)
	      (list 'font-lock-string-face
		    ["RosyBrown"	"LightSalmon" 	"Gray50"	"LightGray"]
		    nil
		    nil
		    [nil		nil		t		t	t]
		    nil)
	      (list 'font-lock-keyword-face
		    ["Purple"		"LightSteelBlue" "DimGray"	"Gray90"]
		    nil
		    [nil		nil		t		t	t]
		    nil
		    nil)
	      (list 'font-lock-function-name-face
		    (vector
		     "Blue"		"LightSkyBlue"	"Gray50"	"LightGray"
		     (cdr (assq 'background-color ; if mono
				(frame-parameters))))
		    (vector
		     nil		nil		nil		nil
		     (cdr (assq 'foreground-color ; if mono
				(frame-parameters))))
		    [nil		nil		t		t	t]
		    nil
		    nil)
	      (list 'font-lock-variable-name-face
		    ["DarkGoldenrod"	"LightGoldenrod" "DimGray"	"Gray90"]
		    nil
		    [nil		nil		t		t	t]
		    [nil		nil		t		t	t]
		    nil)
	      (list 'font-lock-type-face
		    ["DarkOliveGreen"	"PaleGreen" 	"DimGray"	"Gray80"]
		    nil
		    [nil		nil		t		t	t]
		    nil
		    [nil		nil		t		t	t]
		    )
	      (list 'font-lock-reference-face
		    ["CadetBlue"	"Aquamarine" 	"Gray50"	"LightGray"]
		    nil
		    [nil		nil		t		t	t]
		    nil
		    [nil		nil		t		t	t]
		    )
	      (list 'font-lock-other-type-face
		    ["chartreuse3"	("orchid1" "orange")
		     nil		"Gray80"]
		    [nil		nil		"gray90"]
		    [nil		nil		nil		t	t]
		    [nil		nil		t		t]
		    [nil		nil		t		t	t]
		    )
	      (list 'font-lock-emphasized-face
		    ["blue"		"yellow" 	nil		"Gray80"]
		    ["lightyellow2"	("navy" "os2blue" "darkgreen")
		     "gray90"]
		    t
		    nil
		    nil)
	      (list 'font-lock-other-emphasized-face
		    ["red"		"red"	 	nil		"Gray80"]
		    ["lightyellow2"	("navy" "os2blue" "darkgreen")
		     "gray90"]
		    t
		    t
		    nil)))
	  (defvar cperl-guessed-background nil
	    "Display characteristics as guessed by cperl.")
	  (or (fboundp 'x-color-defined-p)
	      (defalias 'x-color-defined-p 
		(cond ((fboundp 'color-defined-p) 'color-defined-p)
		      ;; XEmacs >= 19.12
		      ((fboundp 'valid-color-name-p) 'valid-color-name-p)
		      ;; XEmacs 19.11
		      (t 'x-valid-color-name-p))))
	  (defvar font-lock-reference-face 'font-lock-reference-face)
	  (defvar font-lock-variable-name-face 'font-lock-variable-name-face)
	  (or (boundp 'font-lock-type-face)
	      (defconst font-lock-type-face
		'font-lock-type-face
		"Face to use for data types.")
	      )
	  (or (boundp 'font-lock-other-type-face)
	      (defconst font-lock-other-type-face
		'font-lock-other-type-face
		"Face to use for data types from another group.")
	      )
	  (if (not (cperl-xemacs-p)) nil
	    (or (boundp 'font-lock-comment-face)
		(defconst font-lock-comment-face
		  'font-lock-comment-face
		  "Face to use for comments.")
		)
	    (or (boundp 'font-lock-keyword-face)
		(defconst font-lock-keyword-face
		  'font-lock-keyword-face
		  "Face to use for keywords.")
		)
	    (or (boundp 'font-lock-function-name-face)
		(defconst font-lock-function-name-face
		  'font-lock-function-name-face
		  "Face to use for function names.")
		)
	    )
	  ;;(if (featurep 'font-lock)
	  (if (face-equal font-lock-type-face font-lock-comment-face)
	      (defconst font-lock-type-face
		'font-lock-type-face
		"Face to use for basic data types.")
	    )
;;;	  (if (fboundp 'eval-after-load)
;;;	      (eval-after-load "font-lock"
;;;			       '(if (face-equal font-lock-type-face
;;;						font-lock-comment-face)
;;;				    (defconst font-lock-type-face
;;;				      'font-lock-type-face
;;;				      "Face to use for basic data types.")
;;;				  )))	; This does not work :-( Why?!
;;;					; Workaround: added to font-lock-m-h
;;;	  )
	  (or (boundp 'font-lock-other-emphasized-face)
	      (defconst font-lock-other-emphasized-face
		'font-lock-other-emphasized-face
		"Face to use for another type of emphasizing.")
	      )
	  (or (boundp 'font-lock-emphasized-face)
	      (defconst font-lock-emphasized-face
		'font-lock-emphasized-face
		"Face to use for emphasizing.")
	      )
	  ;; Here we try to guess background
	  (let ((background
		 (if (boundp 'font-lock-background-mode)
		     font-lock-background-mode
		   'light)) 
		(face-list (and (fboundp 'face-list) (face-list)))
		is-face)
	    (fset 'is-face
		  (cond ((fboundp 'find-face)
			 (symbol-function 'find-face))
			(face-list
			 (function (lambda (face) (member face face-list))))
			(t
			 (function (lambda (face) (boundp face))))))
	    (defvar cperl-guessed-background
	      (if (and (boundp 'font-lock-display-type)
		       (eq font-lock-display-type 'grayscale))
		  'gray
		background)
	      "Background as guessed by CPerl mode")
	    (if (is-face 'font-lock-type-face) nil
	      (copy-face 'default 'font-lock-type-face)
	      (cond
	       ((eq background 'light)
		(set-face-foreground 'font-lock-type-face
				     (if (x-color-defined-p "seagreen")
					 "seagreen"
				       "sea green")))
	       ((eq background 'dark)
		(set-face-foreground 'font-lock-type-face
				     (if (x-color-defined-p "os2pink")
					 "os2pink"
				       "pink")))
	       (t
		(set-face-background 'font-lock-type-face "gray90"))))
	    (if (is-face 'font-lock-other-type-face)
		nil
	      (copy-face 'font-lock-type-face 'font-lock-other-type-face)
	      (cond
	       ((eq background 'light)
		(set-face-foreground 'font-lock-other-type-face
				     (if (x-color-defined-p "chartreuse3")
					 "chartreuse3"
				       "chartreuse")))
	       ((eq background 'dark)
		(set-face-foreground 'font-lock-other-type-face
				     (if (x-color-defined-p "orchid1")
					 "orchid1"
				       "orange")))))
	    (if (is-face 'font-lock-other-emphasized-face) nil
	      (copy-face 'bold-italic 'font-lock-other-emphasized-face)
	      (cond
	       ((eq background 'light)
		(set-face-background 'font-lock-other-emphasized-face
				     (if (x-color-defined-p "lightyellow2")
					 "lightyellow2"
				       (if (x-color-defined-p "lightyellow")
					   "lightyellow"
					 "light yellow"))))
	       ((eq background 'dark)
		(set-face-background 'font-lock-other-emphasized-face
				     (if (x-color-defined-p "navy")
					 "navy"
				       (if (x-color-defined-p "darkgreen")
					   "darkgreen"
					 "dark green"))))
	       (t (set-face-background 'font-lock-other-emphasized-face "gray90"))))
	    (if (is-face 'font-lock-emphasized-face) nil
	      (copy-face 'bold 'font-lock-emphasized-face)
	      (cond
	       ((eq background 'light)
		(set-face-background 'font-lock-emphasized-face
				     (if (x-color-defined-p "lightyellow2")
					 "lightyellow2"
				       "lightyellow")))
	       ((eq background 'dark)
		(set-face-background 'font-lock-emphasized-face
				     (if (x-color-defined-p "navy")
					 "navy"
				       (if (x-color-defined-p "darkgreen")
					   "darkgreen"
					 "dark green"))))
	       (t (set-face-background 'font-lock-emphasized-face "gray90"))))
	    (if (is-face 'font-lock-variable-name-face) nil
	      (copy-face 'italic 'font-lock-variable-name-face))
	    (if (is-face 'font-lock-reference-face) nil
	      (copy-face 'italic 'font-lock-reference-face))))
	(setq cperl-faces-init t))
    (error nil)))


(defun cperl-ps-print-init ()
  "Initialization of `ps-print' components for faces used in CPerl."
  ;; Guard against old versions
  (defvar ps-underlined-faces nil)
  (defvar ps-bold-faces nil)
  (defvar ps-italic-faces nil)
  (setq ps-bold-faces
	(append '(font-lock-emphasized-face
		  font-lock-keyword-face 
		  font-lock-variable-name-face 
		  font-lock-reference-face 
		  font-lock-other-emphasized-face) 
		ps-bold-faces))
  (setq ps-italic-faces
	(append '(font-lock-other-type-face
		  font-lock-reference-face 
		  font-lock-other-emphasized-face)
		ps-italic-faces))
  (setq ps-underlined-faces
	(append '(font-lock-emphasized-face
		  font-lock-other-emphasized-face 
		  font-lock-other-type-face font-lock-type-face)
		ps-underlined-faces))
  (cons 'font-lock-type-face ps-underlined-faces))


(if (cperl-enable-font-lock) (cperl-windowed-init))

(defun cperl-set-style (style)
  "Set CPerl-mode variables to use one of several different indentation styles.
The arguments are a string representing the desired style.
Available styles are GNU, K&R, BSD and Whitesmith."
  (interactive 
   (let ((list (mapcar (function (lambda (elt) (list (car elt)))) 
		       c-style-alist)))
     (list (completing-read "Enter style: " list nil 'insist))))
  (let ((style (cdr (assoc style c-style-alist))) setting str sym)
    (while style
      (setq setting (car style) style (cdr style))
      (setq str (symbol-name (car setting)))
      (and (string-match "^c-" str)
	   (setq str (concat "cperl-" (substring str 2)))
	   (setq sym (intern-soft str))
	   (boundp sym)
	   (set sym (cdr setting))))))

(defun cperl-check-syntax ()
  (interactive)
  (require 'mode-compile)
  (let ((perl-dbg-flags "-wc"))
    (mode-compile)))

(defun cperl-info-buffer ()
  ;; Returns buffer with documentation. Creats if missing
  (let ((info (get-buffer "*info-perl*")))
    (if info info
      (save-window-excursion
	;; Get Info running
	(require 'info)
	(save-window-excursion
	  (info))
	(Info-find-node "perl5" "perlfunc")
	(set-buffer "*info*")
	(rename-buffer "*info-perl*")
	(current-buffer)))))

(defun cperl-word-at-point (&optional p)
  ;; Returns the word at point or at P.
  (save-excursion
    (if p (goto-char p))
    (require 'etags)
    (funcall (or (and (boundp 'find-tag-default-function)
		      find-tag-default-function)
		 (get major-mode 'find-tag-default-function)
		 ;; XEmacs 19.12 has `find-tag-default-hook'; it is
		 ;; automatically used within `find-tag-default':
		 'find-tag-default))))

(defun cperl-info-on-command (command)
  "Shows documentation for Perl command in other window."
  (interactive 
   (let* ((default (cperl-word-at-point))
	  (read (read-string 
		     (format "Find doc for Perl function (default %s): " 
			     default))))
     (list (if (equal read "") 
		   default 
		 read))))

  (let ((buffer (current-buffer))
	(cmd-desc (concat "^" (regexp-quote command) "[^a-zA-Z_0-9]")) ; "tr///"
	pos)
    (if (string-match "^-[a-zA-Z]$" command)
	(setq cmd-desc "^-X[ \t\n]"))
    (set-buffer (cperl-info-buffer))
    (beginning-of-buffer)
    (re-search-forward "^-X[ \t\n]")
    (forward-line -1)
    (if (re-search-forward cmd-desc nil t)
	(progn
	  (setq pos (progn (beginning-of-line)
			   (point)))
	  (pop-to-buffer (cperl-info-buffer))
	  (set-window-start (selected-window) pos))
      (message "No entry for %s found." command))
    (pop-to-buffer buffer)))

(defun cperl-info-on-current-command ()
  "Shows documentation for Perl command at point in other window."
  (interactive)
  (cperl-info-on-command (cperl-word-at-point)))

(defun cperl-imenu-info-imenu-search ()
  (if (looking-at "^-X[ \t\n]") nil
    (re-search-backward
     "^\n\\([-a-zA-Z]+\\)[ \t\n]")
    (forward-line 1)))

(defun cperl-imenu-info-imenu-name ()  
  (buffer-substring
   (match-beginning 1) (match-end 1)))

(defun cperl-imenu-on-info ()
  (interactive)
  (let* ((buffer (current-buffer))
	 imenu-create-index-function
	 imenu-prev-index-position-function 
	 imenu-extract-index-name-function 
	 (index-item (save-restriction
		       (save-window-excursion
			 (set-buffer (cperl-info-buffer))
			 (setq imenu-create-index-function 
			       'imenu-default-create-index-function
			       imenu-prev-index-position-function
			       'cperl-imenu-info-imenu-search
			       imenu-extract-index-name-function
			       'cperl-imenu-info-imenu-name)
			 (imenu-choose-buffer-index)))))
    (and index-item
	 (progn
	   (push-mark)
	   (pop-to-buffer "*info-perl*")
	   (cond
	    ((markerp (cdr index-item))
	     (goto-char (marker-position (cdr index-item))))
	    (t
	     (goto-char (cdr index-item))))
	   (set-window-start (selected-window) (point))
	   (pop-to-buffer buffer)))))

(defun cperl-lineup (beg end &optional step minshift)
  "Lineup construction in a region.
Beginning of region should be at the start of a construction.
All first occurences of this construction in the lines that are
partially contained in the region are lined up at the same column.

MINSHIFT is the minimal amount of space to insert before the construction.
STEP is the tabwidth to position constructions.
If STEP is `nil', `cperl-lineup-step' will be used 
\(or `cperl-indent-level', if `cperl-lineup-step' is `nil').
Will not move the position at the start to the left."
  (interactive "r")
  (let (search col tcol seen b e)
    (save-excursion
      (goto-char end)
      (end-of-line)
      (setq end (point-marker))
      (goto-char beg)
      (skip-chars-forward " \t\f")
      (setq beg (point-marker))
      (indent-region beg end nil)
      (goto-char beg)
      (setq col (current-column))
      (if (looking-at "\\sw")
	  (if (looking-at "\\<\\sw+\\>")
	      (setq search
		    (concat "\\<" 
			    (regexp-quote 
			     (buffer-substring (match-beginning 0)
					       (match-end 0))) "\\>"))
	    (error "Cannot line up in a middle of the word"))
	(if (looking-at "$")
	    (error "Cannot line up end of line"))
	(setq search (regexp-quote (char-to-string (following-char)))))
      (setq step (or step cperl-lineup-step cperl-indent-level))
      (or minshift (setq minshift 1))
      (while (progn
	       (beginning-of-line 2)
	       (and (< (point) end) 
		    (re-search-forward search end t)
		    (goto-char (match-beginning 0))))
	(setq tcol (current-column) seen t)
	(if (> tcol col) (setq col tcol)))
      (or seen
	  (error "The construction to line up occured only once"))
      (goto-char beg)
      (setq col (+ col minshift))
      (if (/= (% col step) 0) (setq step (* step (1+ (/ col step)))))
      (while 
	  (progn
	    (setq e (point))
	    (skip-chars-backward " \t")
	    (delete-region (point) e)
	    (indent-to-column col); (make-string (- col (current-column)) ?\ ))
	    (beginning-of-line 2) 
	    (and (< (point) end) 
		 (re-search-forward search end t)
		 (goto-char (match-beginning 0)))))))) ; No body

(defun cperl-etags (&optional add all files)
  "Run etags with appropriate options for Perl files.
If optional argument ALL is `recursive', will process Perl files
in subdirectories too."
  (interactive)
  (let ((cmd "etags")
	(args '("-l" "none" "-r" "/\\<\\(package\\|sub\\)[ \\t]+\\(\\([a-zA-Z0-9:_]*::\\)?\\([a-zA-Z0-9_]+\\)[ \\t]*\\([{#]\\|$\\)\\)/\\4/"))
	res)
    (if add (setq args (cons "-a" args)))
    (or files (setq files (list buffer-file-name)))
    (cond
     ((eq all 'recursive)
      ;;(error "Not implemented: recursive")
      (setq args (append (list "-e" 
			       "sub wanted {push @ARGV, $File::Find::name if /\\.[Pp][Llm]$/}
				use File::Find;
				find(\\&wanted, '.');
				exec @ARGV;" 
			       cmd) args)
	    cmd "perl"))
     (all 
      ;;(error "Not implemented: all")
      (setq args (append (list "-e" 
			       "push @ARGV, <*.PL *.pl *.pm>;
				exec @ARGV;" 
			       cmd) args)
	    cmd "perl"))
     (t
      (setq args (append args files))))
    (setq res (apply 'call-process cmd nil nil nil args))
    (or (eq res 0)
	(message "etags returned \"%s\"" res))))
