# Copyright (C) 2012 Alex Kirhenshtein
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

SUBAGENT=darwin

pkglib_LTLIBRARIES=darwin.la
darwin_la_SOURCES=darwin.cpp net.cpp system.cpp disk.cpp cpu.cpp
darwin_la_CPPFLAGS=-I@top_srcdir@/include -I@top_srcdir@/build
darwin_la_LDFLAGS=-module -avoid-version -export-symbols ../platform-subagent.sym
darwin_la_LIBADD=../../libnxagent/libnxagent.la ../../../libnetxms/libnetxms.la

EXTRA_DIST=cpu.h darwin.h disk.h net.h system.h

if !STATIC_BUILD
install-exec-hook:
	mv -f $(DESTDIR)$(pkglibdir)/$(SUBAGENT)@SHLIB_SUFFIX@ $(DESTDIR)$(pkglibdir)/$(SUBAGENT).nsm
	rm -f $(DESTDIR)$(pkglibdir)/$(SUBAGENT).la
	rm -f $(DESTDIR)$(libdir)/libnsm_$(SUBAGENT)@SHLIB_SUFFIX@
	ln -s netxms/$(SUBAGENT).nsm $(DESTDIR)$(libdir)/libnsm_$(SUBAGENT)@SHLIB_SUFFIX@
endif
