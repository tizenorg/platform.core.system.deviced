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

# libdeviced
SET(DEVICED_SRCS
	src/shared/battery.c)

SET(DEVICED_HEADERS
	src/deviced/dd-battery.h)

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR})

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
	sysman
	vconf
	heynoti
	pmapi
	tapi
	dlog
	syspopup-caller
	notification
	device-node
	libsmack
	libsystemd-daemon)

FOREACH(flag ${pkgs_CFLAGS})
	SET(EXTRA_CFLAGS "${EXTRA_CFLAGS} ${flag}")
ENDFOREACH(flag)

# libdeviced
SET(EXTRA_CFLAGS "${EXTRA_CFLAGS} -fvisibility=hidden")

SET(EXTRA_CFLAGS "${EXTRA_CFLAGS} -g -fno-omit-frame-pointer -finstrument-functions")

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${EXTRA_CFLAGS}")
SET(PREFIX ${CMAKE_INSTALL_PREFIX})
MESSAGE("FLAGS: ${CMAKE_C_FLAGS}")

ADD_DEFINITIONS("-DPREFIX=\"${CMAKE_INSTALL_PREFIX}\"")
ADD_DEFINITIONS("-DFACTORYFS=\"$ENV{FACTORYFS}\"")
IF( $ENV{ARCH} MATCHES "arm" )
	ADD_DEFINITIONS("-DTARGET")
ENDIF()
ADD_DEFINITIONS("-DDEBUG -DENABLE_DLOG_OUT")

SET(UDEV_RULES_PATH share/system-server/udev-rules)
SET(UDEV_RULES udev-rules/91-system-server.rules)

CONFIGURE_FILE(${UDEV_RULES}.in ${UDEV_RULES} @ONLY)

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
TARGET_LINK_LIBRARIES(${PROJECT_NAME} ${pkgs_LDFLAGS} "-ldl" "-ludev" "-lsmack")
INSTALL(TARGETS ${PROJECT_NAME} DESTINATION bin)

INSTALL(FILES ${MOVINAND_FORMAT} DESTINATION bin)
INSTALL(FILES ${UDEV_RULES} DESTINATION ${UDEV_RULES_PATH})
INSTALL(FILES system-server.conf DESTINATION /etc/dbus-1/system.d)
INSTALL(FILES ${CMAKE_SOURCE_DIR}/packaging/system-server.rule DESTINATION /opt/etc/smack/accesses.d)
INSTALL(PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/mmc-smack-label DESTINATION bin)

INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/systemd/system-server.service DESTINATION /usr/lib/systemd/system)
INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/systemd/system-server.socket  DESTINATION /usr/lib/systemd/system)

ADD_SUBDIRECTORY(restarter)
ADD_SUBDIRECTORY(sys_event)
ADD_SUBDIRECTORY(sys_device_noti)
ADD_SUBDIRECTORY(sys_pci_noti)
