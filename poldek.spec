# $Revision$, $Date$
Summary:	The poldek - rpm packages management helper tool
Name:		poldek
Version:	0.11
Release:	1
License:	GPL
Group:		Utilities/System
Group(pl):	Narz�dzia/System
Source:		%{name}-%{version}.tar.gz
Requires:	/bin/rpm
BuildRequires:  db3-devel >= 3.1.14-2
BuildRequires:	rpm-devel >= 3.0.6-2
BuildRequires:	zlib-devel, zlib-static
BuildRequires:	bzip2-devel, bzip2-static
BuildRequires:	/usr/bin/pod2man
BuildRequires:  trurlib-devel >= 0.42
BuildRoot:	/tmp/%{name}-%{version}-root-%(id -u -n)

%description
The poldek is a cmdline tool which can be used to verify, install and
upgrade given package sets. 

%package	semistatic
Summary:	poldek linked almost statically (except glibc)
Group:		Utilities/System
Group(pl):	Narz�dzia/System

%prep 
%setup -q 

%build
%configure
make 

%install
rm -rf $RPM_BUILD_ROOT
install -d $RPM_BUILD_ROOT%{_prefix}

make install DESTDIR=$RPM_BUILD_ROOT

gzip -9nf README* *sample*

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(644,root,root,755)
%attr(755,root,root) %{_bindir}/%{name}
%{_mandir}/man1/%{name}*
%doc README* *sample*

%files static
%defattr(644,root,root,755)
%{_bindir}/%{name}.static

%define date    %(echo `LC_ALL="C" date +"%a %b %d %Y"`)
%changelog
* %{date} PLD Team <pld-list@pld.org.pl>
All persons listed below can be reached at <cvs_login>@pld.org.pl

$Log$
Revision 1.2  2000/10/29 02:47:11  mis
- packages user-level data added to Packages (NFY)
- starting develeop interactive tool
- fixed a lot of bugs and added many code improvements
- rpmlib(...) capabilities support

Revision 1.1  2000/09/22 18:19:45  mis
- set him free ;-)

