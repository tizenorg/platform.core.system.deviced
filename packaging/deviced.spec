
#These options are DEACTIVATED by default.
%bcond_with x
%bcond_with wayland
%bcond_with emulator

# display, extcon, power, usb are always enable
%define battery_module off
%define block_module on
%define block_set_permission on
%define block_tmpfs off
%define display_module on
%define extcon_module on
%define haptic_module off
%define ir_module off
%define led_module off
%define power_module on
%define telephony_module off
%define touchscreen_module off
%define tzip_module off
%define usb_module on
%define usbhost_module off

#Just For debugging
%define sdb_prestart off

# Support two pattern combination vibration
%define standard_mix off

%if "%{?profile}" == "mobile"
%define battery_module on
%define haptic_module on
%define ir_module on
%define led_module on
%define telephony_module on
%define touchscreen_module on
%define tzip_module on
%define usbhost_module on
%endif
%if "%{?profile}" == "wearable"
%define battery_module on
%define haptic_module on
%define telephony_module on
%define touchscreen_module on
%define tzip_module on
%endif
%if "%{?profile}" == "tv"
%define block_tmpfs on
%define sdb_prestart off
%define usbhost_module on
%endif
%if "%{?profile}" == "ivi"
%if "%{?_repository}" == "x86_64"
%define block_set_permission off
%endif
%endif

Name:       deviced
Summary:    Deviced
Version:    1.0.0
Release:    1
Group:      System/Management
License:    Apache-2.0
Source0:    %{name}-%{version}.tar.gz
Source1:    deviced.manifest
Source2:    libdeviced.manifest

BuildRequires:  cmake
BuildRequires:  libattr-devel
BuildRequires:  gettext-devel
BuildRequires:  pkgconfig(ecore)
BuildRequires:  pkgconfig(vconf)
BuildRequires:  pkgconfig(dlog)
BuildRequires:  pkgconfig(device-node)
BuildRequires:  pkgconfig(edbus)
BuildRequires:  pkgconfig(capi-base-common)
BuildRequires:  pkgconfig(dbus-1)
BuildRequires:  pkgconfig(dbus-glib-1)
BuildRequires:  pkgconfig(libudev)
BuildRequires:  pkgconfig(gio-2.0)
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:	pkgconfig(eventsystem)
BuildRequires:  pkgconfig(libtzplatform-config)
BuildRequires:  pkgconfig(hwcommon)
%if %{?display_module} == on
%if %{with x}
BuildRequires:  pkgconfig(x11)
BuildRequires:  pkgconfig(xext)
%endif
BuildRequires:  pkgconfig(libinput)
BuildRequires:	pkgconfig(capi-system-sensor)
%endif
%if %{?block_module} == on
BuildRequires:	pkgconfig(storage)
%endif
%if %{?telephony_module} == on
BuildRequires:  pkgconfig(tapi)
%endif
%if %{?tzip_module} == on
BuildRequires:	pkgconfig(fuse)
BuildRequires:	pkgconfig(minizip)
%endif

Requires: %{name}-tools = %{version}-%{release}
%{?systemd_requires}
Requires(post): /usr/bin/vconftool

%if %{?block_module} == on
Requires: /usr/bin/fsck_msdosfs
Requires: /usr/bin/newfs_msdos
%endif

%description
deviced

%package deviced
Summary:    deviced daemon
Group:      main

%description deviced
deviced daemon.

%package tools
Summary:  Deviced tools
Group:    System/Utilities

%description tools
Deviced helper programs

%package -n libdeviced
Summary:    Deviced library
Group:      Development/Libraries
Requires:   %{name} = %{version}-%{release}

%description -n libdeviced
Deviced library for device control

%package -n libdeviced-devel
Summary:    Deviced library for (devel)
Group:      Development/Libraries
Requires:   libdeviced = %{version}-%{release}

%description -n libdeviced-devel
Deviced library for device control (devel)

%prep
%setup -q
%if %{with emulator}
	%define ARCH emulator
%else
	%ifarch %{arm} aarch64
		%define ARCH arm
	%else
		%define ARCH x86
	%endif
%endif

%ifarch %{arm} %ix86
	%define ARCH_BIT 32
%else
	%define ARCH_BIT 64
%endif

%define DPMS none
%if %{with x}
%define DPMS x
%endif
%if %{with wayland}
%define DPMS wayland
%endif

