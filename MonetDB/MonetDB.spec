%define name MonetDB
%define version 1.36.3
%{!?buildno: %define buildno %(date +%Y%m%d)}
%define release %{buildno}%{?dist}%{?oid32:.oid32}%{!?oid32:.oid%{bits}}

# groups of related archs
%define all_x86 i386 i586 i686

%ifarch %{all_x86}
%define bits 32
%else
%define bits 64
%endif

# buildsystem is set to 1 when building an rpm from within the build
# directory; it should be set to 0 (or not set) when building a proper
# rpm
%{!?buildsystem: %define buildsystem 0}

Name: %{name}
Version: %{version}
Release: %{release}
Summary: MonetDB - Monet Database Management System
Vendor: MonetDB BV <monet@cwi.nl>

Group: Applications/Databases
License: MPL - http://monetdb.cwi.nl/Legal/MonetDBLicense-1.1.html
URL: http://monetdb.cwi.nl/
Source: http://downloads.sourceforge.net/monetdb/%{name}-%{version}.tar.gz
BuildRoot: %(mktemp -ud %{_tmppath}/%{name}-%{version}-%{release}-XXXXXX)

BuildRequires: zlib-devel, bzip2-devel, openssl-devel

%description
MonetDB is a database management system that is developed from a
main-memory perspective with use of a fully decomposed storage model,
automatic index management, extensibility of data types and search
accelerators, SQL- and XML- frontends.

This package contains the core components of MonetDB.  If you want to
use MonetDB, you will certainly need this package.

%package devel
Summary: MonetDB development package
Group: Applications/Databases
Requires: %{name} = %{version}-%{release}
Requires: zlib-devel, bzip2-devel, openssl-devel

%description devel
MonetDB is a database management system that is developed from a
main-memory perspective with use of a fully decomposed storage model,
automatic index management, extensibility of data types and search
accelerators, SQL- and XML- frontends.

This package contains the files needed to develop with MonetDB.

%prep
rm -rf $RPM_BUILD_ROOT

%setup -q -n %{name}-%{version}

%build

%{configure} \
	--enable-strict=no \
	--enable-assert=no \
	--enable-debug=no \
	--enable-optimize=yes \
	--enable-bits=%{bits} \
	%{?oid32:--enable-oid32} \
	%{?comp_cc:CC="%{comp_cc}"}

make

%install
rm -rf $RPM_BUILD_ROOT

make install DESTDIR=$RPM_BUILD_ROOT

# cleanup stuff we don't want to have installed
find $RPM_BUILD_ROOT -name .incs.in -delete -o -name \*.la -delete

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%clean
rm -fr $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%{_libdir}/libmutils.so.*
%{_libdir}/libstream.so.*
%{_libdir}/libbat.so.*

%files devel
%defattr(-,root,root)
%{_bindir}/monetdb-config

%dir %{_includedir}/%{name}
%dir %{_includedir}/%{name}/*
%{_includedir}/%{name}/*.h
%{_includedir}/%{name}/*/*.[hcm]
%{_libdir}/libmutils.so
%{_libdir}/libstream.so
%{_libdir}/libbat.so

%changelog
* Mon Mar 22 2010 Sjoerd Mullender <sjoerd@acm.org> - 1.36.3-20100322
- Rebuilt.

* Mon Mar 01 2010 Fabian Groffen <fabian@cwi.nl> - 1.36.3-20100322
- Fixed bug in UDP stream creation causing UDP connections to already
  bound ports to be reported as successful.
* Wed Feb 24 2010 Sjoerd Mullender <sjoerd@acm.org> - 1.36.1-20100224
- Rebuilt.

* Mon Feb 22 2010 Sjoerd Mullender <sjoerd@acm.org> - 1.36.1-20100223
- Various concurrency bugs were fixed.
- Various changes were made to run better on systems that don't have enough
  memory to keep everything in core that was touched during query processing.
  This is done by having the higher layers giving hints to the database
  kernel about future use, and the database kernel giving hings to the
  operating system kernel about how (virtual) memory is going to be used.

* Thu Feb 18 2010 Stefan Manegold <Stefan.Manegold@cwi.nl> - 1.36.1-20100223
- Fixed bug in mergejoin implementation.
  This fixes bug  #2952191.

* Tue Feb  2 2010 Sjoerd Mullender <sjoerd@acm.org> - 1.36.1-20100223
- Added support for compiling on Windows using the Cygwin-provided
  version of flex.

* Thu Jan 21 2010 Sjoerd Mullender <sjoerd@acm.org> - 1.36.1-20100223
- Fix compilation issue when configured with --with-curl.
  This fixes bug #2924999.

* Thu Jan 21 2010 Fabian Groffen <fabian@cwi.nl> - 1.36.1-20100223
- Added implementation of MT_getrss() for Solaris.  This yields in the
  kernel knowing about its (approximate) memory usage to try and help
  the operating system to free that memory that is best to free, instead
  of a random page, e.g. the work of the vmtrim thread.

* Wed Jan 20 2010 Sjoerd Mullender <sjoerd@cwi.nl> - 1.36.1-20100223
- Implemented a "fast" string BAT append:
  Under certain conditions, instead of inserting values one-by-one,
  we now concatenate the string heap wholesale and just manipulate
  the offsets.
  This works both for BATins and BATappend.

* Wed Jan  6 2010 Sjoerd Mullender <sjoerd@cwi.nl> - 1.36.1-20100223
- Changed the string heap implementation to also contain the hashes of
  strings.
- Changed the implementation of the string offset columns to be
  dynamically sized.

