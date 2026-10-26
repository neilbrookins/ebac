# translation.mk -- keeps one translation's concordance index up to date.
#
# The top-level makefile runs this inside each translation folder listed in
# 'trans' (for example AV), as:   make -C AV -f ../translation.mk INDEX=...
# One copy serves every translation, so a new translation folder needs no
# makefile of its own: just its text files and its 'booklist'.
#
# The .dat files are the index (see readme.architecture), made by the index program from the
# books named in 'booklist'. They are made again whenever one of them is
# missing, or is older than 'booklist', any book, or the index program.
#
# Needs GNU make (the usual 'make' on Linux and macOS).

# The index program. The top-level makefile passes its full path.
INDEX = ../index

# The books of this translation, read from booklist, one name per line.
BOOKS := $(shell cat booklist)

# Every book in booklist must be here. (Without this check, make would
# quietly skip the rule below for a missing book instead of saying so.)
MISSING := $(filter-out $(wildcard $(BOOKS)),$(BOOKS))
ifneq ($(MISSING),)
$(error $(notdir $(CURDIR))/booklist names books with no file in $(notdir $(CURDIR)): $(MISSING))
endif

# The files index writes. These names are fixed by the program.
DATS = bvarray.dat chapter.dat cvarray.dat references.dat words.dat position.dat plainpos.dat format.dat

all: $(DATS)
	@:

# One run of index writes all the files. The rule is written as a pattern
# ("%" stands for the ".") because make treats all the targets of a pattern
# rule as made together by one run of its recipe.
bvarray%dat chapter%dat cvarray%dat references%dat words%dat position%dat plainpos%dat format%dat: booklist $(BOOKS) $(INDEX)
	@echo "Indexing $(notdir $(CURDIR))..."
	$(INDEX) -y > /dev/null

.PHONY: all
