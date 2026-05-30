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

add_library(bsplugins SHARED)

set_property(TARGET bsplugins PROPERTY CXX_STANDARD 20)
target_sources(bsplugins PRIVATE ${TES_INCLUDE_FILES})
target_include_directories(
	bsplugins
	PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
	PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/include
)
target_link_libraries(bsplugins PRIVATE ryml)

if(MSVC)
	target_compile_options(
		bsplugins
		PRIVATE
			/bigobj # increase number of sections in .obj file
			/W4     # level 4 warnings
			/WX     # treat all compiler warnings as errors
			/wd4201 # nonstandard extension used: nameless struct/union
			/wd4458 # declaration of 'identifier' hides class member
			/wd4996 # deprecated API (strerror, Qt invalidateRowsFilter)
	)
endif()

# Qt - must be found before mo2_configure_plugin so AUTOMOC knows the Qt version
find_package(Qt6 COMPONENTS Core Widgets WebEngineWidgets REQUIRED)
target_link_libraries(bsplugins PRIVATE Qt6::Core Qt6::Widgets Qt6::WebEngineWidgets)

# uibase - enable deprecated include so game_features/ headers are on the include path
set(MO2_CMAKE_DEPRECATED_UIBASE_INCLUDE ON)
find_package(mo2-uibase CONFIG REQUIRED)
target_link_libraries(bsplugins PRIVATE mo2::uibase)

# bsatk and lootcli
find_package(mo2-bsatk CONFIG REQUIRED)
target_link_libraries(bsplugins PRIVATE mo2::bsatk)

find_package(mo2-lootcli-header CONFIG REQUIRED)
target_link_libraries(bsplugins PRIVATE mo2::lootcli-header)

# Boost - container has compiled parts; signals2 is header-only via Boost::headers
find_package(Boost CONFIG REQUIRED COMPONENTS thread container)
target_link_libraries(bsplugins PRIVATE Boost::headers Boost::thread Boost::container)

# fmt and zlib
find_package(fmt CONFIG REQUIRED)
target_link_libraries(bsplugins PRIVATE fmt::fmt)

find_package(ZLIB REQUIRED)
target_link_libraries(bsplugins PRIVATE ZLIB::ZLIB)

mo2_configure_plugin(bsplugins WARNINGS OFF)
mo2_install_plugin(bsplugins)
