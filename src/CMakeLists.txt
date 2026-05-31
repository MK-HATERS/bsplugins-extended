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

# Generated TES form parser .inl — added before mo2_configure_sources globbing runs
target_sources(bsplugins PRIVATE ${TES_INCLUDE_FILES})

# The generated include dir (FormParser.*.inl output) must be on the path
target_include_directories(bsplugins
	PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}
	PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/include
)

target_link_libraries(bsplugins PRIVATE ryml)

# --- Dependencies ---------------------------------------------------------
# uibase: enable deprecated include so game_features/ headers are reachable
set(MO2_CMAKE_DEPRECATED_UIBASE_INCLUDE ON)
find_package(mo2-uibase CONFIG REQUIRED)
target_link_libraries(bsplugins PRIVATE mo2::uibase)

find_package(mo2-bsatk CONFIG REQUIRED)
target_link_libraries(bsplugins PRIVATE mo2::bsatk)

find_package(mo2-lootcli-header CONFIG REQUIRED)
target_link_libraries(bsplugins PRIVATE mo2::lootcli-header)

# Qt: must be found before mo2_configure_plugin so AUTOMOC picks up all modules
find_package(Qt6 COMPONENTS Core Widgets WebEngineWidgets REQUIRED)
target_link_libraries(bsplugins PRIVATE Qt6::Core Qt6::Widgets Qt6::WebEngineWidgets)

find_package(Boost CONFIG REQUIRED COMPONENTS thread container)
target_link_libraries(bsplugins PRIVATE Boost::headers Boost::thread Boost::container)

find_package(fmt CONFIG REQUIRED)
target_link_libraries(bsplugins PRIVATE fmt::fmt)

find_package(ZLIB REQUIRED)
target_link_libraries(bsplugins PRIVATE ZLIB::ZLIB)

# --- MO2 plugin configuration ---------------------------------------------
# mo2_configure_plugin handles: source glob, C++23, /MP, /permissive-, LTCG,
# AUTOMOC/AUTOUIC/AUTORCC, NOMINMAX, QT_MESSAGELOGCONTEXT, GITID, translations.
# BIGOBJ ON  → /bigobj (large object files from template-heavy TES parsing)
# WARNINGS 4 → /W4 + /external:anglebrackets /external:W1 for system headers
mo2_configure_plugin(bsplugins
	BIGOBJ   ON
	WARNINGS 4
)

# Warnings-as-errors and specific suppressions not covered by mo2_configure_warnings
if(MSVC)
	target_compile_options(bsplugins PRIVATE
		/WX     # treat all warnings as errors
		/wd4201 # nonstandard extension: nameless struct/union
		/wd4458 # declaration of 'identifier' hides class member
		/wd4996 # deprecated API (strerror, Qt invalidateRowsFilter)
	)
endif()

mo2_install_plugin(bsplugins)
