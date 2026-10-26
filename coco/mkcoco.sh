#!/bin/sh
# mkcoco.sh -- "make coco": everything the Tandy CoCo 3 (OS-9 6809) needs.
#
# usage: sh coco/mkcoco.sh OUTDIR     (from the top of the EBaC source,
#        after "make", which builds the concordance index)
#
# Writes OUTDIR/ebac-coco.tar, and OUTDIR/ebac-coco.dsk (an OS-9 disk image)
# if ToolShed's "os9" program is installed. Both hold one folder, BIBLE,
# to go in /dd/BIBLE on the CoCo (where EBaC looks for its files):
#   BIBLE/trans       the list of translations
#   BIBLE/AV/...      the King James text, booklist and the index (.dat)
#   BIBLE/SRC/...     the program's source, converted to K&R C, and a
#                     makefile for Microware C
#
# The CoCo differs from the computer this runs on in three ways, handled
# here: its C compiler is K&R C (ansi2knr converts the function
# definitions; see also both.h); its text files end lines with a carriage
# return (CR) instead of a line feed; and its file names can't begin with a
# digit, so 1Chronicles is stored as x1Chronicle (as mfopen() in file.c
# expects). The .dat files are copied unchanged: they are the same on every
# computer, and changing LF to CR doesn't move any text.

set -e
CC=${CC:-cc}
# splitdefs.awk needs a newer awk than Solaris's /usr/bin/awk: nawk there
if command -v nawk > /dev/null 2>&1; then AWK=nawk; else AWK=awk; fi
out=$1
[ -n "$out" ] || { echo "usage: sh coco/mkcoco.sh OUTDIR" >&2; exit 2; }
for f in AV/bvarray.dat AV/chapter.dat AV/cvarray.dat \
         AV/references.dat AV/words.dat AV/format.dat; do
        [ -f "$f" ] || { echo "mkcoco.sh: $f is missing; run make first" >&2; exit 1; }
done

stage=$out/stage
rm -rf "$stage"
mkdir -p "$stage/BIBLE/AV" "$stage/BIBLE/SRC"

# LF -> CR, for a text file
cr() {
        tr '\n' '\r' < "$1" > "$2"
}

# the source, as K&R C
$CC -w -DSTDC_HEADERS -o "$out/ansi2knr" coco/ansi2knr.c
for f in bible.c file.c userint.c process.c cure.c; do
        {
                # the version for the Information screen (from the makefile)
                if [ "$f" = userint.c ] && [ -n "$EBAC_COMMIT" ]; then
                        echo "#define EBAC_COMMIT \"$EBAC_COMMIT\""
                fi
                $AWK -f coco/splitdefs.awk "$f" | "$out/ansi2knr" | grep -v '^#line '
        } > "$out/knr.c"
        cr "$out/knr.c" "$stage/BIBLE/SRC/$f"
done
rm -f "$out/knr.c"
for f in bible.h both.h screen.h; do
        cr "$f" "$stage/BIBLE/SRC/$f"
done
cr coco/makefile "$stage/BIBLE/SRC/makefile"

# the text and the index
cr trans "$stage/BIBLE/trans"
cr AV/booklist "$stage/BIBLE/AV/booklist"
for book in $(cat AV/booklist); do
        case $book in
        [0-9]*) name=x$(echo "$book" | sed 's/.$//') ;;
        *)      name=$book ;;
        esac
        cr "AV/$book" "$stage/BIBLE/AV/$name"
done
cp AV/*.dat "$stage/BIBLE/AV/"

(cd "$stage" && tar -cf ../ebac-coco.tar BIBLE)
echo "mkcoco.sh: wrote $out/ebac-coco.tar"

# an OS-9 disk image, with ToolShed (https://github.com/nitros9project/toolshed)
if command -v os9 > /dev/null 2>&1; then
        dsk=$out/ebac-coco.dsk
        rm -f "$dsk"
        # 32768 sectors of 256 bytes: 8 MB, room for the 6 MB of files
        os9 format -q -l32768 -nEBaC "$dsk"
        os9 makdir "$dsk,BIBLE" "$dsk,BIBLE/AV" "$dsk,BIBLE/SRC"
        (cd "$stage" && find BIBLE -type f) | while read -r f; do
                os9 copy "$stage/$f" "$dsk,$f"
        done
        echo "mkcoco.sh: wrote $dsk"
else
        echo "mkcoco.sh: no OS-9 disk image made: ToolShed's os9 program isn't installed"
fi
