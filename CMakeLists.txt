project(wheelie)

cmake_minimum_required(VERSION 2.6)

if(BUILD_32BIT)
set (CMAKE_C_FLAGS "-m32")
set (CMAKE_LD_FLAGS "-m32")
endif(BUILD_32BIT)

SET(EXECUTABLE_OUTPUT_PATH bin)
SET(LIBRARY_OUTPUT_PATH bin/lib)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
SET(CMAKE_BUILD_TYPE Debug)

SET(SHARED_SOURCES include/payloads.h include/wheelie.h)
SET(WHEELIE_SOURCES wheelie.c ${SHARED_SOURCES})

IF(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
Find_Package(libusb-1.0	REQUIRED)
# ${LIBUSB_1_INCLUDE_DIRS} is set manually in CMake on windows
# so we still need the include dir!
INCLUDE_DIRECTORIES(${LIBUSB_1_INCLUDE_DIRS})

SET(LIBNVUSB_SOURCES usb/linux/libnvusb.c include/linux/libnvusb.h)
SET(USBLIBS ${LIBUSB_1_LIBRARIES})
INCLUDE_DIRECTORIES(include/linux)

ENDIF(${CMAKE_SYSTEM_NAME} MATCHES "Linux")

IF(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
SET(LIBNVUSB_SOURCES usb/osx/libnvusb.c include/osx/libnvusb.h)
INCLUDE_DIRECTORIES(include/osx)
find_library(IOKIT NAMES IOKit)
find_library(COREFOUNDATION NAMES CoreFoundation)
SET(USBLIBS ${IOKIT} ${COREFOUNDATION})
ENDIF(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")

IF(WIN32)
SET(LIBNVUSB_SOURCES usb/win32/libnvusb.c include/win32/libnvusb.h)
#FIND_PATH(WINUSB_INCLUDE ddk/winusb.h HINTS C:/WinDDK/7600.16385.1/inc)
INCLUDE_DIRECTORIES( include/win32/winusb )
find_library(WINUSB_LIB winusb HINTS C:/WinDDK/7600.16385.1/lib/win7/amd64)
find_library(SETUPAPI_LIB setupapi HINTS C:/WinDDK/7600.16385.1/lib/win7/amd64)
SET(USBLIBS ${WINUSB_LIB} ${SETUPAPI_LIB})
ENDIF(WIN32)


SET(LIBNV3P_SOURCES libnv3p/libnv3p.c include/libnv3p.h)
SET(LIBNVFBLOB_SOURCES libnvfblob/libnvfblob.c include/libnvfblob.h)

INCLUDE_DIRECTORIES(include)

IF(WIN32)
INCLUDE_DIRECTORIES(include/win32)
SET(WHEELIE_SOURCES ${WHEELIE_SOURCES} include/win32/getopt.h os/win32/getopt.c os/win32/getopt_long.c)
ADD_DEFINITIONS(/D _CRT_SECURE_NO_WARNINGS)
ENDIF(WIN32)

SET(WHEELIE_VERSION 0.2)

# Should get this from USB library subproject?
SET(USB_VERSION 0.1) 
SET(USB_ABI 0.1)

# Should come from nv3p subproject
SET(NV3P_VERSION 0.1)
SET(NV3P_ABI 0.1)

# Do debug prints
OPTION(DEBUGOUTPUT "Print extra debug messages" OFF)
OPTION(STATIC_COMPILE "Compile wheelie statically" OFF)
OPTION(BUILD_32BIT "Build 32-bit version" OFF)

ADD_LIBRARY(nvusb ${LIBNVUSB_SOURCES})

ADD_LIBRARY(nv3p ${LIBNV3P_SOURCES})

ADD_LIBRARY(nvfblob ${LIBNVFBLOB_SOURCES})
TARGET_LINK_LIBRARIES(nv3p nvusb nvfblob)

ADD_EXECUTABLE(wheelie ${WHEELIE_SOURCES})



if(STATIC_COMPILE)

message(STATUS "Statically Linked")

find_library(LIBUSB_1_STATIC_LIBRARIES NAMES libusb-1.0.a PATHS /usr/lib /usr/local/lib /opt/local/lib /sw/lib)
ADD_LIBRARY(libusb STATIC IMPORTED)
set_property(TARGET libusb PROPERTY IMPORTED_LOCATION  ${LIBUSB_1_STATIC_LIBRARIES})

find_library(LIBPTHREAD_LIBRARY NAMES libpthread.a PATHS /usr/lib /usr/local/lib /opt/local/lib /sw/lib)
ADD_LIBRARY(libpthread STATIC IMPORTED)
set_property(TARGET libpthread PROPERTY IMPORTED_LOCATION  ${LIBPTHREAD_LIBRARY})

TARGET_LINK_LIBRARIES(nvusb libusb)
TARGET_LINK_LIBRARIES(wheelie nv3p nvusb libusb libpthread -static -lrt)

else(STATIC_COMPILE)

message(STATUS "Dynamically Linked")

TARGET_LINK_LIBRARIES(nvusb ${USBLIBS})
TARGET_LINK_LIBRARIES(wheelie nv3p nvusb nvfblob ${USBLIBS})

endif(STATIC_COMPILE)



configure_file(${CMAKE_SOURCE_DIR}/include/config.h.in ${CMAKE_SOURCE_DIR}/include/config.h @ONLY)
