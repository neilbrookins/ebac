#!/bin/sh
# add-gutenberg.sh -- add a text from Project Gutenberg to EBaC, by the
#                     address of its plain text (or remove one added so).
#
#   sh add-gutenberg.sh                ask for the address (EBaC's P)
#   sh add-gutenberg.sh URL            add the text at URL
#   sh add-gutenberg.sh --agree URL    the same, agreeing to its license (for
#                                      scripts: no one to answer)
#   sh add-gutenberg.sh --remove BOM   remove one added by this script
#   sh add-gutenberg.sh --list         list those added by this script
#   sh add-gutenberg.sh --file PATH URL   read the text from PATH instead of
#                                      downloading it (for testing)
#
# The person adding a text finds it on gutenberg.org themselves, opens its
# "Plain Text UTF-8" file in a browser, checks it is the one, and gives its
# address here: https://www.gutenberg.org/cache/epub/17/pg17.txt (or
# .../ebooks/17.txt.utf-8, or .../files/1/2/3/4/12345/12345-0.txt for a
# larger number). Nothing is searched or listed here: what is added is
# their own choice. Then, as add-translation.sh does for scrollmapper's
# texts, this:
#
#   1. downloads it (the one address given; refused first, by its
#      fingerprint, if it is the same version as the copy added before:
#      remove that first);
#   2. shows its license (Gutenberg's, from the file's header: so it is
#      downloaded first) and asks whether you agree;
#   3. splits it into a file per book, in a folder named by its short name
#      (BOM), with booklist, booknames, license and version files;
#   4. builds its concordance index, and checks it has every verse;
#   5. adds it to the top of 'trans', so that EBaC starts with it.
#
# One text, and one format, at a time (see "Texts from Project Gutenberg"
# in readme.architecture). Gutenberg's texts are laid out in many ways, so
# each is known by its ebook number, from the list below: its short name,
# its books (their names, and the headings that start them), and how its
# front matter is divided. A text not in the list is refused. Each new one
# is looked at by hand and added to the list (with what its layout needs).
#
# Nothing the text gives is left out: its front matter (title page,
# testimonies, contents), each book's heading and summary, and its
# verses. Only Gutenberg's own header and license around the text, and the
# lines that number the chapters ("Jacob Chapter 1"), are dropped. The
# text before a book's first chapter is its chapter 0, a verse for each
# part; a book's heading and summary are verse 0 of its chapter 1 (EBaC
# shows both as plain text, without the 0s, which aren't in the text).
#
# This script adds and removes only its own texts (those whose folder has
# a file named gutenberg); add-translation.sh, scrollmapper's.
#
# EBaC runs this itself for P)roject Gutenberg on its main menu, as "sh
# add-gutenberg.sh --from-ebac PROGRAM NAME", and is started again when it
# is done, as for add-translation.sh.

LC_ALL=C
export LC_ALL
set -f

here=$(cd "$(dirname "$0")" && pwd)

if command -v nawk > /dev/null 2>&1; then
        AWK=nawk
else
        AWK=awk
fi

# ---- The texts this script knows ----------------------------------------
#
# A line per text: its ebook number, short name, format; then a line per
# book: "book", its name (as EBaC shows it), and the start of the heading
# that begins it in the text; then a line per part of its front matter
# (its chapter 0): "front", and the start of the line that begins the
# part.
#
# Formats:
#   chapters   books start with their heading (and a summary); a book of
#              several chapters has a line per chapter, "Alma Chapter 5";
#              verses are "C:V text", wrapped, a blank line between them,
#              and may start in the middle of a line, with the book's name
#              before them or not ("...them. 2:13 Neither", "4 Nephi 1:1").
KNOWN='
17	BOM	chapters
book	1 Nephi	THE FIRST BOOK OF NEPHI
book	2 Nephi	THE SECOND BOOK OF NEPHI
book	Jacob	THE BOOK OF JACOB
book	Enos	THE BOOK OF ENOS
book	Jarom	THE BOOK OF JAROM
book	Omni	THE BOOK OF OMNI
book	Words of Mormon	THE WORDS OF MORMON
book	Mosiah	THE BOOK OF MOSIAH
book	Alma	THE BOOK OF ALMA
book	Helaman	THE BOOK OF HELAMAN
book	3 Nephi	THIRD BOOK OF NEPHI
book	4 Nephi	FOURTH NEPHI
book	Mormon	THE BOOK OF MORMON
book	Ether	THE BOOK OF ETHER
book	Moroni	THE BOOK OF MORONI
front	THE BOOK OF MORMON
front	Wherefore, it is an abridgment
front	An abridgment taken from
front	TRANSLATED BY
front	THE TESTIMONY OF THREE WITNESSES
front	THE TESTIMONY OF EIGHT WITNESSES
front	Contents
'

