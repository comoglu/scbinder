cmake_minimum_required(VERSION 3.16)
project(scbinder C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

set(SEISCOMP_ROOT /opt/seiscomp CACHE PATH "SeisComP installation prefix")

file(GLOB CORE_SOURCES ${CMAKE_SOURCE_DIR}/core/*.c)
file(GLOB EWLIB_SOURCES ${CMAKE_SOURCE_DIR}/ewlib/*.c)

add_executable(scbinder
	${CORE_SOURCES}
	${EWLIB_SOURCES}
	app/ewglue.cpp
	app/ttime_sc.cpp
	app/app.cpp
	app/main.cpp
)

# The travel-time dispatcher (app/ttime_sc.cpp) exports tlay's public
# interface and routes to either the verbatim tlay model or a SeisComP
# travel-time table (velmod command). The verbatim tlay.c keeps compiling
# unchanged; only its five public symbols are renamed to tlay_*.
set_source_files_properties(${CMAKE_SOURCE_DIR}/ewlib/tlay.c PROPERTIES
	COMPILE_DEFINITIONS "t_com=tlay_t_com;t_phase=tlay_t_phase;t_lay=tlay_t_lay;t_region=tlay_t_region;getPoverS=tlay_getPoverS")

target_include_directories(scbinder PRIVATE
	${CMAKE_SOURCE_DIR}/core
	${CMAKE_SOURCE_DIR}/ewlib
	${SEISCOMP_ROOT}/include
)

target_compile_definitions(scbinder PRIVATE _LINUX)

# The Earthworm sources are decades-old plain C; keep warnings sane.
set_source_files_properties(${CORE_SOURCES} ${EWLIB_SOURCES}
	PROPERTIES COMPILE_OPTIONS "-Wno-unused-but-set-variable;-Wno-unused-variable")
# tlay.h declares static char* string tables, harmless in C++.
set_source_files_properties(app/app.cpp
	PROPERTIES COMPILE_OPTIONS "-Wno-write-strings")

target_link_directories(scbinder PRIVATE ${SEISCOMP_ROOT}/lib)
find_package(Boost REQUIRED COMPONENTS program_options)

target_link_libraries(scbinder
	seiscomp_client
	seiscomp_core
	Boost::program_options
	m
)
set_target_properties(scbinder PROPERTIES
	INSTALL_RPATH ${SEISCOMP_ROOT}/lib
	BUILD_RPATH ${SEISCOMP_ROOT}/lib
)

install(TARGETS scbinder RUNTIME DESTINATION ${SEISCOMP_ROOT}/bin)
