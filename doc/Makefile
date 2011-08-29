#       Makefile
#
#       Dustin Dorroh <ddorroh@aplopteng.com>

# Definitions of programs to be used in the build
MAKEINFO = makeinfo
TEXI2PDF = texi2pdf
TEXI2DVI = texi2dvi
TEXI2HTML = texi2html
DVIPS = dvips

# Definitions fot included GNU documentation 
GNU_STANDARDS = standards
GNU_STANDARDS_TEXI = $(addsuffix .texi, $(GNU_STANDARDS))
GNU_STANDARDS_INFO = $(addsuffix .info, $(GNU_STANDARDS))
GNU_STANDARDS_PDF = $(addsuffix .pdf, $(GNU_STANDARDS))
GNU_STANDARDS_HTML = $(addsuffix .html, $(GNU_STANDARDS))
GNU_STANDARDS_DOCS = $(GNU_STANDARDS_PDF) $(GNU_STANDARDS_HTML) $(GNU_STANDARDS_INFO)

# Basenames of the documentation to be generated
DOCS = rhythmos

# File type definitions
TEXI = $(addsuffix .texi, $(DOCS))
TEXI_SRC = $(wildcard *.texi*)
TEX_SRC = $(wildcard *.tex)
MAKEFILES = $(wildcard Makefile*)
TXT_FILES = $(wildcard *.txt)
SCRIPTS = $(wildcard *.sh) $(wildcard *-sh)

# Names for generated docs (ie `rhythmos.pdf')
INFO = $(addsuffix .info, $(DOCS))
PDF = $(addsuffix .pdf, $(DOCS))
DVI = $(addsuffix .dvi, $(DOCS))
HTML = $(addsuffix .html, $(DOCS))
GENERATED_DOCS = $(INFO) $(PDF) $(DVI) $(HTML) $(PS) $(GNU_STANDARDS_DOCS)

# Files that are *not* removed on a `make clean'
PERMANENT_FILES := $(TEXI_SRC) \
	$(TEX_SRC)\
	$(MAKEFILES) \
	$(SCRIPTS) \
	$(TXT_FILES) \
	$(GENERATED_DOCS)

# Files that are removed on a `make clean'
TMP_FILES := $(wildcard *.aux) \
	$(wildcard *.cp) \
	$(wildcard *.cps) \
	$(wildcard *.fn) \
	$(wildcard *.ky) \
	$(wildcard *.log) \
	$(wildcard *.op ) \
	$(wildcard *.pg) \
	$(wildcard *.toc) \
	$(wildcard *.tp) \
	$(wildcard *.vr)


all : version $(PDF)

info: version $(INFO)

pdf: version $(PDF)

dvi: version $(DVI)

html: version $(HTML)

ps: version  $(PS)

version:
	./version.sh

$(INFO): $(TEXI)
	$(MAKEINFO) $(TEXI)

$(PDF): $(TEXI)
	$(TEXI2PDF) $(TEXI)

$(DVI): $(TEXI)
	$(TEXI2DVI) $(TEXI)

$(HTML): $(TEXI)
	$(TEXI2HTML) $(TEXI)

$(PS): $(DVI)
	$(DVIPS) $(DVI)
 
$(GNU_STANDARDS): $(GNU_STANDARDS_TEXI)
	$(MAKEINFO)  $<
	$(TEXI2PDF) $<
	$(TEXI2HTML)  $<



.PHONY : clean clean-all
clean :
	-rm -f -v $(TMP_FILES)

clean-all :
	-rm -f -v $(TMP_FILES) $(GENERATED_DOCS)
