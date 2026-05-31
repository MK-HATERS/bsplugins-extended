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
# uibase: enable deprecated include so game_features/ headers are reachable.
# Qt6::Core and Qt6::Widgets come transitively via mo2::uibase (it is itself
# a Qt library and exports them as INTERFACE dependencies).
# fmt and zlib are also transitive through uibase/bsatk.
set(MO2_CMAKE_DEPRECATED_UIBASE_INCLUDE ON)
find_package(mo2-uibase CONFIG REQUIRED)
target_link_libraries(bsplugins PRIVATE mo2::uibase)

find_package(mo2-bsatk CONFIG REQUIRED)
target_link_libraries(bsplugins PRIVATE mo2::bsatk)

find_package(mo2-lootcli-header CONFIG REQUIRED)
target_link_libraries(bsplugins PRIVATE mo2::lootcli-header)

# WebEngineWidgets is only used by this plugin — not provided transitively
find_package(Qt6 COMPONENTS WebEngineWidgets REQUIRED)
target_link_libraries(bsplugins PRIVATE Qt6::WebEngineWidgets)

# Boost::thread has compiled parts we use directly (boost::thread::future).
# The header-only Boost components (signals2, container, range) come
# transitively once mo2-bsatk-config calls find_dependency(Boost ...).
find_package(Boost CONFIG REQUIRED COMPONENTS thread)
target_link_libraries(bsplugins PRIVATE Boost::thread)

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