# Typographic punctuation as plain ASCII, and white space as single spaces
# (as add-translation.sh does: EBaC's own text is ASCII punctuation).
PLAIN='
function plain(s) {
        if (index(s, "\342\200")) {
                gsub(/\342\200\230|\342\200\231/, "\047", s)     # curly single quotes
                gsub(/\342\200\234|\342\200\235/, "\"", s)       # curly double quotes
                gsub(/\342\200\224|\342\200\225/, "--", s)       # em dash, horizontal bar
                gsub(/\342\200\223/, "-", s)                     # en dash
                gsub(/\342\200\246/, "...", s)                   # ellipsis
        }
        gsub(/\302\240/, " ", s)                                 # no-break space
        gsub(/[\t\r\v\f]/, " ", s)
        gsub(/  +/, " ", s)
        sub(/^ /, "", s)
        sub(/ $/, "", s)
        return s
}'

tmp=""
adding=""
from_ebac=""
start_with=""
failed=0
problem=""

cleanup() {
        [ -n "$tmp" ] && rm -rf "$tmp"
        tmp=""
}

back_to_ebac() {
        cleanup
        trap - EXIT INT
        exec "$from_ebac" "$start_with"
        printf 'add-gutenberg: couldn\047t start EBaC again (%s).\n' "$from_ebac" >&2
        exit 1
}

interrupted() {
        [ -n "$adding" ] && [ -d "$adding" ] && rm -rf "$adding"
        echo
        if [ -n "$from_ebac" ]; then
                echo "Stopped."
                back_to_ebac
        fi
        cleanup
        exit 1
}

# Tell EBaC's web page in the Home Assistant app that a line is being
# typed (see add-translation.sh).
typing_mode() {
        [ -n "${EBAC_GESTURES:-}" ] && printf '\033]7766;%s\007' "${1:-input}"
        return 0
}

press_any_key() {
        printf '\nPress any key to go back to EBaC. '
        [ -n "${EBAC_GESTURES:-}" ] && printf '\033]7766;anykey\007'
        if [ -t 0 ] && command -v dd > /dev/null 2>&1 &&
           saved=$(stty -g 2> /dev/null); then
                stty -icanon -echo min 1 time 0 2> /dev/null
                dd bs=1 count=1 > /dev/null 2>&1
                stty min 0 time 1 2> /dev/null
                while [ "$(dd bs=64 count=1 2> /dev/null | wc -c)" -gt 0 ]; do
                        :
                done
                stty "$saved" 2> /dev/null
        else
                read -r answer
        fi
        echo
}

# known NUMBER: the text's line of KNOWN ("17<tab>BOM<tab>chapters"), or ""
known() {
        printf '%s\n' "$KNOWN" | $AWK -F '\t' -v n="$1" '
                $1 ~ /^[0-9]+$/ { this = ($1 == n); if (this) print; next }'
}

# known_detail NUMBER: its book and front lines
known_detail() {
        printf '%s\n' "$KNOWN" | $AWK -F '\t' -v n="$1" '
                $1 ~ /^[0-9]+$/ { this = ($1 == n); next }
                this && ($1 == "book" || $1 == "front") { print }'
}

