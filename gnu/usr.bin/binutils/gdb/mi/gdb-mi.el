;;; gdb-mi.el (internally gdbmi6.el) - (24th May 2004)

;; Run gdb with GDB/MI (-interp=mi) and access CLI using "cli-command"
;; (could use "-interpreter-exec console cli-command")

;; Author: Nick Roberts <nickrob@gnu.org>
;; Maintainer: Nick Roberts <nickrob@gnu.org>
;; Keywords: unix, tools

;; Copyright (C) 2004  Free Software Foundation, Inc.

;; This file is part of GNU GDB.

;; GNU GDB is free software; you can redistribute it and/or modify
;; it under the terms of the GNU General Public License as published by
;; the Free Software Foundation; either version 2, or (at your option)
;; any later version.

;; This program is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;; GNU General Public License for more details.

;;; Commentary:

;; This mode acts as a graphical user interface to GDB and requires GDB 6.1
;; onwards. You can interact with GDB through the GUD buffer in the usual way,
;; but there are also buffers which control the execution and describe the
;; state of your program. It separates the input/output of your program from
;; that of GDB and displays expressions and their current values in their own
;; buffers. It also uses features of Emacs 21 such as the fringe/display
;; margin for breakpoints, and the toolbar (see the GDB Graphical Interface
;; section in the Emacs info manual).

;; Start the debugger with M-x gdbmi.

;; This file uses GDB/MI as the primary interface to GDB. It is still under
;; development and is part of a process to migrate Emacs from annotations
;; (as used in gdb-ui.el) to GDB/MI.

;; Known Bugs:
;;

;;; Code:

(require 'gud)
(require 'gdb-ui)


;;;###autoload
(defun gdbmi (command-line)
  "Run gdb on program FILE in buffer *gud-FILE*.
The directory containing FILE becomes the initial working directory
and source-file directory for your debugger.

If `gdb-many-windows' is nil (the default value) then gdb just
pops up the GUD buffer unless `gdb-show-main' is t. In this case
it starts with two windows: one displaying the GUD buffer and the
other with the source file with the main routine of the inferior.

If `gdb-many-windows' is t, regardless of the value of
`gdb-show-main', the layout below will appear. Keybindings are
given in relevant buffer.

Watch expressions appear in the speedbar/slowbar.

The following interactive lisp functions help control operation :

`gdb-many-windows'    - Toggle the number of windows gdb uses.
`gdb-restore-windows' - To restore the window layout.

See Info node `(emacs)GDB Graphical Interface' for a more
detailed description of this mode.


---------------------------------------------------------------------
                               GDB Toolbar
---------------------------------------------------------------------
GUD buffer (I/O of GDB)           | Locals buffer
                                  |
                                  |
                                  |
---------------------------------------------------------------------
 Source buffer                    | Input/Output (of inferior) buffer
                                  | (comint-mode)
                                  |
                                  |
                                  |
                                  |
                                  |
                                  |
---------------------------------------------------------------------
 Stack buffer                     | Breakpoints buffer
 RET      gdb-frames-select       | SPC    gdb-toggle-breakpoint
                                  | RET    gdb-goto-breakpoint
                                  |   d    gdb-delete-breakpoint
---------------------------------------------------------------------
"
  ;;
  (interactive (list (gud-query-cmdline 'gdbmi)))
  ;;
  ;; Let's start with a basic gud-gdb buffer and then modify it a bit.
  (gdb command-line)
  ;;
  (setq gdb-debug-log nil)
  (set (make-local-variable 'gud-minor-mode) 'gdbmi)
  (set (make-local-variable 'gud-marker-filter) 'gud-gdbmi-marker-filter)
  ;;
  (gud-def gud-break (if (not (string-equal mode-name "Machine"))
			 (gud-call "-break-insert %f:%l" arg)
		       (save-excursion
			 (beginning-of-line)
			 (forward-char 2)
			 (gud-call "-break-insert *%a" arg)))
	   "\C-b" "Set breakpoint at current line or address.")
  ;;
  (gud-def gud-remove (if (not (string-equal mode-name "Machine"))
			  (gud-call "clear %f:%l" arg)
			(save-excursion
			  (beginning-of-line)
			  (forward-char 2)
			  (gud-call "clear *%a" arg)))
	   "\C-d" "Remove breakpoint at current line or address.")
  ;;
  (gud-def gud-until  (if (not (string-equal mode-name "Machine"))
			  (gud-call "until %f:%l" arg)
			(save-excursion
			  (beginning-of-line)
			  (forward-char 2)
			  (gud-call "until *%a" arg)))
	   "\C-u" "Continue to current line or address.")

  (define-key gud-minor-mode-map [left-margin mouse-1]
    'gdb-mouse-toggle-breakpoint)
  (define-key gud-minor-mode-map [left-fringe mouse-1]
    'gdb-mouse-toggle-breakpoint)

  (setq comint-input-sender 'gdbmi-send)
  ;;
  ;; (re-)initialise
  (setq gdb-main-file nil)
  (setq gdb-current-address "main")
  (setq gdb-previous-address nil)
  (setq gdb-previous-frame nil)
  (setq gdb-current-frame "main")
  (setq gdb-view-source t)
  (setq gdb-selected-view 'source)
  (setq gdb-var-list nil)
  (setq gdb-var-changed nil)
  (setq gdb-prompting nil)
  (setq gdb-current-item nil)
  (setq gdb-pending-triggers nil)
  (setq gdb-output-sink 'user)
  (setq gdb-server-prefix nil)
  ;;
  (setq gdb-buffer-type 'gdbmi)
  ;;
  ;; FIXME: use tty command to separate io.
  ;;(gdb-clear-inferior-io)
  ;;
  (if (eq window-system 'w32)
      (gdb-enqueue-input (list "-gdb-set new-console off\n" 'ignore)))
  ;; find source file and compilation directory here
  (gdb-enqueue-input (list "list main\n"   'ignore))   ; C program
  (gdb-enqueue-input (list "list MAIN__\n" 'ignore))   ; Fortran program
  (gdb-enqueue-input (list "info source\n" 'gdbmi-source-info))
  ;;
  (run-hooks 'gdbmi-mode-hook))

; Force nil till fixed.
(defconst gdbmi-use-inferior-io-buffer nil)

; uses --all-values Needs GDB 6.1 onwards.
(defun gdbmi-var-list-children (varnum)
  (gdb-enqueue-input
   (list (concat "-var-update " varnum "\n") 'ignore))
  (gdb-enqueue-input
   (list (concat "-var-list-children --all-values "  
		 varnum "\n")
	     `(lambda () (gdbmi-var-list-children-handler ,varnum)))))

