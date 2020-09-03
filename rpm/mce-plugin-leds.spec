Name:       mce-plugin-leds
Summary:    Libhybris plugin for Mode Control Entity
Version:    1.14.2
Release:    1
License:    LGPLv2.1
URL:        https://github.com/mer-hybris/mce-plugin-libhybris
Source0:    %{name}-%{version}.tar.bz2

Provides:   mce-plugin-libhybris = %{version}
Obsoletes:  mce-plugin-libhybris <= 1.14.2

Requires:         mce >= 1.12.10
Requires:         systemd
Requires(pre):    systemd
Requires(post):   systemd
Requires(preun):  systemd
Requires(postun): systemd

BuildRequires:  pkgconfig(glib-2.0) >= 2.18.0
BuildRequires:  systemd

%description
This package contains a mce plugin that allows mce to use led hardware.

%prep
%autosetup -n %{name}-%{version}

%build
%make_build

%install
rm -rf %{buildroot}
make install DESTDIR=%{buildroot} _LIBDIR=%{_libdir}

%pre
if [ "$1" = "2" ]; then
  # upgrade
  systemctl stop mce.service || :
fi

%post
# upgrade or install
systemctl restart mce.service || :

%preun
if [ "$1" = "0" ]; then
  # uninstall
  systemctl stop mce.service || :
fi

%postun
if [ "$1" = "0" ]; then
  # uninstall
  systemctl start mce.service || :
fi

%files
%defattr(-,root,root,-)
%doc COPYING
%{_libdir}/mce/modules/hybris.so
