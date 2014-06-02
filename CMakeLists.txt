CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT(deviced C)

########################################################
# Build options:
########################################################
IF("${ARCH}" STREQUAL "emulator")
	OPTION(USE_EMULATOR "Use Emulator" ON)
ELSEIF("${ARCH}" STREQUAL "arm")
	OPTION(USE_ARM "Use Arm" ON)
ENDIF()

########################################################
# Deviced CMakeLists.txt
########################################################
SET(PREFIX ${CMAKE_INSTALL_PREFIX})
SET(EXEC_PREFIX "${PREFIX}/bin")
SET(LIBDIR "${PREFIX}/lib")
SET(INCLUDEDIR "${PREFIX}/include/${PROJECT_NAME}")
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
	src/mmc/vfat.c
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
	src/apps/apps.c
)

FIND_PROGRAM(UNAME NAMES uname)
EXEC_PROGRAM("${UNAME}" ARGS "-m" OUTPUT_VARIABLE "ARCH")
IF(USE_EMULATOR)
SET(SRCS ${SRCS}
	src/mmc/ext4.c
	)
ENDIF(USE_EMULATOR)

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

SET(SRCS ${SRCS}
	src/haptic/haptic.c)

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR})
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/sysman)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/src)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/src/deviced)

SET(MOVINAND_FORMAT scripts/movi_format.sh)

INCLUDE(FindPkgConfig)

SET( local_pkgs
	ecore
	ecore-file
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
	libsystemd-daemon
)

IF(X11_SUPPORT)
SET( local_pkgs ecore-x ${local_pkgs} )
ENDIF(X11_SUPPORT)

pkg_check_modules(pkgs REQUIRED ${local_pkgs} )

FOREACH(flag ${pkgs_CFLAGS})
	SET(EXTRA_CFLAGS "${EXTRA_CFLAGS} ${flag}")
ENDFOREACH(flag)

SET(EXTRA_CFLAGS "${EXTRA_CFLAGS} -fvisibility=hidden")
SET(EXTRA_CFLAGS "${EXTRA_CFLAGS} -g -fno-omit-frame-pointer -finstrument-functions")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${EXTRA_CFLAGS}")

ADD_DEFINITIONS("-DPREFIX=\"${CMAKE_INSTALL_PREFIX}\"")
ADD_DEFINITIONS("-DFACTORYFS=\"$ENV{FACTORYFS}\"")
ADD_DEFINITIONS("-DENABLE_KEY_FILTER")
ADD_DEFINITIONS("-DENABLE_X_LCD_ONOFF")
ADD_DEFINITIONS("-DENABLE_DLOG_OUT")
ADD_DEFINITIONS("-DENABLE_PM_LOG")
IF(USE_ARM)
	ADD_DEFINITIONS("-DTARGET")
ENDIF()
ADD_DEFINITIONS("-DDEBUG -DENABLE_DLOG_OUT")

ADD_EXECUTABLE(${PROJECT_NAME} ${SRCS})
TARGET_LINK_LIBRARIES(${PROJECT_NAME} ${pkgs_LDFLAGS} "-ldl" "-lm" "-ludev")
INSTALL(TARGETS ${PROJECT_NAME} DESTINATION bin)

INSTALL(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/deviced/ DESTINATION include/${PROJECT_NAME}
		FILES_MATCHING
		PATTERN "*.h")

CONFIGURE_FILE(device-daemon.in device-daemon @ONLY)
CONFIGURE_FILE(${PROJECT_NAME}.pc.in ${PROJECT_NAME}.pc @ONLY)
INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.pc DESTINATION lib/pkgconfig)

INSTALL(FILES ${MOVINAND_FORMAT} DESTINATION bin)
INSTALL(PROGRAMS ${CMAKE_BINARY_DIR}/device-daemon DESTINATION bin)
INSTALL(FILES deviced.conf DESTINATION /etc/dbus-1/system.d)
INSTALL(PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/utils/mmc-smack-label DESTINATION bin)
INSTALL(FILES ${CMAKE_SOURCE_DIR}/packaging/${PROJECT_NAME}.rule DESTINATION /etc/smack/accesses2.d)
INSTALL(PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/utils/set_pmon DESTINATION bin)
INSTALL(PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/utils/regpmon DESTINATION bin)
INSTALL(PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/utils/pmon DESTINATION bin)

INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/systemd/deviced.service DESTINATION lib/systemd/system)
INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/systemd/deviced.socket  DESTINATION lib/systemd/system)
INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/fsck-msdos/LICENSE DESTINATION share/license RENAME fsck_msdosfs)

ADD_SUBDIRECTORY(src/libdeviced)
ADD_SUBDIRECTORY(restarter)
ADD_SUBDIRECTORY(sys_event)
ADD_SUBDIRECTORY(pm_event)
ADD_SUBDIRECTORY(sys_pci_noti)
ADD_SUBDIRECTORY(sysman)
ADD_SUBDIRECTORY(libslp-pm)
ADD_SUBDIRECTORY(haptic)
ADD_SUBDIRECTORY(devman)
ADD_SUBDIRECTORY(fsck-msdos)