# ebook_number URL: the text's ebook number, from any of Gutenberg's
# addresses of its plain text; "" if it isn't one
ebook_number() {
        printf '%s\n' "$1" | $AWK '{
                u = $0
                sub(/^[ \t]+/, "", u); sub(/[ \t]+$/, "", u)
                if (u !~ /^https?:\/\/(www\.)?gutenberg\.org\//) exit
                sub(/^https?:\/\/(www\.)?gutenberg\.org\//, "", u)
                sub(/[?#].*$/, "", u)
                if (u ~ /^cache\/epub\/[0-9]+\/pg[0-9]+\.txt$/) {
                        split(u, p, "/")
                        n = p[3]
                        if ("pg" n ".txt" == p[4]) print n
                } else if (u ~ /^ebooks\/[0-9]+\.txt(\.utf-8)?$/) {
                        n = u; sub(/^ebooks\//, "", n); sub(/\..*$/, "", n)
                        print n
                } else if (u ~ /^files\/([0-9]\/)*[0-9]+\/[0-9]+(-[08])?\.txt$/) {
                        k = split(u, p, "/")
                        n = p[k - 1]
                        f = p[k]; sub(/(-[08])?\.txt$/, "", f)
                        # (the folders: the number'\''s digits but its last)
                        d = ""
                        for (i = 2; i < k - 1; i++) d = d p[i]
                        if (f == n && d == substr(n, 1, length(n) - 1)) print n
                }
        }'
}

# fetch URL FILE (sets problem on failure)
fetch() {
        if command -v curl > /dev/null 2>&1; then
                http_code=$(curl -sSL --max-time 600 -o "$2" -w '%{http_code}' "$1")
                status=$?
                if [ $status -eq 0 ] && [ "$http_code" -ge 400 ] 2>/dev/null; then
                        problem="couldn't download $1 ($http_code)"
                        return 1
                fi
                case $status in
                0) ;;
                6|7|28|35) problem="couldn't download $1 (is this computer connected to the Internet?)"; return 1 ;;
                *) problem="couldn't download $1 (curl error $status)"; return 1 ;;
                esac
        elif command -v wget > /dev/null 2>&1; then
                wget -q -O "$2" "$1" || { problem="couldn't download $1"; return 1; }
        else
                problem="couldn't download $1: needs curl or wget, and neither is installed"
                return 1
        fi
        return 0
}

# version_of URL: the file's fingerprint (its ETag), without downloading it
version_of() {
        if [ -n "$from_file" ]; then
                cksum < "$from_file" | $AWK '{ print "cksum " $1 " " $2 }'
                return
        fi
        if command -v curl > /dev/null 2>&1; then
                curl -sSIL --max-time 60 "$1" 2> /dev/null
        elif command -v wget > /dev/null 2>&1; then
                wget -q --spider --server-response "$1" 2>&1
        fi | $AWK 'tolower($1) == "etag:" { v = $2 } END { print v }' | tr -d '\r'
}

kept_version() {
        [ -f "$here/$1/version" ] || return 0
        $AWK '$1 == "Version:" { v = $2; for (i = 3; i <= NF; i++) v = v " " $i }
              $1 == "Downloaded:" { d = $2 }
              END { if (v != "") print v "\t" d }' "$here/$1/version"
}

# The texts this script added: a folder with a file named gutenberg
# ("Ebook: 17", "Address: ..."), listed in trans. NAME, tab, title.
ours() {
        [ -f "$here/trans" ] || return 0
        $AWK '$1 != "" && substr($1, 1, 1) != "#" { name = $1; $1 = ""; sub(/^ +/, ""); print name "\t" $0 }' \
                "$here/trans" | while IFS='	' read -r name title; do
                [ -f "$here/$name/gutenberg" ] && printf '%s\t%s\n' "$name" "$title"
        done
}

index_program() {
        if [ ! -x "$here/index" ] && [ -f "$here/makefile" ]; then
                make=make
                command -v gmake > /dev/null 2>&1 && make=gmake
                "$make" -C "$here" index > /dev/null
        fi
        [ -x "$here/index" ] || { problem="couldn't build EBaC's index program (make index)"; return 1; }
}

index_verses() {
        od -An -tu1 "$1/bvarray.dat" |
        $AWK '{ for (i = 1; i <= NF; i++) b[++n] = $i }
              END { print ((b[n - 7] * 256 + b[n - 6]) * 256 + b[n - 5]) * 256 + b[n - 4] }'
}

without_line() {
        [ -f "$here/trans" ] || return 0
        $AWK -v n="$1" '$1 != n' "$here/trans"
}

# remove NAME (sets problem on failure)
remove() {
        if [ ! -f "$here/$1/gutenberg" ]; then
                problem="it wasn't added from Project Gutenberg (A removes scrollmapper's)"
                return 1
        fi
        without_line "$1" > "$tmp/trans"
        if ! $AWK '$1 != "" && substr($1, 1, 1) != "#" { f = 1 } END { exit !f }' "$tmp/trans"; then
                problem="it is the only translation in trans"
                return 1
        fi
        cat "$tmp/trans" > "$here/trans" || { problem="couldn't change trans"; return 1; }
        rm -rf "$here/$1" || { problem="couldn't delete the folder $1"; return 1; }
        printf '%s: removed.\n' "$1"
}

# agree NUMBER: show Gutenberg's license, from the file's header, and ask
agree() {
        printf '\n%s: %s\n' "$name" "$title"
        printf 'License: %s\n' "$license_text"
        printf 'Details: https://www.gutenberg.org/ebooks/%s\n' "$1"
        if [ "$agree_all" = yes ]; then
                printf 'Agreed to, with --agree.\n'
                agreed="with --agree"
                return 0
        fi
        if [ ! -t 0 ]; then
                problem="no one to agree to its license (add --agree to agree to it)"
                return 1
        fi
        while :; do
                printf 'Do you agree to these terms (y/n)? '
                typing_mode
                read -r answer || answer=n
                case $(printf '%s\n' "$answer" | tr 'A-Z' 'a-z') in
                y|yes) agreed="typed y"; return 0 ;;
                n|no) problem="its license wasn't agreed to"; return 1 ;;
                *) printf 'Please type y or n.\n' ;;
                esac
        done
}

