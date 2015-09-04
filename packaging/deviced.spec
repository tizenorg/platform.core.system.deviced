
#These options are DEACTIVATED by default.
%bcond_with x
%bcond_with wayland
%bcond_with emulator

# display, extcon, power, usb are always enable
%define battery_module off
%define block_module off
%define display_module on
%define extcon_module on
%define haptic_module off
%define led_module off
%define power_module on
%define telephony_module off
%define usb_module on
%define usbhost_module off

%if "%{?profile}" == "mobile"
%define battery_module on
%define block_module on
%define haptic_module on
%define led_module on
%define telephony_module on
%endif
%if "%{?profile}" == "wearable"
%define battery_module on
%define haptic_module on
%define telephony_module on
%endif
%if "%{?profile}" == "tv"
%define block_module on
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
Source3:    sysman.manifest
Source4:    libslp-pm.manifest
Source5:    haptic.manifest
Source6:    devman.manifest

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
BuildRequires:  pkgconfig(libtzplatform-config)
BuildRequires:  pkgconfig(notification)
BuildRequires:  pkgconfig(hwcommon)
%if %{?display_module} == on
%if %{with x}
BuildRequires:  pkgconfig(x11)
BuildRequires:  pkgconfig(xext)
%endif
BuildRequires:  pkgconfig(libinput)
BuildRequires:  pkgconfig(sensor)
%endif
%if %{?telephony_module} == on
BuildRequires:  pkgconfig(tapi)
%endif

Requires: %{name}-tools = %{version}-%{release}
%{?systemd_requires}
Requires(post): /usr/bin/vconftool

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

%package -n sysman
Summary:    Sysman library
License:    Apache-2.0
Group:      System/Libraries
Requires:   libdeviced = %{version}-%{release}

%description -n sysman
sysman library.

%package -n sysman-devel
Summary:    Sysman devel library
License:    Apache-2.0
Group:      System/Development
Requires:   sysman = %{version}-%{release}

%description -n sysman-devel
sysman devel library.

%package -n sysman-internal-devel
Summary:    Sysman internal devel library
License:    Apache-2.0
Group:      System/Development
Requires:   sysman = %{version}-%{release}

%description -n sysman-internal-devel
sysman internal devel library.

%package -n libslp-pm
Summary:    Power manager client
Group:      System/Libraries
Requires:   libdeviced = %{version}-%{release}

%description -n libslp-pm
power-manager library.

%package -n libslp-pm-devel
Summary:    Power manager client (devel)
Group:      System/Development
Requires:   libslp-pm = %{version}-%{release}

%description -n libslp-pm-devel
power-manager devel library.

%package -n libhaptic
Summary:    Haptic library
Group:      Development/Libraries

%description -n libhaptic
Haptic library for device control

%package -n libhaptic-devel
Summary:    Haptic library for (devel)
Group:      Development/Libraries
Requires:   libhaptic = %{version}-%{release}

%description -n libhaptic-devel
Haptic library for device control (devel)

%package -n libhaptic-plugin-devel
Summary:    Haptic plugin library for (devel)
Group:      Development/Libraries
Requires:   libhaptic = %{version}-%{release}

%description -n libhaptic-plugin-devel
Haptic plugin library for device control (devel)

%package -n libdevman
Summary:    Device manager library
Group:      Development/Libraries
Requires:   libdeviced = %{version}-%{release}

%description -n libdevman
Device manager library for device control

%package -n libdevman-devel
Summary:    Device manager library for (devel)
Group:      Development/Libraries
Requires:   libdevman = %{version}-%{release}

%description -n libdevman-devel
Device manager library for device control (devel)

%package -n libdevman-haptic-devel
Summary:    Haptic Device manager library for (devel)
Group:      Development/Libraries
Requires:   libdevman-devel = %{version}-%{release}

%description -n libdevman-haptic-devel
Haptic Device manager library for device control (devel)

%prep
%setup -q
%if %{with emulator}
%define ARCH emulator
%else
%define ARCH arm
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
	-DDPMS=%{DPMS} \
	-DPROFILE=%{profile} \
	-DBATTERY_MODULE=%{battery_module} \
	-DBLOCK_MODULE=%{block_module} \
	-DDISPLAY_MODULE=%{display_module} \
	-DEXTCON_MODULE=%{extcon_module} \
	-DHAPTIC_MODULE=%{haptic_module} \
	-DLED_MODULE=%{led_module} \
	-DPOWER_MODULE=%{power_module} \
	-DTELEPHONY_MODULE=%{telephony_module} \
	-DUSB_MODULE=%{usb_module} \
	-DUSBHOST_MODULE=%{usbhost_module} \
	#eol

%build
cp %{SOURCE1} .
cp %{SOURCE2} .
cp %{SOURCE3} .
cp %{SOURCE4} .
cp %{SOURCE5} .
cp %{SOURCE6} .
make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
%make_install

%install_service multi-user.target.wants deviced.service
%install_service sockets.target.wants deviced.socket
%install_service graphical.target.wants zbooting-done.service

%post
#memory type vconf key init
users_gid=$(getent group %{TZ_SYS_USER_GROUP} | cut -f3 -d':')

