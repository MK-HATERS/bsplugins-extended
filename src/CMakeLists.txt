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

# Pre-find packages that mo2-bsatk-config.cmake needs via find_dependency.
# Pattern follows modorganizer-preview_bsa — find_package here so cmake can
# satisfy the transitive dependency lookups without extra vcpkg hints.
find_package(mo2-cmake CONFIG REQUIRED)
find_package(ZLIB REQUIRED)
find_package(lz4 CONFIG REQUIRED)
find_package(Boost CONFIG REQUIRED COMPONENTS thread interprocess)
find_package(mo2-dds-header CONFIG REQUIRED)

# fmt must be linked explicitly: uibase's log.h instantiates fmt templates
# inside our translation units, producing unresolved fmt::vformat symbols
# at link time even though uibase itself links fmt privately.
find_package(fmt CONFIG REQUIRED)

# MO2 library dependencies
set(MO2_CMAKE_DEPRECATED_UIBASE_INCLUDE ON)
find_package(mo2-uibase CONFIG REQUIRED)
find_package(mo2-bsatk CONFIG REQUIRED)
find_package(mo2-lootcli-header CONFIG REQUIRED)

# Qt modules that mo2::uibase does not re-export as INTERFACE
find_package(Qt6 COMPONENTS WebEngineWidgets Svg REQUIRED)

add_library(bsplugins SHARED)

# Generated TES form parser .inl — added before mo2_configure_sources globs
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

# mo2_configure_plugin handles: source glob, C++23, /MP, /permissive-, LTCG,
# AUTOMOC/AUTOUIC/AUTORCC, NOMINMAX, QT_MESSAGELOGCONTEXT, GITID, translations
mo2_configure_plugin(bsplugins
	BIGOBJ   ON
	WARNINGS 4
)

if(MSVC)
	target_compile_options(bsplugins PRIVATE
		/WX     # treat all warnings as errors
		/wd4201 # nonstandard extension: nameless struct/union
		/wd4458 # declaration of 'identifier' hides class member
		/wd4996 # deprecated API (strerror, Qt invalidateRowsFilter)
	)
endif()

mo2_install_plugin(bsplugins)
