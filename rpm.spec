Summary: Programs for manipulating PostScript Type 1 fonts

Name: t1utils
Version: 1.7.2
Release: 1
Source: http://www.lcdf.org/type/t1utils-1.7.2.tar.gz

URL: http://www.lcdf.org/type/

Group: Utilities/Printing
Vendor: Little Cambridgeport Design Factory
Packager: Eddie Kohler <eddietwo@lcs.mit.edu>
Copyright: GPL

BuildRoot: /tmp/t1utils-build

%description
The t1utils package is a set of programs for
manipulating PostScript Type 1 fonts. It contains
programs to change between binary PFB format (for
storage), ASCII PFA format (for printing), a
human-readable and -editable ASCII format, and
Macintosh resource forks.

%prep
%setup

%build
CFLAGS="$RPM_OPT_FLAGS" ./configure --prefix=$RPM_BUILD_ROOT/usr
make

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/usr/bin $RPM_BUILD_ROOT/usr/man/man1
make install

%clean
rm -rf $RPM_BUILD_ROOT

%post

%files
%doc NEWS README
%attr(0755,root,root) /usr/bin/t1ascii
%attr(0755,root,root) /usr/bin/t1binary
%attr(0755,root,root) /usr/bin/t1asm
%attr(0755,root,root) /usr/bin/t1disasm
%attr(0755,root,root) /usr/bin/t1unmac
%attr(0644,root,root) /usr/man/man1/t1ascii.1
%attr(0644,root,root) /usr/man/man1/t1binary.1
%attr(0644,root,root) /usr/man/man1/t1asm.1
%attr(0644,root,root) /usr/man/man1/t1disasm.1
%attr(0644,root,root) /usr/man/man1/t1unmac.1
