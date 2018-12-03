CMAKE_MINIMUM_REQUIRED(VERSION 3.1)
PROJECT(stackmenu)

INCLUDE(GNUInstallDirs)
INCLUDE(FindPkgConfig)

SET(LIB_VERSION_MAJOR "0")
SET(LIB_VERSION_MINOR "1")
SET(LIB_VERSION_PATCH "0")
SET(VERSION "${LIB_VERSION_MAJOR}.${LIB_VERSION_MINOR}.${LIB_VERSION_PATCH}")
SET(version ${VERSION})

ADD_COMPILE_OPTIONS(
	-g
	-fPIC -fdata-sections -ffunction-sections -Wl,--gc-sections
	# Increased reliability of backtraces
	-fasynchronous-unwind-tables
	# Turn on all warnings and treat all warnings as errors.
	-Werror -Wall -Wextra
	# Additiional warnings
	-Wunreachable-code -Wpointer-arith -Waggregate-return
	-Wcast-align -Wformat=2
	# Not warnings
	-Wno-missing-field-initializers
	-Wno-missing-braces
	-Wno-unused-parameter)

# Common link options
LINK_LIBRARIES(-Wl,--no-as-needed)

# RPATH is useful only for testing without installation. Please use the
# '-DNO-RPATH' option for debian packaging.
IF(NOT NO-RPATH)
LINK_LIBRARIES(-Wl,--rpath=${CMAKE_BINARY_DIR})
ENDIF()

ADD_LIBRARY(stackmenu SHARED src/stackmenu.c)
SET_TARGET_PROPERTIES(stackmenu PROPERTIES
	VERSION ${VERSION} SOVERSION ${LIB_VERSION_MAJOR}
	OUTPUT_NAME stackmenu)
INSTALL(TARGETS stackmenu LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

CONFIGURE_FILE(stackmenu.pc.in ${CMAKE_BINARY_DIR}/stackmenu.pc @ONLY)
INSTALL(FILES ${CMAKE_BINARY_DIR}/stackmenu.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig/)

pkg_check_modules(GLIB2 glib-2.0)
IF(${GLIB2_FOUND})
	ADD_LIBRARY(stackmenu-glib SHARED src/stackmenu.c)
	FOREACH(flag ${GLIB2_CFLAGS})
		TARGET_COMPILE_OPTIONS(stackmenu-glib PRIVATE "${flag}")
	ENDFOREACH(flag)
	TARGET_COMPILE_DEFINITIONS(stackmenu-glib PRIVATE "-DBACKEND_GLIB")
	TARGET_LINK_LIBRARIES(stackmenu-glib ${GLIB2_LDFLAGS})
	SET_TARGET_PROPERTIES(stackmenu-glib PROPERTIES
		VERSION ${VERSION} SOVERSION ${LIB_VERSION_MAJOR}
		OUTPUT_NAME stackmenu-glib)
	INSTALL(TARGETS stackmenu-glib LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

	CONFIGURE_FILE(stackmenu-glib.pc.in ${CMAKE_BINARY_DIR}/stackmenu-glib.pc @ONLY)
	INSTALL(FILES ${CMAKE_BINARY_DIR}/stackmenu-glib.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig/)
ENDIF(${GLIB2_FOUND})

INSTALL(FILES src/stackmenu.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/stackmenu)

ADD_SUBDIRECTORY(examples)
