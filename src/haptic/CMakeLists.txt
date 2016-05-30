CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT(deviced-vibrator C)

########################################################
# Build options:
# -DTIZEN_ENGINEER_MODE -
########################################################
IF("${ARCH}" STREQUAL "emulator")
	OPTION(USE_EMULATOR "Use Emulator" ON)
ELSEIF("${ARCH}" STREQUAL "arm")
	OPTION(USE_ARM "Use Arm" ON)
ENDIF()

IF("${ARCH_BIT}" STREQUAL "32")
	OPTION(USE_32BIT "Use 32bit architecture" ON)
ELSEIF("${ARCH_BIT}" STREQUAL "64")
	OPTION(USE_64BIT "Use 64bit architecture" ON)
ENDIF()

########################################################
# Deviced CMakeLists.txt
########################################################
SET(VERSION 0.1.0)

SET(SRCS
	main.c
	haptic.c
	standard.c
	emulator.c
	external.c
	edbus-handler.c
	${CMAKE_SOURCE_DIR}/src/core/common.c
	${CMAKE_SOURCE_DIR}/src/core/config-parser.c
	${CMAKE_SOURCE_DIR}/src/core/device-idler.c
	${CMAKE_SOURCE_DIR}/src/core/device-notifier.c
	${CMAKE_SOURCE_DIR}/src/core/devices.c
	${CMAKE_SOURCE_DIR}/src/core/execute.c
	${CMAKE_SOURCE_DIR}/src/core/log.c
	${CMAKE_SOURCE_DIR}/src/shared/dbus.c
)

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src)

SET(PKG_MODULES
	ecore
	edbus
	vconf
	dlog
	capi-base-common
	gio-2.0
)

INCLUDE(FindPkgConfig)
pkg_check_modules(pkgs2 REQUIRED ${PKG_MODULES})

FOREACH(flag ${pkgs2_CFLAGS})
	SET(EXTRA_CFLAGS "${EXTRA_CFLAGS} ${flag}")
ENDFOREACH(flag)

SET(EXTRA_CFLAGS "${EXTRA_CFLAGS} -fvisibility=hidden -Werror")
SET(EXTRA_CFLAGS "${EXTRA_CFLAGS} -g -fno-omit-frame-pointer -finstrument-functions")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${EXTRA_CFLAGS} -lrt -fPIE")
SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pie")

IF(USE_ARM)
	ADD_DEFINITIONS("-DTARGET")
ELSEIF(USE_EMULATOR)
	ADD_DEFINITIONS("-DEMULATOR")
ENDIF()
IF(USE_32BIT)
	ADD_DEFINITIONS("-DARCH_32BIT")
ELSEIF(USE_64BIT)
	ADD_DEFINITIONS("-DARCH_64BIT")
ENDIF()
ADD_DEFINITIONS("-DDEBUG")

ADD_EXECUTABLE(${PROJECT_NAME} ${SRCS})
TARGET_LINK_LIBRARIES(${PROJECT_NAME} ${pkgs2_LDFLAGS} "-ldl" "-lm")
INSTALL(TARGETS ${PROJECT_NAME} DESTINATION bin)

INSTALL(DIRECTORY ${CMAKE_SOURCE_DIR}/systemd/ DESTINATION lib/systemd/system
	FILES_MATCHING
	PATTERN "deviced-vibrator.service")