# split_chapters FILE NUMBER FOLDER: the "chapters" format (see KNOWN), into
# FOLDER's book files, booklist and booknames. Prints "ok BOOKS VERSES" or
# "problem MESSAGE".
split_chapters() {
        known_detail "$2" > "$tmp/detail"
        tr -d '\r' < "$1" | $AWK -F '\t' -v folder="$3" "$PLAIN"'
        function fail(m) { if (!failed) print "problem " m; failed = 1; exit }
        function starts(line, s) { return substr(line, 1, length(s)) == s }
        function file_of(name,    f) { f = name; gsub(/[^A-Za-z0-9]/, "", f); return f }
        # a verse (or a part of chapter 0, or a book heading as verse 0),
        # as a line of its book file
        function put(c, v, text) {
                text = plain(text)
                if (text == "") return
                print "[" c ":" v "] " text > out
                verses++
                if (c > 0 && v > 0) { counted[b]++ }
        }
        function flush() {
                if (have) put(cur_c, cur_v, buf)
                have = 0; buf = ""
        }
        function add(text) {
                if (text == "") return
                buf = (buf == "") ? text : buf " " text
        }
        function start_book(k) {
                flush()
                if (out != "") close(out)
                b = k
                out = folder "/" file_of(bookname[b])
                print file_of(bookname[b]) > (folder "/booklist")
                print bookname[b] > (folder "/booknames")
                mode = "heading"; chap = 0; vnum = 0; cur_c = 1; cur_v = 0; have = 1; buf = ""
        }
        # the next verse expected ("C:V", the next of the chapter, or the
        # first of the next) in line, at a word: its index, and in
        # mlen how long it is with the book name before it, if any
        function find_verse(line, c, v,    pat, i, j, from, pre, n) {
                pat = c ":" v
                from = 1
                while ((i = index(substr(line, from), pat)) > 0) {
                        j = from + i - 1
                        n = j + length(pat)
                        if ((j == 1 || substr(line, j - 1, 1) == " ") &&
                            (n > length(line) || substr(line, n, 1) == " ")) {
                                pre = bookname[b] " "
                                mstart = j; mlen = length(pat)
                                if (j > length(pre) && substr(line, j - length(pre), length(pre)) == pre &&
                                    (j - length(pre) == 1 || substr(line, j - length(pre) - 1, 1) == " ")) {
                                        mstart = j - length(pre); mlen += length(pre)
                                }
                                return mstart
                        }
                        from = j + 1
                }
                return 0
        }
        # the rest of a line of verses: split where the next verses start
        function verses_in(line,    m, nc, nv) {
                for (;;) {
                        nc = chap; nv = vnum + 1
                        if (chap == 0) { nc = 1; nv = 1 }
                        m = find_verse(line, nc, nv)
                        if (m == 0) { add(line); return }
                        add(substr(line, 1, m - 1))
                        if (chap == 0) chap = 1
                        flush()
                        vnum = nv; cur_c = chap; cur_v = vnum; have = 1
                        line = substr(line, m + mlen)
                        sub(/^ +/, "", line)
                        mode = "verses"
                }
        }
        FILENAME == ARGV[1] {
                if ($1 == "book") { nb++; bookname[nb] = $2; heading[nb] = $3 }
                if ($1 == "front") { nf++; front[nf] = $2 }
                next
        }
        /^\*\*\* START OF / { body = 1; next }
        /^\*\*\* END OF / { body = 0; next }
        !body { next }
        {
                line = $0
                sub(/^[ \t]+/, "", line); sub(/[ \t]+$/, "", line)
                text[++lines] = line
        }
        END {
                if (failed) exit
                if (lines == 0) fail("no text between Gutenberg'\''s START and END lines")
                # Where book 1 starts: its heading'\''s last line before the
                # first verse (the Contents list names it too)
                for (i = 1; i <= lines; i++)
                        if (text[i] ~ /^([0-9] [A-Za-z]+ )?1:1 /) { first_verse = i; break }
                if (!first_verse) fail("no verse 1:1 in it")
                for (i = first_verse; i >= 1; i--)
                        if (starts(text[i], heading[1])) { book1 = i; break }
                if (!book1) fail("no heading \"" heading[1] "\" before its first verse")

                # Chapter 0: the front matter, a verse for each part
                out = folder "/" file_of(bookname[1])
                b = 1; part = 0; buf = ""; have = 0
                for (i = 1; i < book1; i++) {
                        if (part < nf && starts(text[i], front[part + 1])) {
                                flush(); part++; cur_c = 0; cur_v = part; have = 1
                        }
                        add(text[i])
                }
                flush()
                if (part != nf) fail("its front matter has " part " of the " nf " parts expected")
                print file_of(bookname[1]) > (folder "/booklist")
                print bookname[1] > (folder "/booknames")
                b = 1; mode = "heading"; chap = 0; vnum = 0; cur_c = 1; cur_v = 0; have = 1; buf = ""

                for (i = book1; i <= lines; i++) {
                        line = text[i]
                        if (line == "") continue
                        # the next book: its heading, once this one has verses
                        if (b < nb && mode == "verses" && starts(line, heading[b + 1])) {
                                start_book(b + 1)
                                add(line)
                                continue
                        }
                        # a line numbering a chapter: "Alma Chapter 5"
                        if (line == bookname[b] " Chapter " (chap + 1) || (chap == 0 && line == bookname[b] " Chapter 1")) {
                                if (mode == "heading") { flush(); have = 0 }
                                continue
                        }
                        if (line ~ / Chapter [0-9]+$/ && starts(line, bookname[b] " Chapter "))
                                fail("\"" line "\" out of order (in " bookname[b] " " chap ")")
                        if (mode == "heading") {
                                # its heading and summary, until verse 1:1
                                if (find_verse(line, 1, 1) == 0) { add(line); continue }
                        }
                        # (a new chapter: the verse after it is C:1)
                        if (mode == "verses" && find_verse(line, chap + 1, 1) == 1 &&
                            find_verse(line, chap, vnum + 1) == 0) {
                                flush(); chap++; vnum = 0
                        }
                        verses_in(line)
                }
                flush()
                if (b != nb) fail("found " b " of its " nb " books (the next: \"" heading[b + 1] "\")")
                for (k = 1; k <= nb; k++)
                        if (!counted[k]) fail(bookname[k] " has no verses")
                print "ok " nb " " verses
        }' "$tmp/detail" -
}

