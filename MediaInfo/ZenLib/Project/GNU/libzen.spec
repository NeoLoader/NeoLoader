%define libzen_version            0.4.30

Name:           libzen
Version:        %{libzen_version}
Release:        1
Summary:        C++ utility library

License:        Zlib
Group:          System/Libraries
URL:            http://sourceforge.net/projects/zenlib
Packager:       MediaArea.net SARL <info@mediaarea.net>
Source:         %{name}_%{version}-1.tar.gz

BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-root
BuildRequires:  gcc-c++
BuildRequires:  pkgconfig
BuildRequires:  glibc-devel
BuildRequires:  doxygen
BuildRequires:  libtool
BuildRequires:  automake
BuildRequires:  autoconf

%description
ZenLib is a C++ utility library. It includes classes for handling strings,
configuration, bit streams, threading, translation, and cross-platform
operating system functions.

%package -n %{name}0
Summary:		C++ utility library -- runtime
Group:			System/Libraries
Requires:		glibc

%description -n %{name}0
ZenLib is a C++ utility library. It includes classes for handling strings,
configuration, bit streams, threading, translation, and cross-platform
operating system functions.

This package contains the headers required for compiling applications/libraries
which use this library.

%package        doc
Summary:        C++ utility library -- documentation
Group:          Development/Libraries
Requires:       %{name}0 = %{version}

%description    doc
ZenLib is a C++ utility library. It includes classes for handling strings,
configuration, bit streams, threading, translation, and cross-platform
operating system functions.

This package contains the documentation

%package        devel
Summary:        C++ utility library -- development
Group:          Development/Libraries
Requires:    	%{name}0%{?_isa} = %{version}
Requires:    	glibc-devel

%description    devel
ZenLib is a C++ utility library. It includes classes for handling strings,
configuration, bit streams, threading, translation, and cross-platform
operating system functions.

This package contains the include files and mandatory libraries
for development.

%prep
%setup -q -n ZenLib
#Correct documentation encoding and permissions
sed -i 's/.$//' *.txt
chmod 644 *.txt Source/Doc/Documentation.html

chmod 644 Source/ZenLib/*.h Source/ZenLib/*.cpp \
    Source/ZenLib/Format/Html/*.h Source/ZenLib/Format/Html/*.cpp \
    Source/ZenLib/Format/Http/*.h Source/ZenLib/Format/Http/*.cpp

pushd Project/GNU/Library
    autoreconf -i
popd

%build
export CFLAGS="%{optflags}"
export CPPFLAGS="%{optflags}"
export CXXFLAGS="%{optflags}"

#Make documentation
pushd Source/Doc/
    doxygen Doxyfile
popd
cp Source/Doc/*.html ./

pushd Project/GNU/Library
    %configure --disable-static --enable-shared

    make clean
    make %{?_smp_mflags}
popd

%install
pushd Project/GNU/Library
    make install DESTDIR=%{buildroot}
popd

#Install headers and ZenLib-config
install -dm 755 %{buildroot}%{_includedir}/ZenLib
install -m 644 Source/ZenLib/*.h \
    %{buildroot}%{_includedir}/ZenLib
for i in HTTP_Client Format/Html Format/Http; do
    install -dm 755 %{buildroot}%{_includedir}/ZenLib/$i
    install -m 644 Source/ZenLib/$i/*.h \
        %{buildroot}%{_includedir}/ZenLib/$i
done

sed -i -e 's|Version: |Version: %{version}|g' \
    Project/GNU/Library/%{name}.pc
install -dm 755 %{buildroot}%{_libdir}/pkgconfig
install -m 644 Project/GNU/Library/%{name}.pc \
    %{buildroot}%{_libdir}/pkgconfig


%post -n %{name}0 -p /sbin/ldconfig

%postun -n %{name}0 -p /sbin/ldconfig

%files -n %{name}0
%defattr(-,root,root,-)
%doc History.txt License.txt ReadMe.txt
%{_libdir}/%{name}.so.*

%files doc
%defattr(-,root,root,-)
%doc Documentation.html
%doc Doc

%files devel
%defattr(-,root,root,-)
%{_bindir}/%{name}-config
%{_includedir}/ZenLib
%{_libdir}/%{name}.so
%{_libdir}/%{name}.la
%{_libdir}/pkgconfig/*.pc

%changelog
* Tue Jan 01 2009 MediaArea.net SARL <info@mediaarea.net> - 0.4.30-0
- See History.txt for more info and real dates
- Previous packages made by Toni Graffy <toni@links2linux.de>
- Fedora style made by Vasiliy N. Glazov <vascom2@gmail.com>
