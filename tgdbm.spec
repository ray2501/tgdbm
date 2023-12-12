%{!?directory:%define directory /usr}

%define buildroot %{_tmppath}/%{name}
%define packagename tgdbm

Name:          tcl-tgdbm
Summary:       Tcl interface to the Gdbm
Version:       0.7
Release:       0
License:       GPL-2.0-only
Group:         Development/Libraries/Tcl
Source:        %{packagename}-%{version}.tar.gz
URL:           https://github.com/ray2501/tgdbm
BuildRequires: autoconf
BuildRequires: gdbm-devel
BuildRequires: tcl-devel >= 8.4
Requires:      tcl >= 8.4
Requires:      libgdbm4
BuildRoot:     %{buildroot}

%description
This is a Tcl-Wrapper for the famous gdbm (the GNU-Version of dbm) and
a small database-utility "Qgdbm".

%prep
%setup -q -n %{packagename}-%{version}

%build
./configure \
	--prefix=%{directory} \
	--exec-prefix=%{directory} \
	--libdir=%{directory}/%{_lib} \
	--with-tcl=%{directory}/%{_lib}
make 

%install
make DESTDIR=%{buildroot} pkglibdir=%{tcl_archdir}/%{packagename}%{version} install

%clean
rm -rf %buildroot

%files
%defattr(-,root,root)
%{tcl_archdir}/%{packagename}%{version}

