# splitdefs.awk -- part of "make coco" (see mkcoco.sh).
#
# ansi2knr only recognizes a function definition whose name is the first
# thing on its line, with the type on the line before:
#       static int
#       foo(int a)
# EBaC writes "static int foo(int a)" on one line, so split those lines.
# A definition is taken to be a line starting at the left margin with a
# type, with no ";" or "=" in it, ending (after any comment) with ")", or
# with "," when its parameters go on to the next lines.

/^(static|extern|int|char|short|long|unsigned|void|struct|FILE|double|float)[ \t*]/ &&
!/;/ && !/=/ {
        line = $0
        gsub(/\/\*.*\*\//, "", line)            # without comments
        if (line ~ /[),][ \t]*$/ &&
            match($0, /[A-Za-z_][A-Za-z0-9_]*[ \t]*\(/)) {
                type = substr($0, 1, RSTART - 1)
                sub(/[ \t]+$/, "", type)
                if (type != "") {
                        print type
                        print substr($0, RSTART)
                        next
                }
        }
}
{ print }
