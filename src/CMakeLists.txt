cmake_minimum_required(VERSION 3.27)

include(FetchContent)

FetchContent_Declare(
	ryml
	EXCLUDE_FROM_ALL
	GIT_REPOSITORY https://github.com/biojppm/rapidyaml.git
	GIT_TAG master
)

FetchContent_MakeAvailable(ryml)

include(${CMAKE_CURRENT_SOURCE_DIR}/ParseTES.cmake)

# MO2 library dependencies — transitive deps (Boost, ZLIB, lz4, Qt Core/Widgets)
# are resolved automatically via the vcpkg toolchain's CMAKE_PREFIX_PATH.
set(MO2_CMAKE_DEPRECATED_UIBASE_INCLUDE ON)
find_package(mo2-uibase        CONFIG REQUIRED)
find_package(mo2-bsatk         CONFIG REQUIRED)
find_package(mo2-lootcli-header CONFIG REQUIRED)

# fmt must be linked directly: uibase's log.h instantiates fmt templates in
# our translation units, producing unresolved fmt::vformat at link time.
find_package(fmt CONFIG REQUIRED)

# Qt modules not re-exported by mo2::uibase.
# NOTE: Qt6::Network is intentionally excluded — linking it initialises
# WinHTTP inside our DLL and interferes with MO2's NXM download reception.
# Update checking is done via a browser link instead of HTTP requests.
find_package(Qt6 COMPONENTS WebEngineWidgets Svg REQUIRED)

add_library(bsplugins SHARED)

target_sources(bsplugins PRIVATE ${TES_INCLUDE_FILES})
target_include_directories(bsplugins
	PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}
	PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/include
)

target_link_libraries(bsplugins PRIVATE
	ryml
	mo2::uibase
	mo2::bsatk
	mo2::lootcli-header
	fmt::fmt
	Qt6::WebEngineWidgets
	Qt6::Svg
)

mo2_configure_plugin(bsplugins BIGOBJ ON WARNINGS 4)

if(MSVC)
	target_compile_options(bsplugins PRIVATE
		/WX     # warnings as errors
		/wd4201 # nonstandard extension: nameless struct/union
		/wd4458 # declaration hides class member
		/wd4996 # deprecated API
	)
endif()

mo2_install_plugin(bsplugins)
