(use json list-of srfi-13 getopt-long posix)

(define (json-fix jro)
  (cond ((null? jro) jro)
        ((vector? jro)
         (alist->hash-table (json-fix (vector->list jro))))
        ((pair? jro)
         (cons (json-fix (car jro))
               (json-fix (cdr jro))))
        (else jro)))

(define (json->scm port)
  (json-fix (json-read port)))

(define (scm->json obj port)
  (json-write obj port))

(define (scm->json-string obj)
  (call-with-output-string
    (lambda (port)
      (scm->json obj port))))

(define (mapcat f l)
  (foldr append '() (map f l)))

(define (partial f . args)
  (lambda more (apply f (append args more))))

(define (jt-array ins ops table args)
  (let ((jt* (lambda (x) (jt (cons x (cdr ins)) ops '(()) args))))
    (map (partial apply append)
         (list-of (list i j) (i in table) (j in (mapcat jt* (car ins)))))))

(define (jt-struct-skip ins ops table args)
  (jt ins ops table (cddr args)))

(define (jt-struct-u ins ops table args)
  (jt (cdr ins) ops table (cdr args)))

(define (jt-struct-p ins ops table args)
  (jt ins ops (map (lambda (x) (append x (list (car ins)))) table) (cdr args)))

(define (jt-struct-q ins ops table args)
  (let ((json (scm->json-string (scm->json-string (car ins))))
        (ops* (cons (cons "thing" (car ops)) (cdr ops))))
    (jt (cons json ins) ops* table (cdr args))))

(define (jt-struct-d ins ops table args)
  (let ((val (hash-table-ref (car ins) (cadr args)))
        (ops* (cons (cons "thing" (car ops)) (cdr ops))))
    (jt (cons val ins) ops* table (cddr args))))

(define (jt-struct-k ins ops table args)
  (write-line (string-join (sort (hash-table-keys (car ins)) string<=) "\n"))
  (exit 0))

(define (jt-struct-lb ins ops table args)
  (let ((os (if (equal? '() (car ops))
              ops
              (cons (cons 'no-print (car ops)) (cdr ops)))))
    (jt ins (cons '() os) table (cdr args))))

(define (jt-struct-rb ins ops table args)
  (let ((ps (if (string? (caar ops)) '("-p") '()))
        (us (list-of "-u" (i in (filter string? (car ops))))))
    (jt ins (cdr ops) table (append ps us (cdr args)))))

(define (jt-struct-prop ins ops table args)
  (jt ins ops table (cons "-d" args)))

(define (jt-struct ins ops table args)
  (let ((a (car args)))
    ((cond
       ((or (equal? "-k" a) (equal? "--keys"         a)) jt-struct-k)
       ((or (equal? "-u" a) (equal? "--up"           a)) jt-struct-u)
       ((or (equal? "-p" a) (equal? "--print"        a)) jt-struct-p)
       ((or (equal? "-q" a) (equal? "--quote"        a)) jt-struct-q)
       ((or (equal? "-d" a) (equal? "--down"         a)) jt-struct-d)
       ((or (equal? "-g" a) (equal? "--gosub"        a)) jt-struct-lb)
       ((or (equal? "-r" a) (equal? "--return"       a)) jt-struct-rb)
       ((or (equal? "["  a))                             jt-struct-lb)
       ((or (equal? "]"  a))                             jt-struct-rb)
       ((or (equal? "-F" a) (equal? "--field-delim"  a)) jt-struct-skip)
       ((or (equal? "-R" a) (equal? "--record-delim" a)) jt-struct-skip)
       (else            jt-struct-prop)) ins ops table args)))

(define (jt ins ops table args)
  (cond
    ((equal? '() args) table)
    ((list? (car ins)) (jt-array ins ops table args))
    (else              (jt-struct ins ops table args))))

(define (json-table port args)
  (jt (list (json->scm port)) '(()) '(()) args))

(define (str x)
  (cond
    ((string?  x) x)
    ((number?  x) (number->string x))
    (else (call-with-output-string (partial scm->json x)))))

(define (print-row fs rs row)
  (display (string-append (string-join (map str row) fs) rs)))

(define (print-table table fs rs)
  (for-each (partial print-row fs rs) table))

(define optspec
  '((help
      "Print usage info and exit."
      (single-char #\h))
    (field-delim
      "Set the output field separator character (default is tab)."
      (single-char #\F)
      (value (required C)))
    (record-delim
      "Set the output record separator character (default is newline)."
      (single-char #\R)
      (value (required C)))
    (keys
      "Print the keys of the object on the top of the JSON stack and exit."
      (single-char #\k))
    (down
      "Push the value of x.PROP onto the JSON stack, where x is the top item."
      (single-char #\d)
      (value (required PROP)))
    (up
      "Pop the top item off of the JSON stack."
      (single-char #\u))
    (print
      "Append the top item on the JSON stack to the current row's output stack."
      (single-char #\p))
    (quote
      "JSON encode the item on the top of the JSON stack and push it onto the stack."
      (single-char #\q))
    (gosub
      "Push the current JSON stack pointer onto the GOSUB stack."
      (single-char #\g))
    (return
      "Pop the JSON stack up to where the top of the GOSUB stack points, and pop the GOSUB stack."
      (single-char #\r))))

(define usage-body #<<EOT
NAME

  jt - transform JSON data to tabular format

SYNOPSIS

  jt [-h|--help]
  jt [-F <char>] [-R <char>] [-d <prop>|-kpqu]...
EOT
)

(define (usage*)
  (newline)
  (write-line usage-body)
  (newline)
  (write-line "OPTIONS")
  (newline)
  (write-line (usage optspec))
  (newline))

(let* ((args (command-line-arguments))
       (opts (getopt-long args optspec))
       (fs   (or (alist-ref 'field-delim opts) "\t"))
       (rs   (or (alist-ref 'record-delim opts) "\n")))
  (if (alist-ref 'help opts)
    (usage*)
    (unless (terminal-port? (current-input-port))
      (print-table (json-table (current-input-port) args) fs rs))))