%cmake . \
	-DTZ_SYS_ETC=%TZ_SYS_ETC \
	-DCMAKE_INSTALL_PREFIX=%{_prefix} \
	-DARCH=%{ARCH} \
	-DARCH_BIT=%{ARCH_BIT} \
	-DDPMS=%{DPMS} \
	-DPROFILE=%{profile} \
	-DBATTERY_MODULE=%{battery_module} \
	-DBLOCK_MODULE=%{block_module} \
	-DBLOCK_SET_PERMISSION=%{block_set_permission} \
	-DBLOCK_TMPFS=%{block_tmpfs} \
	-DDISPLAY_MODULE=%{display_module} \
	-DEXTCON_MODULE=%{extcon_module} \
	-DHAPTIC_MODULE=%{haptic_module} \
	-DSTANDARD_MIX=%{standard_mix} \
	-DIR_MODULE=%{ir_module} \
	-DLED_MODULE=%{led_module} \
	-DPOWER_MODULE=%{power_module} \
	-DTELEPHONY_MODULE=%{telephony_module} \
	-DTOUCHSCREEN_MODULE=%{touchscreen_module} \
	-DTZIP_MODULE=%{tzip_module} \
	-DUSB_MODULE=%{usb_module} \
	-DUSBHOST_MODULE=%{usbhost_module} \
	#eol

%build
cp %{SOURCE1} .
cp %{SOURCE2} .
make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
%make_install

%install_service multi-user.target.wants deviced.service
%install_service sockets.target.wants deviced.socket
%install_service graphical.target.wants zbooting-done.service

%if %{?haptic_module} == on
%install_service multi-user.target.wants deviced-vibrator.service
%endif

%if %{?sdb_prestart} == on
%install_service basic.target.wants sdb-prestart.service
%endif
%if %{?usbhost_module} == on
mkdir -p %{buildroot}%{_prefix}/lib/udev/rules.d
install -m 644 udev/99-usbhost.rules %{buildroot}%{_prefix}/lib/udev/rules.d/99-usbhost.rules
%endif

%post
#memory type vconf key init
users_gid=$(getent group %{TZ_SYS_USER_GROUP} | cut -f3 -d':')

systemctl daemon-reload
if [ "$1" == "1" ]; then
    systemctl restart deviced.service
%if %{?haptic_module} == on
    systemctl restart deviced-vibrator.service
%endif
    systemctl restart zbooting-done.service
fi

%preun
if [ "$1" == "0" ]; then
    systemctl stop deviced.service
%if %{?haptic_module} == on
    systemctl stop deviced-vibrator.service
%endif
    systemctl stop zbooting-done.service
fi

%postun
systemctl daemon-reload

%post -n libdeviced -p /sbin/ldconfig

%postun -n libdeviced -p /sbin/ldconfig

%files -n deviced
%manifest %{name}.manifest
%license LICENSE
%config %{_sysconfdir}/dbus-1/system.d/deviced.conf
%{_bindir}/deviced-pre.sh
%{_bindir}/deviced
%{_unitdir}/multi-user.target.wants/deviced.service
%if %{?haptic_module} == on
%{_unitdir}/deviced-vibrator.service
%{_unitdir}/multi-user.target.wants/deviced-vibrator.service
%{_bindir}/deviced-vibrator
%endif
%{_unitdir}/sockets.target.wants/deviced.socket
%{_unitdir}/graphical.target.wants/zbooting-done.service
%{_unitdir}/deviced.service
%{_unitdir}/deviced.socket
%{_unitdir}/deviced-pre.service
%{_unitdir}/zbooting-done.service
%if %{?battery_module} == on
%config %{_sysconfdir}/deviced/battery.conf
%endif
%if %{?block_module} == on
%if %{?block_set_permission} == on
%{_bindir}/mmc-smack-label
%endif
%config %{_sysconfdir}/deviced/block.conf
%config %{_sysconfdir}/deviced/storage.conf
%endif
%if %{?display_module} == on
%config %{_sysconfdir}/deviced/display.conf
%endif
%if %{?usb_module} == on
%config %{_sysconfdir}/deviced/usb-setting.conf
%config %{_sysconfdir}/deviced/usb-operation.conf
%endif
%if %{?usbhost_module} == on
%{_prefix}/lib/udev/rules.d/99-usbhost.rules
%endif

%{_unitdir}/sdb-prestart.service
%if %{?sdb_prestart} == on
%{_unitdir}/basic.target.wants/sdb-prestart.service
%endif

%files tools
%manifest %{name}.manifest
%{_bindir}/devicectl
%if %{?usb_module} == on
%{_bindir}/direct_set_debug.sh
%endif

%files -n libdeviced
%manifest deviced.manifest
%defattr(-,root,root,-)
%{_libdir}/libdeviced.so.*

%files -n libdeviced-devel
%defattr(-,root,root,-)
%{_includedir}/deviced/*.h
%{_libdir}/libdeviced.so
%{_libdir}/pkgconfig/deviced.pc