vconftool set -t int memory/sysman/usbhost_status -1 -i
vconftool set -t int memory/sysman/mmc -1 -i
vconftool set -t int memory/sysman/earjack_key 0 -i
vconftool set -t int memory/sysman/charger_status -1 -i
vconftool set -t int memory/sysman/charge_now -1 -i
vconftool set -t int memory/sysman/battery_status_low -1 -i
vconftool set -t int memory/sysman/battery_capacity -1 -i
vconftool set -t int memory/sysman/usb_status -1 -i
vconftool set -t int memory/sysman/earjack -1 -i
vconftool set -t int memory/sysman/mmc_mount -1 -i
vconftool set -t int memory/sysman/mmc_format -1 -i
vconftool set -t int memory/sysman/mmc_format_progress 0 -i
vconftool set -t int memory/sysman/power_off 0 -g "$users_gid" -i -f
vconftool set -t int memory/sysman/battery_level_status -1 -i
vconftool set -t int memory/sysman/hdmi 0 -i
vconftool set -t int memory/sysman/stime_changed 0 -i

vconftool set -t int memory/pm/state 0 -i -g "$users_gid"
vconftool set -t int memory/pm/battery_timetofull -1 -i
vconftool set -t int memory/pm/battery_timetoempty -1 -i
vconftool set -t int memory/pm/custom_brightness_status 0 -i -g "$users_gid"
vconftool set -t bool memory/pm/brt_changed_lpm 0 -i
vconftool set -t int memory/pm/current_brt 60 -i -g "$users_gid"

systemctl daemon-reload
if [ "$1" == "1" ]; then
    systemctl restart deviced.service
    systemctl restart zbooting-done.service
fi

%preun
if [ "$1" == "0" ]; then
    systemctl stop deviced.service
    systemctl stop zbooting-done.service
fi

%postun
systemctl daemon-reload

%post -n libdeviced -p /sbin/ldconfig

%postun -n libdeviced -p /sbin/ldconfig

%post -n sysman -p /sbin/ldconfig

%postun -n sysman -p /sbin/ldconfig

%post -n libslp-pm -p /sbin/ldconfig

%postun -n libslp-pm -p /sbin/ldconfig

%post -n libhaptic -p /sbin/ldconfig

%postun -n libhaptic -p /sbin/ldconfig

%post -n libdevman -p /sbin/ldconfig

%postun -n libdevman -p /sbin/ldconfig

%files -n deviced
%manifest %{name}.manifest
%license LICENSE
%config %{_sysconfdir}/dbus-1/system.d/deviced.conf
%{_bindir}/deviced-pre.sh
%{_bindir}/deviced
%{_bindir}/movi_format.sh
%{_unitdir}/multi-user.target.wants/deviced.service
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
%{_bindir}/mmc-smack-label
%{_bindir}/fsck_msdosfs
%{_bindir}/newfs_msdos
%{_datadir}/license/fsck_msdosfs
%{_datadir}/license/newfs_msdos
%config %{_sysconfdir}/deviced/block.conf
%endif
%if %{?usb_module} == on
%config %{_sysconfdir}/deviced/usb-setting.conf
%config %{_sysconfdir}/deviced/usb-operation.conf
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

%files -n sysman
%manifest sysman.manifest
%defattr(-,root,root,-)
%{_libdir}/libsysman.so.*

%files -n sysman-devel
%defattr(-,root,root,-)
%{_includedir}/sysman/sysman.h
%{_includedir}/sysman/sysman_managed.h
%{_includedir}/sysman/SLP_sysman_PG.h
%{_libdir}/pkgconfig/sysman.pc
%{_libdir}/libsysman.so

%files -n sysman-internal-devel
%defattr(-,root,root,-)
%{_includedir}/sysman/sysman-internal.h

%files -n libslp-pm
%defattr(-,root,root,-)
%manifest libslp-pm.manifest
%{_libdir}/libpmapi.so.*

%files -n libslp-pm-devel
%defattr(-,root,root,-)
%{_includedir}/pmapi/pmapi.h
%{_includedir}/pmapi/pmapi_managed.h
%{_includedir}/pmapi/SLP_pm_PG.h
%{_libdir}/pkgconfig/pmapi.pc
%{_libdir}/libpmapi.so

%files -n libhaptic
%defattr(-,root,root,-)
%{_libdir}/libhaptic.so.*
%manifest haptic.manifest

%files -n libhaptic-devel
%defattr(-,root,root,-)
%{_includedir}/haptic/haptic.h
%{_libdir}/libhaptic.so
%{_libdir}/pkgconfig/haptic.pc

%files -n libhaptic-plugin-devel
%defattr(-,root,root,-)
%{_includedir}/haptic/haptic_module.h
%{_includedir}/haptic/haptic_plugin_intf.h
%{_includedir}/haptic/SLP_haptic_PG.h
%{_libdir}/pkgconfig/haptic-plugin.pc

%files -n libdevman
%{_bindir}/display_wd
%{_libdir}/libdevman.so.*
%manifest devman.manifest

%files -n libdevman-devel
%{_includedir}/devman/devman.h
%{_includedir}/devman/devman_image.h
%{_includedir}/devman/devman_managed.h
%{_includedir}/devman/devman_haptic.h
%{_includedir}/devman/SLP_devman_PG.h
%{_libdir}/pkgconfig/devman.pc
%{_libdir}/libdevman.so

%files -n libdevman-haptic-devel
%{_includedir}/devman/devman_haptic_ext.h
%{_includedir}/devman/devman_haptic_ext_core.h
%{_libdir}/pkgconfig/devman_haptic.pc