# add NUMBER URL: add the text (sets problem on failure)
add() {
        folder=$here/$name
        adding=$folder
        printf '%s: writing its books into the folder %s...\n' "$name" "$name"
        rm -rf "$folder"
        mkdir "$folder" || { problem="couldn't make the folder $folder"; return 1; }
        case $format in
        chapters) result=$(split_chapters "$tmp/text" "$1" "$folder" | tail -n 1) ;;
        *) result="problem this script doesn't know the format $format" ;;
        esac
        case $result in
        "ok "*) ;;
        "problem "*) problem=${result#problem }; return 1 ;;
        *) problem="couldn't read the text"; return 1 ;;
        esac
        set -- $result
        total=$3
        printf '%s: building the concordance index...\n' "$name"
        index_program || return 1
        (cd "$folder" && "$here/index" -y > /dev/null) || { problem="building the index failed"; return 1; }
        found=$(index_verses "$folder")
        if [ "$found" != "$total" ]; then
                problem="the index has $found verses, but the text has $total"
                return 1
        fi
        { printf 'License: %s\n' "$license_text"
          printf 'Details: https://www.gutenberg.org/ebooks/%s\n' "$number"
          printf 'Agreed: %s (%s)\n' "$(date '+%Y-%m-%d')" "$agreed"
        } > "$folder/license" || { problem="couldn't write $name/license"; return 1; }
        if [ -n "$version_now" ]; then
                { printf 'Version: %s\n' "$version_now"
                  printf 'Downloaded: %s\n' "$(date '+%Y-%m-%d')"
                } > "$folder/version"
        fi
        { printf 'Ebook: %s\n' "$number"
          printf 'Address: %s\n' "$url"
        } > "$folder/gutenberg" || { problem="couldn't write $name/gutenberg"; return 1; }
        { printf '%-11s %s\n' "$name" "$title"
          without_line "$name"; } > "$tmp/trans" &&
                cat "$tmp/trans" > "$here/trans"
        adding=""
        printf '%s: done. %s is ready: start EBaC and press C to choose it.\n' "$name" "$title"
        return 0
}

