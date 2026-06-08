# ── Laptop native build ───────────────────────────────────────────────────────
set(CAPI_PREFIX "/usr/local" CACHE PATH "CommonAPI install prefix")

find_library(LIB_VSOMEIP        vsomeip3           PATHS ${CAPI_PREFIX}/lib NO_DEFAULT_PATH)
find_library(LIB_CAPI_CORE      CommonAPI          PATHS ${CAPI_PREFIX}/lib NO_DEFAULT_PATH)
find_library(LIB_CAPI_SOMEIP    CommonAPI-SomeIP   PATHS ${CAPI_PREFIX}/lib NO_DEFAULT_PATH)

if(NOT LIB_VSOMEIP OR NOT LIB_CAPI_CORE OR NOT LIB_CAPI_SOMEIP)
    message(FATAL_ERROR
        "Could not find CommonAPI/vsomeip libs under ${CAPI_PREFIX}/lib.\n"
        "  vsomeip3        : ${LIB_VSOMEIP}\n"
        "  CommonAPI       : ${LIB_CAPI_CORE}\n"
        "  CommonAPI-SomeIP: ${LIB_CAPI_SOMEIP}")
endif()

message(STATUS "vsomeip3        : ${LIB_VSOMEIP}")
message(STATUS "CommonAPI       : ${LIB_CAPI_CORE}")
message(STATUS "CommonAPI-SomeIP: ${LIB_CAPI_SOMEIP}")

find_package(Boost 1.65 REQUIRED COMPONENTS system thread)

# All CommonAPI headers (core + SomeIP) live under CommonAPI-3.2/
# vsomeip headers live directly under /usr/local/include/
set(CAPI_INCLUDE_DIR  ${CAPI_PREFIX}/include/CommonAPI-3.2)
set(VSOMEIP_INCLUDE_DIR ${CAPI_PREFIX}/include)

message(STATUS "CommonAPI headers: ${CAPI_INCLUDE_DIR}")
message(STATUS "vsomeip headers  : ${VSOMEIP_INCLUDE_DIR}")

# ── Sources ───────────────────────────────────────────────────────────────────
set(SRCS_GEN
    ${CMAKE_SOURCE_DIR}/src-gen/v1/com/myapp/ota/OtaUpdateSomeIPDeployment.cpp
    ${CMAKE_SOURCE_DIR}/src-gen/v1/com/myapp/ota/OtaUpdateSomeIPProxy.cpp
    ${CMAKE_SOURCE_DIR}/src-gen/v1/com/myapp/ota/OtaUpdateSomeIPStubAdapter.cpp
)

add_executable(ota_client
    ${CMAKE_SOURCE_DIR}/laptop/main_laptop.cpp
    ${SRCS_GEN}
)

target_include_directories(ota_client PRIVATE
    ${CMAKE_SOURCE_DIR}/src-gen       # generated headers (OtaUpdate*.hpp)
    ${CAPI_INCLUDE_DIR}               # CommonAPI/CommonAPI.hpp + CommonAPI/SomeIP/*
    ${VSOMEIP_INCLUDE_DIR}            # vsomeip/vsomeip.hpp
    ${Boost_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
)

target_link_libraries(ota_client PRIVATE
    ${LIB_CAPI_SOMEIP}
    ${LIB_CAPI_CORE}
    ${LIB_VSOMEIP}
    ${Boost_LIBRARIES}
    OpenSSL::Crypto
    pthread
)

configure_file(
    ${CMAKE_SOURCE_DIR}/laptop/vsomeip-ota-laptop.json
    ${CMAKE_BINARY_DIR}/vsomeip-ota-laptop.json
    COPYONLY
)

install(TARGETS ota_client DESTINATION bin)
install(FILES ${CMAKE_SOURCE_DIR}/laptop/vsomeip-ota-laptop.json
        DESTINATION etc/vsomeip)

message(STATUS "Laptop target: ota_client")
