#
# Makefile for a Video Disk Recorder plugin
#

# The official name of this plugin.
# This name will be used in the '-P...' option of VDR to load the plugin.
# By default the main source file also carries this name.

PLUGIN = osdteletext

### The version number of this plugin (taken from the main source file):

VERSION = $(shell grep 'static const char \*VERSION *=' $(PLUGIN).c | awk '{ print $$6 }' | sed -e 's/[";]//g')

### switch compiler
ifeq ($(CLANG), 1)
    CC="clang"
    CXX="clang++"
endif

### The directory environment:

# Use package data if installed...otherwise assume we're under the VDR source directory:
PKGCFG = $(if $(VDRDIR),$(shell pkg-config --variable=$(1) $(VDRDIR)/vdr.pc),$(shell PKG_CONFIG_PATH="$$PKG_CONFIG_PATH:../../.." pkg-config --variable=$(1) vdr))
LIBDIR = $(call PKGCFG,libdir)
LOCDIR = $(call PKGCFG,locdir)
PLGCFG = $(call PKGCFG,plgcfg)
#
TMPDIR ?= /tmp

### The compiler options:

export CFLAGS   = $(call PKGCFG,cflags)
export CXXFLAGS = $(call PKGCFG,cxxflags)

ifeq ($(CLANG), 1)
    $(info ORG CFLAGS=$(CFLAGS))
    $(info ORG CXXFLAGS=$(CXXFLAGS))
    # remove not supported options (at least Fedora 33)
    CFLAGS_PKGCFG   = $(call PKGCFG,cflags)
    CXXFLAGS_PKGCFG = $(call PKGCFG,cxxflags)
    CFLAGS_1 = $(subst -ffat-lto-objects,,$(CFLAGS_PKGCFG))
    CXXFLAGS_1 = $(subst -ffat-lto-objects,,$(CXXFLAGS_PKGCFG))
    CFLAGS_2 = $(subst -flto=auto,,$(CFLAGS_1))
    CXXFLAGS_2 = $(subst -flto=auto,,$(CXXFLAGS_1))
    CFLAGS_3 = $(subst -specs=/usr/lib/rpm/redhat/redhat-hardened-cc1,,$(CFLAGS_2))
    CXXFLAGS_3 = $(subst -specs=/usr/lib/rpm/redhat/redhat-hardened-cc1,,$(CXXFLAGS_2))
    CFLAGS_4 = $(subst -specs=/usr/lib/rpm/redhat/redhat-hardened-ld,,$(CFLAGS_3))
    CXXFLAGS_4 = $(subst -specs=/usr/lib/rpm/redhat/redhat-hardened-ld,,$(CXXFLAGS_3))
    CFLAGS_5 = $(subst -specs=/usr/lib/rpm/redhat/redhat-annobin-cc1,,$(CFLAGS_4))
    CXXFLAGS_5 = $(subst -specs=/usr/lib/rpm/redhat/redhat-annobin-cc1,,$(CXXFLAGS_4))
    export CFLAGS   = $(CFLAGS_5)
    export CXXFLAGS = $(CXXFLAGS_5)
    $(info MOD CFLAGS=$(CFLAGS))
    $(info MOD CXXFLAGS=$(CXXFLAGS))
endif

### The version number of VDR's plugin API:

APIVERSION = $(call PKGCFG,apiversion)

### Allow user defined options to overwrite defaults:

-include $(PLGCFG)

### The name of the distribution archive:

ARCHIVE = $(PLUGIN)-$(VERSION)
PACKAGE = vdr-$(ARCHIVE)

### The name of the shared object file:

SOFILE = libvdr-$(PLUGIN).so

### Includes and Defines (add further entries here):

INCLUDES +=

DEFINES += -DPLUGIN_NAME_I18N='"$(PLUGIN)"'

### The object files (add further files here):

OBJS = $(PLUGIN).o menu.o txtfont.o txtrecv.o txtrender.o displaybase.o display.o storage.o legacystorage.o packedstorage.o rootdir.o setup.o

### The main target:

all: $(SOFILE) i18n

### Implicit rules:

%.o: %.c
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $(DEFINES) $(INCLUDES) -o $@ $<

### Dependencies:

MAKEDEP = $(CXX) -MM -MG
DEPFILE = .dependencies
$(DEPFILE): Makefile
	@$(MAKEDEP) $(CPPFLAGS) $(CXXFLAGS) $(DEFINES) $(INCLUDES) $(OBJS:%.o=%.c) > $@

-include $(DEPFILE)

### Internationalization (I18N):

PODIR     = po
I18Npo    = $(wildcard $(PODIR)/*.po)
I18Nmo    = $(addsuffix .mo, $(foreach file, $(I18Npo), $(basename $(file))))
I18Nmsgs  = $(addprefix $(DESTDIR)$(LOCDIR)/, $(addsuffix /LC_MESSAGES/vdr-$(PLUGIN).mo, $(notdir $(foreach file, $(I18Npo), $(basename $(file))))))
I18Npot   = $(PODIR)/$(PLUGIN).pot

%.mo: %.po
	msgfmt -c -o $@ $<

$(I18Npot): $(wildcard *.c) $(wildcard *.h)
	xgettext -C -cTRANSLATORS --no-wrap --no-location -k -ktr -ktrNOOP --package-name=vdr-$(PLUGIN) --package-version=$(VERSION) --msgid-bugs-address='<see README>' -o $@ `ls $^`

%.po: $(I18Npot)
	msgmerge -U --no-wrap --no-location --backup=none -q -N $@ $<
	@touch $@

$(I18Nmsgs): $(DESTDIR)$(LOCDIR)/%/LC_MESSAGES/vdr-$(PLUGIN).mo: $(PODIR)/%.mo
	install -D -m644 $< $@

.PHONY: i18n
i18n: $(I18Nmo) $(I18Npot)

install-i18n: $(I18Nmsgs)

### Targets:

$(SOFILE): $(OBJS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(LDFLAGS) -shared $(OBJS) -o $@

install-lib: $(SOFILE)
	install -D $^ $(DESTDIR)$(LIBDIR)/$^.$(APIVERSION)

install: install-lib install-i18n

dist: $(I18Npo) clean
	@-rm -rf $(TMPDIR)/$(ARCHIVE)
	@mkdir $(TMPDIR)/$(ARCHIVE)
	@cp -a * $(TMPDIR)/$(ARCHIVE)
	@tar czf $(PACKAGE).tgz -C $(TMPDIR) --exclude debian --exclude CVS --exclude .svn $(ARCHIVE)
	@-rm -rf $(TMPDIR)/$(ARCHIVE)
	@echo Distribution package created as $(PACKAGE).tgz

clean:
	@-rm -f $(PODIR)/*.mo $(PODIR)/*.pot
	@-rm -f $(OBJS) $(DEPFILE) *.so *.tgz core* *~
