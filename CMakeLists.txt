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
SET(INCLUDEDIR "${PREFIX}/include/${PROJECT_NAME}")
SET(DATADIR "${PREFIX}/share/${PROJECT_NAME}")
SET(CONFDIR "/etc/${PROJECT_NAME}")
SET(VERSION 0.1.0)

SET(SRCS
	src/core/common.c
	src/core/config-parser.c
	src/control/control.c
	src/core/device-notifier.c
	src/core/devices.c
	src/core/edbus-handler.c
	src/core/execute.c
	src/core/launch.c
	src/core/log.c
	src/core/main.c
	src/core/sig-handler.c
	src/core/udev.c
	src/extcon/cradle.c
	src/extcon/earjack.c
	src/extcon/extcon.c
	src/extcon/hdmi.c
	src/gpio/gpio.c
	src/power/power-handler.c
	src/proc/cpu-info.c
	src/proc/proc-handler.c
	src/time/time-handler.c
)

SET(SRCS ${SRCS}
	src/usb/usb.c
	src/usb/usb-default.c
	src/usb/usb-host.c
)

IF(TIZEN_BUZZER)
SET(SRCS ${SRCS}
	src/gpio/buzzer.c)
ENDIF(TIZEN_BUZZER)

SET(SRCS ${SRCS}
	src/battery/battery-time.c
	src/battery/config.c
	src/battery/lowbat-handler.c
	src/battery/power-supply.c
   )

IF(TIZEN_HALL)
SET(SRCS ${SRCS}
	src/gpio/hall.c)
ENDIF(TIZEN_HALL)

SET(SRCS ${SRCS}
	src/mmc/config.c
	src/mmc/mmc-handler.c
	src/mmc/uevent.c
	src/mmc/vfat.c
)
IF(USE_EMULATOR)
SET(SRCS ${SRCS}
	src/mmc/ext4.c
	)
ENDIF(USE_EMULATOR)

IF(TIZEN_SIM)
SET(SRCS ${SRCS}
	src/gpio/sim.c)
ENDIF(TIZEN_SIM)

IF(TIZEN_DISPLAY)
SET(SRCS ${SRCS}
	src/display/auto-brightness.c
	src/display/brightness.c
	src/display/core.c
	src/display/device-interface.c
	src/display/display-actor.c
	src/display/display-dbus.c
	src/display/display-ops.c
	src/display/key-filter.c
	src/display/lock-detector.c
	src/display/poll.c
	src/display/setting.c
	src/display/input.c
)
ENDIF(TIZEN_DISPLAY)

SET(SRCS ${SRCS}
	src/led/ir.c
	src/led/noti.c
	src/led/torch.c
)

SET(SRCS ${SRCS}
	src/haptic/haptic.c
	src/haptic/external.c
	src/haptic/standard.c
	src/haptic/emulator.c)

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR})
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/src)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/src/deviced)

SET(PKG_MODULES
	ecore
	edbus
	eina
	vconf
	dlog
	device-node
	capi-base-common
	sensor
	tapi
	libtzplatform-config
	notification
	hwcommon
	libinput
)

IF(X11_SUPPORT)
SET(PKG_MODULES ${PKG_MODULES}
	ecore-x
)
ENDIF(X11_SUPPORT)

INCLUDE(FindPkgConfig)
pkg_check_modules(pkgs REQUIRED ${PKG_MODULES})

FOREACH(flag ${pkgs_CFLAGS})
	SET(EXTRA_CFLAGS "${EXTRA_CFLAGS} ${flag}")
ENDFOREACH(flag)

SET(EXTRA_CFLAGS "${EXTRA_CFLAGS} -fvisibility=hidden")
SET(EXTRA_CFLAGS "${EXTRA_CFLAGS} -g -fno-omit-frame-pointer -finstrument-functions")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${EXTRA_CFLAGS} -lrt")
MESSAGE("FLAGS: ${CMAKE_C_FLAGS}")

ADD_DEFINITIONS("-DPREFIX=\"${CMAKE_INSTALL_PREFIX}\"")
ADD_DEFINITIONS("-DFACTORYFS=\"$ENV{FACTORYFS}\"")
ADD_DEFINITIONS("-DLIBPATH=\"${LIB_INSTALL_DIR}\"")
ADD_DEFINITIONS("-DENABLE_DEVICED_DLOG")
ADD_DEFINITIONS("-DENABLE_LIBDEVICED_DLOG")
ADD_DEFINITIONS("-DENABLE_PM_LOG")
IF(USE_ARM)
	ADD_DEFINITIONS("-DTARGET")
ELSEIF(USE_EMULATOR)
	ADD_DEFINITIONS("-DEMULATOR")
ENDIF()
ADD_DEFINITIONS("-DDEBUG")

ADD_EXECUTABLE(${PROJECT_NAME} ${SRCS})
TARGET_LINK_LIBRARIES(${PROJECT_NAME} ${pkgs_LDFLAGS} "-ldl" "-lm" "-ludev" "-ledbus" shared)
INSTALL(TARGETS ${PROJECT_NAME} DESTINATION bin)

INSTALL(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/deviced/ DESTINATION include/${PROJECT_NAME}
		FILES_MATCHING
		PATTERN "*_doc.h" EXCLUDE
		PATTERN "*.h")

CONFIGURE_FILE(${PROJECT_NAME}.pc.in ${PROJECT_NAME}.pc @ONLY)
INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.pc DESTINATION ${LIB_INSTALL_DIR}/pkgconfig)

INSTALL(PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/scripts/deviced-pre.sh DESTINATION bin)
INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/scripts/deviced.conf DESTINATION /etc/dbus-1/system.d)
INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/scripts/movi_format.sh DESTINATION bin)
INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/usb/usb-setting.conf DESTINATION /etc/deviced)
INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/usb/usb-operation.conf DESTINATION /etc/deviced)
INSTALL(PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/scripts/mmc-smack-label DESTINATION bin)
INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/fsck-msdos/LICENSE DESTINATION share/license RENAME fsck_msdosfs)

# USB (Manual setting)
INSTALL(PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/scripts/direct_set_debug.sh DESTINATION bin)

INSTALL(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/systemd/ DESTINATION lib/systemd/system
		FILES_MATCHING
		PATTERN "*.service"
		PATTERN "*.socket")

ADD_SUBDIRECTORY(src)
ADD_SUBDIRECTORY(src/libdeviced)
ADD_SUBDIRECTORY(src/fsck-msdos)
ADD_SUBDIRECTORY(src/newfs-msdos)
ADD_SUBDIRECTORY(src/devicectl)
ADD_SUBDIRECTORY(src/libsysman)
ADD_SUBDIRECTORY(src/libslp-pm)
ADD_SUBDIRECTORY(src/libhaptic)
ADD_SUBDIRECTORY(src/libdevman)