(defconst gdbmi-var-list-children-regexp
"name=\"\\(.*?\\)\",exp=\"\\(.*?\\)\",numchild=\"\\(.*?\\)\",value=\"\\(.*?\\)\""
)

(defun gdbmi-var-list-children-handler (varnum)
  (with-current-buffer (gdb-get-create-buffer 'gdb-partial-output-buffer)
    (goto-char (point-min))
    (let ((var-list nil))
     (catch 'child-already-watched
       (dolist (var gdb-var-list)
	 (if (string-equal varnum (cadr var))
	     (progn
	       (push var var-list)
	       (while (re-search-forward gdbmi-var-list-children-regexp nil t)
		 (let ((varchild (list (match-string 2)
				       (match-string 1)
				       (match-string 3)
				       nil
				       (match-string 4)
				       nil)))
		   (if (looking-at ",type=\"\\(.*?\\)\"")
		       (setcar (nthcdr 3 varchild) (match-string 1)))
		   (dolist (var1 gdb-var-list)
		     (if (string-equal (cadr var1) (cadr varchild))
			 (throw 'child-already-watched nil)))
		   (push varchild var-list))))
	   (push var var-list)))
       (setq gdb-var-changed t)
       (setq gdb-var-list (nreverse var-list))))))

;(defun gdbmi-send (proc string)
;  "A comint send filter for gdb."
;  (setq gdb-output-sink 'user)
;  (setq gdb-prompting nil)
;  (process-send-string proc (concat "-interpreter-exec console \"" string "\"")))

(defun gdbmi-send (proc string)
  "A comint send filter for gdb."
  (setq gdb-output-sink 'user)
  (setq gdb-prompting nil)
  (process-send-string proc (concat string "\n")))

(defcustom gud-gdbmi-command-name "~/gdb/gdb/gdb -interp=mi"
  "Default command to execute an executable under the GDB-UI debugger."
  :type 'string
  :group 'gud)

(defconst gdb-stopped-regexp 
  "\\((gdb) \n\\*stopped\\|^\\^done\\),reason=.*,file=\"\\(.*\\)\",line=\"\\(.*\\)\".*")

(defconst gdb-console-regexp "~\"\\(.*\\)\\\\n\"")

(defconst gdb-internals-regexp "&\".*\\n\"\n")

(defconst gdb-gdb-regexp "(gdb) \n")

(defconst gdb-running-regexp "^\\^running")

(defun gdbmi-prompt ()
  "This handler terminates the any collection of output. It also
  sends the next command (if any) to gdb."
  (unless gdb-pending-triggers
	(gdb-get-current-frame)
	(gdbmi-invalidate-frames)
	(gdbmi-invalidate-breakpoints)
	(gdbmi-invalidate-locals)
	(dolist (frame (frame-list))
	  (when (string-equal (frame-parameter frame 'name) "Speedbar")
	    (setq gdb-var-changed t)    ; force update
	    (dolist (var gdb-var-list)
	      (setcar (nthcdr 5 var) nil))))
	(gdb-var-update))
  (let ((sink gdb-output-sink))
    (when (eq sink 'emacs)
      (let ((handler
	     (car (cdr gdb-current-item))))
	(with-current-buffer (gdb-get-create-buffer 'gdb-partial-output-buffer)
	  (funcall handler)))))
  (let ((input (gdb-dequeue-input)))
    (if input
	(gdb-send-item input)
      (progn
	(setq gud-running nil)
	(setq gdb-prompting t)
	(gud-display-frame)))))

(defun gud-gdbmi-marker-filter (string)
  "Filter GDB/MI output."
  (if gdb-enable-debug-log (push (cons 'recv string) gdb-debug-log))
  ;; Recall the left over gud-marker-acc from last time
  (setq gud-marker-acc (concat gud-marker-acc string))
  ;; Start accumulating output for the GUD buffer
  (let ((output ""))

    (if (string-match gdb-running-regexp gud-marker-acc) 
       (setq gud-marker-acc (substring gud-marker-acc (match-end 0))
	     gud-running t))

    ;; Remove the trimmings from the console stream.
    (while (string-match gdb-console-regexp gud-marker-acc) 
       (setq 
	gud-marker-acc (concat (substring gud-marker-acc 0 (match-beginning 0))
			       (match-string 1 gud-marker-acc)
			       (substring gud-marker-acc (match-end 0)))))

    ;; Remove log stream containing debugging messages being produced by GDB's
    ;; internals.
    (while (string-match gdb-internals-regexp gud-marker-acc) 
       (setq 
	 gud-marker-acc (concat (substring gud-marker-acc 0 (match-beginning 0))
				(substring gud-marker-acc (match-end 0)))))

    (if (string-match gdb-stopped-regexp gud-marker-acc)
      (setq

       ;; Extract the frame position from the marker.
       gud-last-frame (cons (match-string 2 gud-marker-acc)
			    (string-to-int (match-string 3 gud-marker-acc)))

       ;; Append any text before the marker to the output we're going
       ;; to return - we don't include the marker in this text.
       output (gdbmi-concat-output output
		      (substring gud-marker-acc 0 (match-beginning 0)))

       ;; Set the accumulator to the remaining text.
       gud-marker-acc (substring gud-marker-acc (match-end 0))))
      
    (while (string-match gdb-gdb-regexp gud-marker-acc) 
      (setq

       ;; Append any text up to and including prompt less \n to the output.
       output (gdbmi-concat-output output
		      (substring gud-marker-acc 0 (- (match-end 0) 1)))

       ;; Set the accumulator to the remaining text.
       gud-marker-acc (substring gud-marker-acc (match-end 0)))
      (gdbmi-prompt))

    (setq output (gdbmi-concat-output output gud-marker-acc))
    (setq gud-marker-acc "")
    output))

(defun gdbmi-concat-output (so-far new)
  (let ((sink gdb-output-sink))
    (cond
     ((eq sink 'user) (concat so-far new))
     ((eq sink 'emacs)
      (gdb-append-to-partial-output new)
      so-far)
     ((eq sink 'inferior)
      (gdb-append-to-inferior-io new)
      so-far))))


;; Breakpoint buffer : This displays the output of `-break-list'.
;;
(def-gdb-auto-updated-buffer gdb-breakpoints-buffer
  ;; This defines the auto update rule for buffers of type
  ;; `gdb-breakpoints-buffer'.
  ;;
  ;; It defines a function that queues the command below.  That function is
  ;; called:
  gdbmi-invalidate-breakpoints
  ;;
  ;; To update the buffer, this command is sent to gdb.
  "-break-list\n"
  ;;
  ;; This also defines a function to be the handler for the output
  ;; from the command above.  That function will copy the output into
  ;; the appropriately typed buffer.  That function will be called:
  gdb-break-list-handler
  ;; buffer specific functions
  gdb-break-list-custom)

(defconst gdb-break-list-regexp
"number=\"\\(.*?\\)\",type=\"\\(.*?\\)\",disp=\"\\(.*?\\)\",enabled=\"\\(.\\)\",addr=\"\\(.*?\\)\",func=\"\\(.*?\\)\",file=\"\\(.*?\\)\",line=\"\\(.*?\\)\"")

(defun gdb-break-list-handler ()
  (setq gdb-pending-triggers (delq 'gdbmi-invalidate-breakpoints
				  gdb-pending-triggers))
  (let ((breakpoint nil)
	(breakpoints-list nil))
    (with-current-buffer (gdb-get-create-buffer 'gdb-partial-output-buffer)
      (goto-char (point-min))
      (while (re-search-forward gdb-break-list-regexp nil t)
	(let ((breakpoint (list (match-string 1)
				(match-string 2)
				(match-string 3)
				(match-string 4)
				(match-string 5)
				(match-string 6)
				(match-string 7)
				(match-string 8))))
	  (push breakpoint breakpoints-list))))
    (let ((buf (gdb-get-buffer 'gdb-breakpoints-buffer)))
      (and buf (with-current-buffer buf
		 (let ((p (point))
		       (buffer-read-only nil))
		   (erase-buffer)
		   (insert "Num Type        Disp Enb Func\tFile:Line\tAddr\n")
		   (dolist (breakpoint breakpoints-list)
		     (insert (concat
			      (nth 0 breakpoint) "   "
			      (nth 1 breakpoint) "  "
			      (nth 2 breakpoint) "   "
			      (nth 3 breakpoint) " "
			      (nth 5 breakpoint) "\t"
			      (nth 6 breakpoint) ":" (nth 7 breakpoint) "\t" 
			      (nth 4 breakpoint) "\n")))
		   (goto-char p))))))
  (gdb-break-list-custom))

;;-put breakpoint icons in relevant margins (even those set in the GUD buffer)
(defun gdb-break-list-custom ()
  (let ((flag)(address))
    ;;
    ;; remove all breakpoint-icons in source buffers but not assembler buffer
    (dolist (buffer (buffer-list))
      (with-current-buffer buffer
	(if (and (eq gud-minor-mode 'gdbmi)
		 (not (string-match "\\`\\*.+\\*\\'" (buffer-name))))
	    (gdb-remove-breakpoint-icons (point-min) (point-max)))))
    (with-current-buffer (gdb-get-buffer 'gdb-breakpoints-buffer)
      (save-excursion
	(goto-char (point-min))
	(while (< (point) (- (point-max) 1))
	  (forward-line 1)
	  (if (looking-at "[0-9]*\\s-*\\S-*\\s-*\\S-*\\s-*\\(.\\)\\s-*\\S-*\\s-*\\(\\S-*\\):\\([0-9]+\\)")
	      (progn
		(setq flag (char-after (match-beginning 1)))
		(let ((line (match-string 3)) (buffer-read-only nil)
		      (file (match-string 2)))
		  (add-text-properties (point-at-bol) (point-at-eol)
				       '(mouse-face highlight
						    help-echo "mouse-2, RET: visit breakpoint"))
		  (with-current-buffer
		      (find-file-noselect
		       (if (file-exists-p file) file
			 (expand-file-name file gdb-cdir)))
		    (save-current-buffer
		      (set (make-local-variable 'gud-minor-mode) 'gdbmi)
		      (set (make-local-variable 'tool-bar-map)
			   gud-tool-bar-map))
		    ;; only want one breakpoint icon at each location
		    (save-excursion
		      (goto-line (string-to-number line))
		      (gdb-put-breakpoint-icon (eq flag ?y)))))))))
	  (end-of-line)))
  (if (gdb-get-buffer 'gdb-assembler-buffer) (gdb-assembler-custom)))

;; Frames buffer.  This displays a perpetually correct bactrack trace.
;;
(def-gdb-auto-updated-buffer gdb-stack-buffer
  gdbmi-invalidate-frames
  "-stack-list-frames\n"
  gdb-stack-list-frames-handler
  gdb-stack-list-frames-custom)

(defconst gdb-stack-list-frames-regexp
"level=\"\\(.*?\\)\",addr=\"\\(.*?\\)\",func=\"\\(.*?\\)\",file=\"\\(.*?\\)\",line=\"\\(.*?\\)\"")

(defun gdb-stack-list-frames-handler ()
  (setq gdb-pending-triggers (delq 'gdbmi-invalidate-frames
				  gdb-pending-triggers))
  (let ((frame nil)
	(call-stack nil))
    (with-current-buffer (gdb-get-create-buffer 'gdb-partial-output-buffer)
      (goto-char (point-min))
      (while (re-search-forward gdb-stack-list-frames-regexp nil t)
	(let ((frame (list (match-string 1)
			   (match-string 2)
			   (match-string 3)
			   (match-string 4)
			   (match-string 5))))
	  (push frame call-stack))))
    (let ((buf (gdb-get-buffer 'gdb-stack-buffer)))
      (and buf (with-current-buffer buf
		 (let ((p (point))
		       (buffer-read-only nil))
		   (erase-buffer)
		   (insert "Level\tFunc\tFile:Line\tAddr\n")
		   (dolist (frame (nreverse call-stack))
		     (insert (concat
			      (nth 0 frame) "\t"
			      (nth 2 frame) "\t"
			      (nth 3 frame) ":" (nth 4 frame) "\t"
			      (nth 1 frame) "\n")))
		   (goto-char p))))))
  (gdb-stack-list-frames-custom))

(defun gdb-stack-list-frames-custom ()
  (with-current-buffer (gdb-get-buffer 'gdb-stack-buffer)
    (save-excursion
      (let ((buffer-read-only nil))
	(goto-char (point-min))
	(forward-line 1)
	(while (< (point) (point-max))
	  (add-text-properties (point-at-bol) (point-at-eol)
			     '(mouse-face highlight
			       help-echo "mouse-2, RET: Select frame"))
	  (beginning-of-line)
	  (when (and (or (looking-at "^#[0-9]*\\s-*\\S-* in \\(\\S-*\\)")
			 (looking-at "^#[0-9]*\\s-*\\(\\S-*\\)"))
		     (equal (match-string 1) gdb-current-frame))
	    (put-text-property (point-at-bol) (point-at-eol)
			       'face '(:inverse-video t)))
	  (forward-line 1))))))

;; Locals buffer.
;; uses "-stack-list-locals 2". Needs GDB 6.1 onwards.
(def-gdb-auto-updated-buffer gdb-locals-buffer
  gdbmi-invalidate-locals
  "-stack-list-locals 2\n"
  gdb-stack-list-locals-handler
  gdb-stack-list-locals-custom)

(defconst gdb-stack-list-locals-regexp
  (concat "name=\"\\(.*?\\)\",type=\"\\(.*?\\)\""))

;; Dont display values of arrays or structures.
;; These can be expanded using gud-watch.
(defun gdb-stack-list-locals-handler nil
  (setq gdb-pending-triggers (delq 'gdbmi-invalidate-locals
				  gdb-pending-triggers))
  (let ((local nil)
	(locals-list nil))
    (with-current-buffer (gdb-get-create-buffer 'gdb-partial-output-buffer)
      (goto-char (point-min))
      (while (re-search-forward gdb-stack-list-locals-regexp nil t)
	(let ((local (list (match-string 1)
			   (match-string 2)
			   nil)))
	  (if (looking-at ",value=\"\\(.*?\\)\"")
	      (setcar (nthcdr 2 local) (match-string 1)))
	(push local locals-list))))
    (let ((buf (gdb-get-buffer 'gdb-locals-buffer)))
      (and buf (with-current-buffer buf
		 (let ((p (point))
		       (buffer-read-only nil))
		   (erase-buffer)
		   (dolist (local locals-list)
		     (insert 
		      (concat (car local) "\t" (nth 1 local) "\t"
			      (or (nth 2 local)
				  (if (string-match "struct" (nth 1 local))
				      "(structure)"
				    "(array)"))
			      "\n")))
		   (goto-char p)))))))

(defun gdb-stack-list-locals-custom ()
  nil)

(defun gdbmi-source-info ()
  "Find the source file where the program starts and displays it with related
buffers."
  (goto-char (point-min))
  (if (search-forward "source file is " nil t)
      (if (looking-at "\\S-*")
	  (setq gdb-main-file (match-string 0)))
    (setq gdb-view-source nil))
  (if (search-forward "directory is " nil t)
      (if (looking-at "\\S-*:\\(\\S-*\\)")
	  (setq gdb-cdir (match-string 1))
	(looking-at "\\S-*")
	(setq gdb-cdir (match-string 0))))

;temporary heuristic
  (if gdb-main-file
      (setq gdb-main-file (expand-file-name gdb-main-file gdb-cdir)))

  (if gdb-many-windows
      (gdb-setup-windows)
    (gdb-get-create-buffer 'gdb-breakpoints-buffer)
    (when gdb-show-main
      (switch-to-buffer gud-comint-buffer)
      (delete-other-windows)
      (split-window)
      (other-window 1)
      (switch-to-buffer
       (if gdb-view-source
	   (gud-find-file gdb-main-file)
	 (gdb-get-create-buffer 'gdb-assembler-buffer)))
      (other-window 1))))

(provide 'gdb-mi)
;;; gdbmi.el ends here
