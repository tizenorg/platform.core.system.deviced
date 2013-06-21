CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT(system_server C)

# deviced
SET(DEVICED_NAME deviced)
SET(VERSION 0.1.0)

SET(SRCS
	ss_main.c
	ss_sysnoti.c
	ss_launch.c
	ss_queue.c
	ss_core.c
	ss_sig_handler.c
	ss_log.c
	ss_device_change_handler.c
	ss_predefine.c
	ss_noti.c
	ss_lowbat_handler.c
	ss_lowmem_handler.c
	ss_pmon_handler.c
	ss_mmc_handler.c
	ss_usb_handler.c
	ss_ta_handler.c
	ss_bs.c
	ss_procmgr.c
	ss_timemgr.c
	ss_cpu_handler.c
	ss_common.c
	ss_vibrator.c
	edbus-handler.c)

SET(SRCS ${SRCS}
	display/util.c
	display/llinterface.c
	display/conf.c
	display/setting.c
	display/poll.c
	display/core.c
	display/lsensor.c
	display/key-filter.c
	display/battery.c)

# libdeviced
SET(DEVICED_SRCS
	src/shared/battery.c
	src/shared/display.c
	src/shared/led.c)

SET(DEVICED_HEADERS
	src/deviced/dd-battery.h
	src/deviced/dd-display.h
	src/deviced/dd-led.h)

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/display ${CMAKE_CURRENT_SOURCE_DIR}/sysman)

# libdeviced
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/src/deviced)

SET(MOVINAND_FORMAT movi_format.sh)

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
INSTALL(PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/mmc-smack-label DESTINATION bin)

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
