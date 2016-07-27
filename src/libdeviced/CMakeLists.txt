CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT(libdeviced C)

SET(LIBDEVICED_SRCS
	control.c
	display.c
	dbus.c
	haptic.c
	led.c
	mmc.c
	storage.c
	usbhost.c
	deviced-conf.c
	deviced-noti.c
	deviced-util.c
)

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/src/deviced ${CMAKE_SOURCE_DIR}/src/shared)

INCLUDE(FindPkgConfig)
pkg_check_modules(libpkgs REQUIRED
	vconf
	dlog
	storage
	dbus-1
	dbus-glib-1
	edbus
	glib-2.0)

FOREACH(flag ${libpkgs_CFLAGS})
	SET(EXTRA_LIB_CFLAGS "${EXTRA_LIB_CFLAGS} ${flag}")
ENDFOREACH(flag)
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${EXTRA_LIB_CFLAGS}")

# libdeviced
ADD_LIBRARY(${PROJECT_NAME} SHARED ${LIBDEVICED_SRCS})
TARGET_LINK_LIBRARIES(${PROJECT_NAME} ${libpkgs_LDFLAGS})
SET_TARGET_PROPERTIES(${PROJECT_NAME} PROPERTIES VERSION ${VERSION})
# CMake Policy (CMP0002)
# The logical name of executable and library targes
# does not have to correspond to the physical file name built.
SET_TARGET_PROPERTIES(${PROJECT_NAME} PROPERTIES OUTPUT_NAME deviced)
INSTALL(TARGETS ${PROJECT_NAME} DESTINATION ${LIB_INSTALL_DIR} COMPONENT RuntimeLibraries)
