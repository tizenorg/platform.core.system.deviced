CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT(system_server C)

SET(PREFIX ${CMAKE_INSTALL_PREFIX})
SET(EXEC_PREFIX "${PREFIX}/bin")
SET(INCLUDEDIR "${PREFIX}/include/${PROJECT_NAME}")
SET(LIBDIR "\${prefix}/lib")

# deviced
SET(DEVICED_NAME deviced)
SET(VERSION 0.1.0)

SET(SRCS
	src/battery/lowbat-handler.c
	src/bs/bs.c
	src/core/device-notifier.c
	src/core/main.c
	src/core/sysnoti.c
	src/core/launch.c
	src/core/queue.c
	src/core/core.c
	src/core/devices.c
	src/core/sig-handler.c
	src/core/log.c
	src/core/device-change-handler.c
	src/core/predefine.c
	src/core/noti.c
	src/core/common.c
	src/core/edbus-handler.c
	src/cpu/cpu-handler.c
	src/mmc/mmc-handler.c
	src/power/power-handler.c
	src/proc/lowmem-handler.c
	src/proc/pmon-handler.c
	src/proc/proc-handler.c
	src/ta/ta-handler.c
	src/time/time-handler.c
	src/usb/usb-handler.c
	src/vibrator/vibrator.c
)

SET(SRCS ${SRCS}
	src/display/util.c
	src/display/llinterface.c
	src/display/conf.c
	src/display/setting.c
	src/display/poll.c
	src/display/core.c
	src/display/display-dbus.c
	src/display/lsensor.c
	src/display/key-filter.c
	src/display/battery.c)

SET(SRCS ${SRCS}
	src/led/led.c)

SET(SRCS ${SRCS}
	src/control/control.c)

# libdeviced
SET(DEVICED_SRCS
	src/shared/battery.c
	src/shared/control.c
	src/shared/deviced-conf.c
	src/shared/deviced-noti.c
	src/shared/deviced-util.c
	src/shared/display.c
	src/shared/haptic.c
	src/shared/led.c
)

SET(DEVICED_HEADERS
	src/deviced/dd-battery.h
	src/deviced/dd-control.h
	src/deviced/dd-deviced.h
	src/deviced/dd-deviced-managed.h
	src/deviced/dd-display.h
	src/deviced/dd-haptic.h
	src/deviced/dd-led.h
	src/deviced/haptic-module.h
	src/deviced/haptic-plugin-intf.h
)

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/sysman ${CMAKE_CURRENT_SOURCE_DIR}/src)

# libdeviced
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/src/deviced)

SET(MOVINAND_FORMAT scripts/movi_format.sh)

INCLUDE(FindPkgConfig)
pkg_check_modules(pkgs REQUIRED
	ecore
	ecore-file
	ecore-x
	edbus
	eina
	vconf
	heynoti
	tapi
	dlog
	syspopup-caller
	device-node
	sensor
	notification
	libsmack
	libsystemd-daemon)

FOREACH(flag ${pkgs_CFLAGS})
	SET(EXTRA_CFLAGS "${EXTRA_CFLAGS} ${flag}")
ENDFOREACH(flag)

SET(EXTRA_CFLAGS "${EXTRA_CFLAGS} -fvisibility=hidden")
SET(EXTRA_CFLAGS "${EXTRA_CFLAGS} -g -fno-omit-frame-pointer -finstrument-functions")

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${EXTRA_CFLAGS}")
SET(PREFIX ${CMAKE_INSTALL_PREFIX})

ADD_DEFINITIONS("-DPREFIX=\"${CMAKE_INSTALL_PREFIX}\"")
ADD_DEFINITIONS("-DFACTORYFS=\"$ENV{FACTORYFS}\"")

ADD_DEFINITIONS("-DENABLE_KEY_FILTER")
ADD_DEFINITIONS("-DENABLE_X_LCD_ONOFF")
ADD_DEFINITIONS("-DENABLE_DLOG_OUT")
ADD_DEFINITIONS("-DENABLE_PM_LOG")

IF( $ENV{ARCH} MATCHES "arm" )
	ADD_DEFINITIONS("-DTARGET")
ENDIF()
ADD_DEFINITIONS("-DDEBUG -DENABLE_DLOG_OUT")

SET(UDEV_RULES_PATH share/system-server/udev-rules)
SET(UDEV_RULES udev-rules/91-system-server.rules)

CONFIGURE_FILE(${UDEV_RULES}.in ${UDEV_RULES} @ONLY)
CONFIGURE_FILE(device-daemon.in device-daemon @ONLY)

# libdeviced
ADD_LIBRARY(${DEVICED_NAME} SHARED ${DEVICED_SRCS})
TARGET_LINK_LIBRARIES(${DEVICED_NAME} ${pkgs_LDFLAGS})
SET_TARGET_PROPERTIES(${DEVICED_NAME} PROPERTIES VERSION ${VERSION})
INSTALL(TARGETS ${DEVICED_NAME} DESTINATION lib COMPONENT RuntimeLibraries)

FOREACH(hfile ${DEVICED_HEADERS})
	INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/${hfile} DESTINATION include/${DEVICED_NAME})
ENDFOREACH(hfile)

CONFIGURE_FILE(${DEVICED_NAME}.pc.in ${DEVICED_NAME}.pc @ONLY)
INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/${DEVICED_NAME}.pc DESTINATION lib/pkgconfig)

ADD_EXECUTABLE(${PROJECT_NAME} ${SRCS})
TARGET_LINK_LIBRARIES(${PROJECT_NAME} ${pkgs_LDFLAGS} "-ldl" "-ludev")
INSTALL(TARGETS ${PROJECT_NAME} DESTINATION bin)

INSTALL(FILES ${MOVINAND_FORMAT} DESTINATION bin)
INSTALL(FILES ${UDEV_RULES} DESTINATION ${UDEV_RULES_PATH})
INSTALL(PROGRAMS ${CMAKE_BINARY_DIR}/device-daemon DESTINATION bin)
INSTALL(FILES system-server.conf DESTINATION /etc/dbus-1/system.d)
INSTALL(FILES ${CMAKE_SOURCE_DIR}/packaging/system-server.rule DESTINATION /opt/etc/smack/accesses.d)
INSTALL(PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/utils/mmc-smack-label DESTINATION bin)
INSTALL(PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/utils/set_pmon DESTINATION bin)
INSTALL(PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/utils/regpmon DESTINATION bin)
INSTALL(PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/utils/pmon DESTINATION bin)

INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/systemd/system-server.service DESTINATION lib/systemd/system)
INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/systemd/system-server.socket  DESTINATION lib/systemd/system)

ADD_SUBDIRECTORY(restarter)
ADD_SUBDIRECTORY(sys_event)
ADD_SUBDIRECTORY(pm_event)
ADD_SUBDIRECTORY(sys_pci_noti)
ADD_SUBDIRECTORY(sysman)
ADD_SUBDIRECTORY(libslp-pm)
ADD_SUBDIRECTORY(haptic)
ADD_SUBDIRECTORY(devman)