# add_url URL: from the address to the text added
add_url() {
        url=$1
        number=$(ebook_number "$url")
        if [ -z "$number" ]; then
                problem="not an address of a Project Gutenberg plain text (such as https://www.gutenberg.org/cache/epub/17/pg17.txt)"
                return 1
        fi
        line=$(known "$number")
        if [ -z "$line" ]; then
                problem="ebook $number isn't one this importer knows yet (each text's layout is added by hand; see readme.architecture)"
                return 1
        fi
        name=$(printf '%s\n' "$line" | cut -f2)
        format=$(printf '%s\n' "$line" | cut -f3)
        if [ -d "$here/$name" ] && [ ! -f "$here/$name/gutenberg" ]; then
                problem="a translation named $name is there already, not from Project Gutenberg"
                return 1
        fi
        version_now=$(version_of "$url")
        kept=$(kept_version "$name")
        if [ -n "$version_now" ] && [ -f "$here/$name/gutenberg" ] &&
           [ "${kept%%	*}" = "$version_now" ]; then
                problem="you already have this version (downloaded ${kept#*	}). To download it again, remove it first (r and its number), then add it"
                return 1
        fi
        # The text, then its title and license, from its header (Gutenberg
        # gives the license in the file itself, so it is downloaded first;
        # nothing is added unless it is agreed to)
        printf '%s: downloading...\n' "$name"
        if [ -n "$from_file" ]; then
                cp "$from_file" "$tmp/text" || { problem="couldn't read $from_file"; return 1; }
        else
                fetch "$url" "$tmp/text" || return 1
        fi
        title=$(tr -d '\r' < "$tmp/text" | $AWK "$PLAIN"'/^Title: / { print plain(substr($0, 8)); exit }')
        [ -n "$title" ] || title="Project Gutenberg ebook $number"
        license_text=$(tr -d '\r' < "$tmp/text" | $AWK "$PLAIN"'
                /^This (e|E)(b|B)ook is for the use of anyone/ { on = 1 }
                on && /^[ \t]*$/ { exit }
                on { t = t " " $0 }
                END { print plain(t) }')
        [ -n "$license_text" ] || license_text="Project Gutenberg License (https://www.gutenberg.org/policy/license.html)"
        agree "$number" || return 1
        add "$number" "$url"
}

usage() {
        echo "usage: sh add-gutenberg.sh [--agree] [--file PATH] URL | --remove NAME | --list"
        echo "Adds a text from Project Gutenberg to EBaC by its plain text's address, or removes one."
}

# ---- the arguments
agree_all=no
from_file=""
url_arg=""
remove_name=""
list_only=no
while [ $# -gt 0 ]; do
        case $1 in
        --agree) agree_all=yes ;;
        --list) list_only=yes ;;
        --file) [ $# -ge 2 ] || { usage >&2; exit 2; }; from_file=$2; shift ;;
        --remove) [ $# -ge 2 ] || { usage >&2; exit 2; }; remove_name=$2; shift ;;
        --from-ebac) [ $# -ge 3 ] || { usage >&2; exit 2; }; from_ebac=$2; start_with=$3; shift; shift ;;
        -h|--help) usage; exit 0 ;;
        -*) usage >&2; exit 2 ;;
        *) url_arg=$1 ;;
        esac
        shift
