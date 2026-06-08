# ── RPi cross-compile build (Yocto — no pkg-config available) ────────────────

find_package(Boost 1.65 REQUIRED COMPONENTS system thread)

# ── vsomeip3 ──────────────────────────────────────────────────────────────────
find_library(LIB_VSOMEIP NAMES vsomeip3)
if(NOT LIB_VSOMEIP)
    message(FATAL_ERROR "Cannot find libvsomeip3 in sysroot")
endif()
message(STATUS "vsomeip3: ${LIB_VSOMEIP}")

find_path(VSOMEIP_INCLUDE_DIR NAMES vsomeip/vsomeip.hpp)
if(NOT VSOMEIP_INCLUDE_DIR)
    message(FATAL_ERROR "Cannot find vsomeip/vsomeip.hpp in sysroot")
endif()
message(STATUS "vsomeip headers: ${VSOMEIP_INCLUDE_DIR}")

# ── capicxx-core-runtime ──────────────────────────────────────────────────────
find_library(LIB_CAPI_CORE NAMES CommonAPI)
if(NOT LIB_CAPI_CORE)
    message(FATAL_ERROR "Cannot find libCommonAPI in sysroot")
endif()
message(STATUS "CommonAPI: ${LIB_CAPI_CORE}")

find_path(CAPI_CORE_INCLUDE_DIR
    NAMES CommonAPI/CommonAPI.hpp
    PATH_SUFFIXES CommonAPI-3.2
)
if(NOT CAPI_CORE_INCLUDE_DIR)
    message(FATAL_ERROR "Cannot find CommonAPI/CommonAPI.hpp in sysroot")
endif()
message(STATUS "CommonAPI headers: ${CAPI_CORE_INCLUDE_DIR}")

# ── capicxx-someip-runtime ────────────────────────────────────────────────────
find_library(LIB_CAPI_SOMEIP NAMES CommonAPI-SomeIP)
if(NOT LIB_CAPI_SOMEIP)
    message(FATAL_ERROR "Cannot find libCommonAPI-SomeIP in sysroot")
endif()
message(STATUS "CommonAPI-SomeIP: ${LIB_CAPI_SOMEIP}")

# SomeIP headers install alongside core headers under CommonAPI-3.2/
find_path(CAPI_SOMEIP_INCLUDE_DIR
    NAMES CommonAPI/SomeIP/Deployment.hpp
    PATH_SUFFIXES CommonAPI-3.2
)
if(NOT CAPI_SOMEIP_INCLUDE_DIR)
    message(FATAL_ERROR "Cannot find CommonAPI/SomeIP/Deployment.hpp in sysroot")
endif()
message(STATUS "CommonAPI-SomeIP headers: ${CAPI_SOMEIP_INCLUDE_DIR}")

# ── Sources ───────────────────────────────────────────────────────────────────
set(SRCS_GEN
    ${CMAKE_SOURCE_DIR}/src-gen/v1/com/myapp/ota/OtaUpdateSomeIPDeployment.cpp
    ${CMAKE_SOURCE_DIR}/src-gen/v1/com/myapp/ota/OtaUpdateSomeIPProxy.cpp
    ${CMAKE_SOURCE_DIR}/src-gen/v1/com/myapp/ota/OtaUpdateSomeIPStubAdapter.cpp
)

add_executable(ota_update_service
    ${CMAKE_SOURCE_DIR}/rpi/main_rpi.cpp
    ${CMAKE_SOURCE_DIR}/rpi/OtaUpdateServiceImpl.cpp
    ${SRCS_GEN}
)

target_include_directories(ota_update_service PRIVATE
    ${CMAKE_SOURCE_DIR}/src-gen
    ${CMAKE_SOURCE_DIR}/rpi
    ${VSOMEIP_INCLUDE_DIR}
    ${CAPI_CORE_INCLUDE_DIR}
    ${CAPI_SOMEIP_INCLUDE_DIR}
    ${Boost_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
)

target_link_libraries(ota_update_service PRIVATE
    ${LIB_CAPI_SOMEIP}
    ${LIB_CAPI_CORE}
    ${LIB_VSOMEIP}
    ${Boost_LIBRARIES}
    OpenSSL::Crypto
    pthread
)

# ── Install ───────────────────────────────────────────────────────────────────
install(TARGETS ota_update_service
        DESTINATION ${CMAKE_INSTALL_BINDIR})

install(FILES ${CMAKE_SOURCE_DIR}/rpi/vsomeip-ota.json
        DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/vsomeip)

message(STATUS "RPi target: ota_update_service")