done

tmp=$(mktemp -d "${TMPDIR:-/tmp}/ebac-gutenberg.XXXXXX") || exit 1
trap cleanup EXIT
trap interrupted INT

if [ $list_only = yes ]; then
        ours | $AWK -F '\t' '{ print $1 ": " $2 }'
        exit 0
fi

if [ -n "$remove_name" ]; then
        remove "$remove_name" || { printf '%s: not removed: %s.\n' "$remove_name" "$problem"; exit 1; }
        exit 0
fi

if [ -z "$url_arg" ]; then
        # Asked for, from EBaC's P: the address, or r and a number to remove
        [ -t 1 ] && { clear 2> /dev/null || echo; }
        echo "Add a text from Project Gutenberg"
        echo
        # (lines of 50 columns at most: a phone's screen)
        echo "Find it on gutenberg.org and open its"
        echo "\"Plain Text UTF-8\" file. When it is the one,"
        echo "paste its address here."
        ours > "$tmp/ours"
        if [ -s "$tmp/ours" ]; then
                echo
                echo "Added from Project Gutenberg:"
                $AWK -F '\t' '{ printf "%2d) %s: %s\n", NR, $1, $2 }' "$tmp/ours"
                echo
                echo "To remove one: r and its number (r1)."
        fi
        echo
        printf 'Address (Enter alone: back): '
        typing_mode
        read -r answer || answer=""
        answer=$(printf '%s\n' "$answer" | $AWK '{ sub(/^[ \t]+/, ""); sub(/[ \t]+$/, ""); print }')
        case $answer in
        "")
                [ -n "$from_ebac" ] && back_to_ebac
                exit 0
                ;;
        [rR][0-9]*)
                k=${answer#?}
                pick=$($AWK -F '\t' -v n="$k" 'NR == n + 0 { print $1 }' "$tmp/ours")
                if [ -z "$pick" ]; then
                        printf '%s: not in the list.\n' "$answer"
                        failed=1
                elif remove "$pick"; then
                        [ "$start_with" = "$pick" ] && start_with=""
                else
                        printf '%s: not removed: %s.\n' "$pick" "$problem"
                        failed=1
                fi
                ;;
        *)
                url_arg=$answer
                ;;
        esac
fi

if [ -n "$url_arg" ]; then
        if add_url "$url_arg"; then
                start_with=$name
        else
                [ -n "$adding" ] && [ -d "$adding" ] && rm -rf "$adding"
                adding=""
                printf '%s: not added: %s.\n' "${name:-$url_arg}" "$problem"
                failed=1
        fi
fi

if [ -n "$from_ebac" ]; then
        press_any_key
        back_to_ebac
fi
[ $failed -eq 0 ]
