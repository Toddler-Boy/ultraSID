cmake_minimum_required (VERSION 4.3.0 FATAL_ERROR)

set (CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set (CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set (CMAKE_SUPPRESS_REGENERATION ON)
set (CMAKE_XCODE_GENERATE_SCHEME ON)
set (CMAKE_XCODE_ATTRIBUTE_SHOW_ENVIRONMENT_BUILD_SETTINGS NO)
set (CMAKE_XCODE_ATTRIBUTE_COMPILER_INDEX_STORE_ENABLE NO)
set (CMAKE_CONFIGURATION_TYPES Debug Release Development)
set (CMAKE_OSX_DEPLOYMENT_TARGET 11.0)

if (WIN32)
	set (CMAKE_C_FLAGS_RELEASE "/O2 /Ob2 /Oi /Ot /Oy /GF /GS- /Gy /fp:fast /DNDEBUG")
	set (CMAKE_CXX_FLAGS_RELEASE ${CMAKE_C_FLAGS_RELEASE})
	set (CMAKE_SHARED_LINKER_FLAGS_RELEASE "/INCREMENTAL:NO /OPT:REF /OPT:ICF /nodefaultlib:msvcrt")
	set (CMAKE_EXE_LINKER_FLAGS_RELEASE ${CMAKE_SHARED_LINKER_FLAGS_RELEASE})
endif()

file (READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" VERSION)
string (STRIP "${VERSION}" VERSION)

project (
	ultraSID
	VERSION ${VERSION}
	LANGUAGES CXX C
	HOMEPAGE_URL "https://ultrasid.com/")

# The major version IS the HVSC release this build targets, and the shipped
# tune database must be built for the same release (header byte 4)
file (READ "${CMAKE_CURRENT_SOURCE_DIR}/Data/ultraSID.db" db_header LIMIT 5 HEX)
string (SUBSTRING "${db_header}" 8 2 db_version_hex)
math (EXPR db_version "0x${db_version_hex}")
if (NOT db_version EQUAL ultraSID_VERSION_MAJOR)
	message (FATAL_ERROR "Data/ultraSID.db is for HVSC ${db_version}, VERSION says ${ultraSID_VERSION_MAJOR}")
endif ()
set_property (DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
			  "${CMAKE_CURRENT_SOURCE_DIR}/VERSION"
			  "${CMAKE_CURRENT_SOURCE_DIR}/Data/ultraSID.db")

Include (FetchContent)

# Development = Release plus dev-only features (naked Data folder, inspector);
# custom configs start with empty flags, so inherit the Release set wholesale
set (CMAKE_C_FLAGS_DEVELOPMENT "${CMAKE_C_FLAGS_RELEASE}")
set (CMAKE_CXX_FLAGS_DEVELOPMENT "${CMAKE_CXX_FLAGS_RELEASE}")
set (CMAKE_EXE_LINKER_FLAGS_DEVELOPMENT "${CMAKE_EXE_LINKER_FLAGS_RELEASE}")
set (CMAKE_SHARED_LINKER_FLAGS_DEVELOPMENT "${CMAKE_SHARED_LINKER_FLAGS_RELEASE}")

if (WIN32)
	# Plus a pdb for profilers; the inherited explicit /OPT pair keeps /DEBUG
	# from disabling those optimizations
	set (CMAKE_C_FLAGS_DEVELOPMENT "${CMAKE_C_FLAGS_DEVELOPMENT} /Zi")
	set (CMAKE_CXX_FLAGS_DEVELOPMENT "${CMAKE_CXX_FLAGS_DEVELOPMENT} /Zi")
	set (CMAKE_EXE_LINKER_FLAGS_DEVELOPMENT "${CMAKE_EXE_LINKER_FLAGS_DEVELOPMENT} /DEBUG")
	set (CMAKE_SHARED_LINKER_FLAGS_DEVELOPMENT "${CMAKE_SHARED_LINKER_FLAGS_DEVELOPMENT} /DEBUG")
endif()

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ultraSID)

set (CMAKE_EXPORT_COMPILE_COMMANDS ON)
set (CMAKE_CXX_STANDARD 23)
set (CMAKE_CXX_STANDARD_REQUIRED ON)
set (CMAKE_CXX_EXTENSIONS OFF)
set (CMAKE_C_STANDARD 17)
set (CMAKE_C_STANDARD_REQUIRED ON)
set (CMAKE_C_EXTENSIONS OFF)
set (CMAKE_OBJCXX_STANDARD 23)
set (CMAKE_OBJCXX_STANDARD_REQUIRED ON)
set (CMAKE_CXX_VISIBILITY_PRESET hidden)
set (CMAKE_VISIBILITY_INLINES_HIDDEN ON)

set_property (GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER utility)

# Turn all install() calls into no-ops (JUCE and the fetched dependencies declare
# install rules unconditionally); without any registered rules, CMake generates no
# INSTALL target. Remove this macro if install support is ever needed.
macro (install)
endmacro ()

# JUCE
set (JUCE_ENABLE_MODULE_SOURCE_GROUPS ON)

# Soft pin on develop: the fixes land there months before a tagged release
FetchContent_Declare (JUCE
					GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
					GIT_TAG origin/develop
					GIT_SHALLOW TRUE
					SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/JUCE)
FetchContent_MakeAvailable (JUCE)

# Binary Data (icons)
if(WIN32)
	set (icon_path_big "${CMAKE_CURRENT_LIST_DIR}/icons/windows_big.png")
	set (icon_path_small "${CMAKE_CURRENT_LIST_DIR}/icons/windows_small.png")
elseif(APPLE)
	set (icon_path_big "${CMAKE_CURRENT_LIST_DIR}/icons/macos15_big.png")
	set (icon_path_small "${CMAKE_CURRENT_LIST_DIR}/icons/macos15_small.png")
endif()

# Gin modules
foreach(module_name IN ITEMS gin gin_dsp gin_simd gin_graphics gin_gui)
	juce_add_module ( "${CMAKE_CURRENT_LIST_DIR}/modules/Gin/modules/${module_name}" )
endforeach()

# Lime modules
foreach ( module_name IN ITEMS lime_Logger lime_ShaderToyComponent lime_YamlConfig lime_Webcam lime_CRTEmulation )
	juce_add_module ( "${CMAKE_CURRENT_LIST_DIR}/modules/lime/modules/${module_name}" )
endforeach()

# Melatonin Inspector: dev tooling, Development config only, and Windows only,
# the sole dev platform (Xcode cannot exclude an interface module per config)
if (WIN32)
	FetchContent_Declare (melatonin_inspector
						GIT_REPOSITORY https://github.com/sudara/melatonin_inspector.git
						GIT_TAG origin/main
						GIT_SHALLOW TRUE
						SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/melatonin_inspector)
	FetchContent_MakeAvailable (melatonin_inspector)
endif ()

# Melatonin Blur
FetchContent_Declare (melatonin_blur
					GIT_REPOSITORY https://github.com/sudara/melatonin_blur.git
					GIT_TAG origin/main
					GIT_SHALLOW TRUE
					SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/melatonin_blur)
FetchContent_MakeAvailable (melatonin_blur)

# fmt replaces std::format (features land overnight instead of every three
# years); hard pin on a release, bumped deliberately. Plain includes only, the
# C++20 module flavor stays off; fmt-c has no off-switch, so it is excluded
# from builds below instead
set (FMT_MODULE OFF)
FetchContent_Declare (fmt
					GIT_REPOSITORY https://github.com/fmtlib/fmt.git
					GIT_TAG 12.2.0
					GIT_SHALLOW TRUE
					SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/fmt)
FetchContent_MakeAvailable (fmt)
set_target_properties (fmt-c PROPERTIES EXCLUDE_FROM_ALL TRUE)

# boost::regex, standalone and header-only (BOOST_REGEX_STANDALONE): no other
# boost libs, nothing to build. SOURCE_SUBDIR points nowhere so its own CMake
# never runs; hard pin on a release, bumped deliberately
FetchContent_Declare (boost_regex
					GIT_REPOSITORY https://github.com/boostorg/regex.git
					GIT_TAG boost-1.92.0
					GIT_SHALLOW TRUE
					SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/boost_regex
					SOURCE_SUBDIR nonexistent)
FetchContent_MakeAvailable (boost_regex)

# --- xz (liblzma) options ---
# xz >= 5.6 prefixes all its CMake options with XZ_; unprefixed names are ignored.
# Decompression only: no encoders. Decoders lzma1+lzma2 for 7z, plus delta+x86:
# libarchive's 7z reader requests those from lzma_raw_decoder for BCJ-filtered
# LZMA2 streams (other BCJ variants, BCJ2 and PPMd are handled by libarchive itself).
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(XZ_ENCODERS "" CACHE STRING "" FORCE)
set(XZ_DECODERS "lzma1;lzma2;delta;x86" CACHE STRING "" FORCE)
set(XZ_CHECKS "crc32" CACHE STRING "" FORCE)
set(XZ_THREADS no CACHE STRING "" FORCE)
set(XZ_MICROLZMA_ENCODER OFF CACHE BOOL "" FORCE)
set(XZ_MICROLZMA_DECODER OFF CACHE BOOL "" FORCE)
set(XZ_LZIP_DECODER OFF CACHE BOOL "" FORCE)
foreach(_t XZ XZDEC LZMADEC LZMAINFO SCRIPTS)
  set(XZ_TOOL_${_t} OFF CACHE BOOL "" FORCE)
endforeach()
set(XZ_DOC OFF CACHE BOOL "" FORCE)
set(XZ_NLS OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)

# --- libarchive options ---
foreach(_o OPENSSL LIBB2 LZ4 LZO ZSTD ZLIB BZip2 LIBXML2 EXPAT PCREPOSIX
          PCRE2POSIX CNG MBEDTLS NETTLE ICONV ACL XATTR
          WIN32_XMLLITE LIBGCC
          TAR CPIO CAT UNZIP TEST INSTALL WERROR)
  set(ENABLE_${_o} OFF CACHE BOOL "" FORCE)
endforeach()
set(POSIX_REGEX_LIB "NONE" CACHE STRING "" FORCE)

# Block libarchive from finding a system LibLZMA, the FetchContent one is the only one
set(CMAKE_DISABLE_FIND_PACKAGE_LibLZMA ON CACHE BOOL "" FORCE)

# Skip xz's NLS probes (XZ_NLS is forced off; silences not-found notices)
set(CMAKE_DISABLE_FIND_PACKAGE_Intl ON CACHE BOOL "" FORCE)
set(CMAKE_DISABLE_FIND_PACKAGE_Gettext ON CACHE BOOL "" FORCE)

# libarchive include()s FindPkgConfig directly, so the disable-find_package
# variable doesn't apply; quiet its not-found notice instead
set(PkgConfig_FIND_QUIETLY TRUE)

# Soft pins: maintenance branches, so a fresh configure picks up the latest
# patch release of the current minor version (bugfix/security only). Adopting
# a new minor version (e.g. v5.10, patch/3.9) is a deliberate edit here.
# Not libarchive's "release" branch, it is stale.
FetchContent_Declare(xz
					GIT_REPOSITORY https://github.com/tukaani-project/xz.git
					GIT_TAG v5.8
					GIT_SHALLOW TRUE)

FetchContent_Declare(libarchive
					GIT_REPOSITORY https://github.com/libarchive/libarchive.git
					GIT_TAG patch/3.8
					GIT_SHALLOW TRUE)

set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)

FetchContent_MakeAvailable(xz)

# Wire liblzma into libarchive's config by hand, find_package is blocked above
set(LIBLZMA_FOUND TRUE CACHE BOOL "" FORCE)
set(HAVE_LIBLZMA 1 CACHE INTERNAL "")
set(HAVE_LZMA_H 1 CACHE INTERNAL "")
set(LIBLZMA_INCLUDE_DIR "${xz_SOURCE_DIR}/src/liblzma/api" CACHE PATH "" FORCE)
set(LIBLZMA_INCLUDE_DIRS "${xz_SOURCE_DIR}/src/liblzma/api" CACHE PATH "" FORCE)
set(LIBLZMA_LIBRARIES liblzma CACHE STRING "" FORCE)
set(LIBLZMA_HAS_AUTO_DECODER TRUE CACHE BOOL "" FORCE)
set(LIBLZMA_HAS_EASY_ENCODER TRUE CACHE BOOL "" FORCE)
set(LIBLZMA_HAS_LZMA_PRESET TRUE CACHE BOOL "" FORCE)

# Linux: libarchive's probe misses glibc's id_t and would typedef a colliding one
if(UNIX AND NOT APPLE)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_GNU_SOURCE")
  set(HAVE_ID_T 1 CACHE BOOL "" FORCE)
endif()

FetchContent_MakeAvailable(libarchive)

# archive_static links the FetchContent liblzma
target_include_directories(archive_static PUBLIC ${xz_SOURCE_DIR}/src/liblzma/api)
target_link_libraries(archive_static liblzma)
if(WIN32)
  # libarchive 3.8.6+ calls BCryptGenRandom unconditionally on Windows
  # (archive_util.c temp names) but only links bcrypt when ENABLE_CNG is ON.
  target_link_libraries(archive_static bcrypt)
  # Build the dependency libs with the same toolset as the app (they would
  # otherwise fall back to the solution default, MSVC)
  set_target_properties(liblzma libgnu archive_static fmt PROPERTIES VS_PLATFORM_TOOLSET ClangCL)
  # no C++20 modules, clang-cl has no /scanDependencies. The inherited
  # /fp:fast would strip the inf/nan handling fmt's float paths rely on
  set_target_properties(fmt PROPERTIES CXX_SCAN_FOR_MODULES OFF)
  target_compile_options(fmt PRIVATE /clang:-fno-finite-math-only)
endif()
target_compile_definitions(archive_static PUBLIC HAVE_LIBLZMA=1 HAVE_LZMA_H=1 LZMA_API_STATIC)

# Silence compiler warnings inside FetchContent dependencies: their code, their
# warnings. Skips INTERFACE targets (nothing is compiled for those).
if(MSVC OR CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
  # /W0, not /w: the VS generator maps /W0 to the vcxproj WarningLevel element,
  # while /w lands in AdditionalOptions and collides with MSBuild's default
  # warning level (cl warning D9025)
  set(_dep_no_warnings /W0)
else()
  set(_dep_no_warnings -w)
endif()
# Strip warning flags the dependencies set themselves (xz adds /W4 and /wd
# suppressions directory-wide), otherwise cl warns D9025 "overriding /W4 with /w"
foreach(_d ${xz_SOURCE_DIR} ${libarchive_SOURCE_DIR})
  get_property(_dep_opts DIRECTORY ${_d} PROPERTY COMPILE_OPTIONS)
  if(_dep_opts)
    list(FILTER _dep_opts EXCLUDE REGEX "^[-/][Ww]")
    set_property(DIRECTORY ${_d} PROPERTY COMPILE_OPTIONS ${_dep_opts})
  endif()
endforeach()
foreach(_t liblzma libgnu archive_static fmt melatonin_inspector melatonin_blur)
  if(TARGET ${_t})
    get_target_property(_type ${_t} TYPE)
    if(NOT _type STREQUAL "INTERFACE_LIBRARY")
      get_target_property(_t_opts ${_t} COMPILE_OPTIONS)
      if(_t_opts)
        list(FILTER _t_opts EXCLUDE REGEX "^[-/][Ww]")
        set_target_properties(${_t} PROPERTIES COMPILE_OPTIONS "${_t_opts}")
      endif()
      target_compile_options(${_t} PRIVATE ${_dep_no_warnings})
    endif()
  endif()
endforeach()

# PGO for the emulation core (Tools/pgo: profile-list.txt scopes it, train.sh
# writes the profile, one per mangling; a present profile is always used)
option (ULTRA_PGO_GENERATE "Instrument the emulation core for a PGO training run" OFF)
set (pgo_list ${CMAKE_CURRENT_SOURCE_DIR}/Tools/pgo/profile-list.txt)
if (WIN32)
	set (pgo_profile ${CMAKE_CURRENT_SOURCE_DIR}/Tools/pgo/sidplayez-msvc.profdata)
else ()
	set (pgo_profile ${CMAKE_CURRENT_SOURCE_DIR}/Tools/pgo/sidplayez-itanium.profdata)
endif ()
set_property (DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${pgo_list} ${pgo_profile}.commit)

# An engine that moved past the commit the profile was trained on has lost
# part of it
if (EXISTS ${pgo_profile}.commit)
	file (READ ${pgo_profile}.commit pgo_trained)
	string (STRIP "${pgo_trained}" pgo_trained)
	execute_process (COMMAND git -C ${CMAKE_CURRENT_SOURCE_DIR}/Source/libSidplayEZ rev-parse HEAD
					 OUTPUT_VARIABLE pgo_engine OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
	if (pgo_engine AND NOT pgo_engine STREQUAL pgo_trained)
		message (WARNING "PGO profile trained on libSidplayEZ ${pgo_trained}, the engine is at ${pgo_engine}: run Tools/pgo/train.sh")
	endif ()
endif ()

# Own targets: clang-cl on Windows, no module scanning, finite-math-only off
# (same reasons as fmt above); DSP adds the fast-math set, PGO the profile
function (ultra_target target)
	cmake_parse_arguments (arg "DSP;PGO" "" "" ${ARGN})
	set_target_properties (${target} PROPERTIES CXX_SCAN_FOR_MODULES OFF)
	if (WIN32)
		set_target_properties (${target} PROPERTIES VS_PLATFORM_TOOLSET ClangCL)
	endif ()
	if (arg_DSP AND WIN32)
		target_compile_options (${target} PRIVATE -Wno-missing-braces $<$<CONFIG:Release,Development>:/clang:-O3> /clang:-fno-math-errno /clang:-ffast-math /clang:-fno-finite-math-only /clang:-msse4.2 /clang:-ffp-contract=fast /clang:-funsafe-math-optimizations)
	elseif (arg_DSP)
		target_compile_options (${target} PRIVATE -Wno-missing-braces -ffast-math -fno-finite-math-only -ffp-contract=fast -funsafe-math-optimizations)
	elseif (WIN32)
		target_compile_options (${target} PRIVATE -Wno-missing-braces /clang:-fno-finite-math-only)
	endif ()
	if (arg_PGO AND ULTRA_PGO_GENERATE)
		set (pgo_flags -fprofile-instr-generate -fprofile-list=${pgo_list})
		if (NOT WIN32)
			target_link_options (${target} PRIVATE -fprofile-instr-generate)
		endif ()
	elseif (arg_PGO AND EXISTS ${pgo_profile})
		set (pgo_flags -fprofile-instr-use=${pgo_profile} -fprofile-list=${pgo_list} -Wno-profile-instr-unprofiled)
	endif ()
	if (pgo_flags AND WIN32)
		list (TRANSFORM pgo_flags PREPEND /clang:)
	endif ()
	if (pgo_flags)
		target_compile_options (${target} PRIVATE ${pgo_flags})
	endif ()
endfunction ()

# Framework-free test executable, never part of the normal build
function (ultra_test target)
	cmake_parse_arguments (arg "DSP;PGO" "" "" ${ARGN})
	add_executable (${target} EXCLUDE_FROM_ALL ${arg_UNPARSED_ARGUMENTS})
	target_include_directories (${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source)
	target_compile_definitions (${target} PRIVATE _CRT_SECURE_NO_WARNINGS)
	set_target_properties (${target} PROPERTIES FOLDER Tests)
	set (opts)
	foreach (opt DSP PGO)
		if (arg_${opt})
			list (APPEND opts ${opt})
		endif ()
	endforeach ()
	ultra_target (${target} ${opts})
endfunction ()

# juce_core console app, never part of the normal build
function (ultra_console_app target folder)
	cmake_parse_arguments (arg "DSP;PGO" "" "" ${ARGN})
	juce_add_console_app (${target} PRODUCT_NAME ${target})
	target_sources (${target} PRIVATE ${arg_UNPARSED_ARGUMENTS})
	target_include_directories (${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source)
	target_link_libraries (${target} PRIVATE juce::juce_core lime_Logger juce::juce_recommended_config_flags)
	juce_generate_juce_header (${target})
	target_compile_definitions (${target} PRIVATE DONT_SET_USING_JUCE_NAMESPACE=1 _CRT_SECURE_NO_WARNINGS JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0 JUCE_MODAL_LOOPS_PERMITTED=0)
	set_target_properties (${target} PROPERTIES EXCLUDE_FROM_ALL TRUE FOLDER ${folder})
	if (TARGET ${target}_rc_lib)
		set_target_properties (${target}_rc_lib PROPERTIES FOLDER utility)
	endif ()
	set (opts)
	foreach (opt DSP PGO)
		if (arg_${opt})
			list (APPEND opts ${opt})
		endif ()
	endforeach ()
	ultra_target (${target} ${opts})
endfunction ()

juce_add_gui_app (ultraSID
				  PRODUCT_NAME ultraSID
				  CAMERA_PERMISSION_ENABLED YES
				  CAMERA_PERMISSION_TEXT "User reflection"
				  VERSION "${ultraSID_VERSION}"
  				  COMPANY_NAME "Michael Hartmann"
				  COMPANY_WEBSITE "https://ultrasid.com/"
				  ICON_BIG "${icon_path_big}"
				  ICON_SMALL "${icon_path_small}"
				  BUNDLE_ID com.toddler-boy.ultraSID)

# JUCE's version-resource command only runs when the rc is missing (its only
# declared dependency is the icon), so a VERSION bump in an existing tree
# would keep shipping the old numbers; evict a stale rc so the build remakes it
if (WIN32)
	set (version_rc "${CMAKE_BINARY_DIR}/ultraSID_artefacts/JuceLibraryCode/ultraSID_resources.rc")
	if (EXISTS "${version_rc}" AND "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" IS_NEWER_THAN "${version_rc}")
		file (REMOVE "${version_rc}")
	endif ()
endif ()

file (GLOB_RECURSE source_files CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/*.c
	${CMAKE_CURRENT_SOURCE_DIR}/Source/*.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/*.h)

# ultra-shared/Network is ultraView-only (C64u streaming); ultraSID has no consumer
list (FILTER source_files EXCLUDE REGEX "/ultra-shared/Network/")

target_sources (ultraSID PRIVATE ${source_files})
source_group (TREE ${CMAKE_CURRENT_SOURCE_DIR}/Source PREFIX Source FILES ${source_files})

# Poor-man's LTCG: clang-cl can't consume /GL, and MSBuild's ClangCL toolset links
# with link.exe, which can't consume ThinLTO bitcode. Instead, merge the emulation
# sources into one unity TU so clang can inline across the per-cycle CPU/MMU/CIA/SID
# call boundaries. GROUP mode compiles every source without a UNITY_GROUP normally,
# so the JUCE/Gin/lime module sources are unaffected. MD5.cpp stays out of the
# group: it defines dozens of unscoped macros (T1..T64) that would leak into any
# file merged after it.
set (emu_unity_sources ${source_files})
list (FILTER emu_unity_sources INCLUDE REGEX "/libSidplayEZ/.*\\.cpp$")
list (FILTER emu_unity_sources EXCLUDE REGEX "/MD5/")
set_source_files_properties (${emu_unity_sources} PROPERTIES UNITY_GROUP "emulation")
set_target_properties (ultraSID PROPERTIES UNITY_BUILD ON UNITY_BUILD_MODE GROUP)

# src/player.cpp and src/EZ/player.cpp both compile only through the unity TU,
# so neither emits its own player.obj, but MSBuild's duplicate-output check
# (MSB8027) scans the raw item list before unity filtering. Give one a distinct
# object name to appease it. Inert everywhere but the VS generator.
set_source_files_properties (${CMAKE_CURRENT_SOURCE_DIR}/Source/libSidplayEZ/src/EZ/player.cpp
							 PROPERTIES VS_SETTINGS "ObjectFileName=$(IntDir)EZ_player.obj")

# The About screen's icon is the canonical app icon, synced from icons/ so it
# ships in the pak without a second committed copy drifting out of date
configure_file (${CMAKE_CURRENT_SOURCE_DIR}/icons/windows_big.png
				${CMAKE_CURRENT_SOURCE_DIR}/Data/UI/png/about-icon.png COPYONLY)

target_include_directories (ultraSID PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source ${libarchive_SOURCE_DIR}/libarchive ${boost_regex_SOURCE_DIR}/include)

target_link_libraries (ultraSID PRIVATE

						gin
						gin_dsp
						gin_simd
						gin_graphics
						gin_gui

						lime_Logger
						lime_ShaderToyComponent
						lime_YamlConfig
						lime_Webcam
						lime_CRTEmulation

						juce::juce_audio_basics
						juce::juce_audio_devices
						juce::juce_audio_formats
						juce::juce_audio_plugin_client
						juce::juce_audio_processors
						juce::juce_audio_utils
						juce::juce_core
						juce::juce_cryptography
						juce::juce_data_structures
						juce::juce_dsp
						juce::juce_events
						juce::juce_graphics
						juce::juce_gui_basics
						juce::juce_gui_extra
						juce::juce_opengl

    					juce::juce_recommended_config_flags

						melatonin_blur

						archive_static
						liblzma

						fmt::fmt
					)

juce_generate_juce_header (ultraSID)

target_compile_definitions (ultraSID PRIVATE
								JUCE_COREGRAPHICS_DRAW_ASYNC=1
								JUCE_DISABLE_COREGRAPHICS_FONT_SMOOTHING=1
								JUCE_MODAL_LOOPS_PERMITTED=0
								JUCE_WEB_BROWSER=0
								JUCE_USE_FLAC=1
								JUCE_USE_WEBP=0
								JUCE_USE_CURL=1
								JUCE_SILENCE_XCODE_15_LINKER_WARNING=1
								JUCE_USE_MP3AUDIOFORMAT=0
								JUCE_USE_LAME_AUDIO_FORMAT=0
								JUCE_USE_WINDOWS_MEDIA_FORMAT=0
								JUCE_USE_CAMERA=0
								JUCE_DONT_ASSERT_ON_GLSL_COMPILE_ERROR=1
								DONT_SET_USING_JUCE_NAMESPACE=1
								_CRT_SECURE_NO_WARNINGS
								GIN_HAS_SIMD=0
								LIBARCHIVE_STATIC
								BOOST_REGEX_STANDALONE
								$<$<CONFIG:Development>:ULTRA_DEVELOPMENT=1>
								# Debug/Development read the naked repo Data folder, baked in
								# so the exe runs from anywhere, with the ultra-shared Data
								# tree as fallback root; Release is pak-only
								$<$<NOT:$<CONFIG:Release>>:ULTRA_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/Data">
								$<$<NOT:$<CONFIG:Release>>:ULTRA_SHARED_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/Source/ultra-shared/Data">
							)

ultra_target (ultraSID DSP PGO)

if (APPLE)
	set_target_properties(ultraSID PROPERTIES
		XCODE_ATTRIBUTE_INFOPLIST_KEY_NSCameraUsageDescription "Add users face to CRT screen"
		XCODE_ATTRIBUTE_CLANG_LINK_OBJC_RUNTIME OFF
		XCODE_ATTRIBUTE_DEPLOYMENT_POSTPROCESSING[variant=Release] ON
		XCODE_ATTRIBUTE_ONLY_ACTIVE_ARCH[variant=Debug] ON
		XCODE_ATTRIBUTE_ONLY_ACTIVE_ARCH[variant=Development] ON)
	target_link_libraries(ultraSID PUBLIC "-framework AVFoundation" "-framework CoreMedia")

	# The whole data set ships inside the bundle as Resources/Data.pak, where
	# datasource reads it: images stored (they carry their own compression),
	# the rest weakest deflate. Release only (Debug/Development read the repo
	# Data folder); the non-Release commands degrade to argument-eating `true`.
	# Runs before any code-signing. Source-only files (bitmap PSDs, submodule
	# .git pointers) stay out. The ultra-shared Data tree packs first, so app
	# entries would win a name clash (matching the naked-mode precedence); it
	# is tiny and text-only, one weakest-deflate pass
	add_custom_command(TARGET ultraSID POST_BUILD
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/Source/ultra-shared/Data
		COMMAND $<IF:$<CONFIG:Release>,rm,true> -rf $<TARGET_BUNDLE_CONTENT_DIR:ultraSID>/Resources/Data $<TARGET_BUNDLE_CONTENT_DIR:ultraSID>/Resources/Data.pak $<TARGET_BUNDLE_CONTENT_DIR:ultraSID>/Resources/CRTEmulation
		COMMAND $<IF:$<CONFIG:Release>,zip,true> -q -1 -r $<TARGET_BUNDLE_CONTENT_DIR:ultraSID>/Resources/Data.pak . -x "*/!src/*" ".*" "*/.*" "*/Thumbs.db" "Thumbs.db"
		VERBATIM)
	add_custom_command(TARGET ultraSID POST_BUILD
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/Data
		COMMAND $<IF:$<CONFIG:Release>,zip,true> -q -0 -r $<TARGET_BUNDLE_CONTENT_DIR:ultraSID>/Resources/Data.pak . -i "*.png" "*.jpg" -x "*/!src/*" ".*" "*/.*"
		COMMAND $<IF:$<CONFIG:Release>,zip,true> -q -1 -r $<TARGET_BUNDLE_CONTENT_DIR:ultraSID>/Resources/Data.pak . -x "*.png" "*.jpg" "*/!src/*" ".*" "*/.*" "*/Thumbs.db" "Thumbs.db"
		VERBATIM)
	# Completeness: every live file made it into the pak. A name clash between
	# the two trees would collapse into one entry and trip the count
	add_custom_command(TARGET ultraSID POST_BUILD
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		COMMAND $<IF:$<CONFIG:Release>,sh,true> -c "expected=$(find Source/ultra-shared/Data Data -type f ! -path '*/!src/*' ! -path '*/.*' ! -name '.*' ! -name 'Thumbs.db' | wc -l); actual=$(zipinfo -1 '$<TARGET_BUNDLE_CONTENT_DIR:ultraSID>/Resources/Data.pak' | grep -vc '/$'); [ \"$expected\" -eq \"$actual\" ] || { echo \"Data.pak entry count mismatch: $actual packed, $expected on disk\"; exit 1; }"
		VERBATIM)
endif()

if (WIN32)
	target_compile_options(ultraSID PRIVATE -showFilenames)
	# The debugger starts in the repo root, where the naked dev Data lives
	set_target_properties(ultraSID PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
	set_property(TARGET ultraSID APPEND_STRING PROPERTY LINK_FLAGS_DEBUG " /INCREMENTAL:NO")
	set_target_properties(ultraSID PROPERTIES LINK_FLAGS "/ignore:4099")
	target_link_libraries (ultraSID PRIVATE mfplat mf mfuuid Mfreadwrite Shlwapi)
	# The inspector compiles and links into Development only; ULTRA_INSPECTOR
	# gates its include, member and key handler
	target_link_libraries (ultraSID PRIVATE $<$<CONFIG:Development>:melatonin_inspector>)
	target_compile_definitions (ultraSID PRIVATE $<$<CONFIG:Development>:ULTRA_INSPECTOR=1>)
endif()

if(UNIX AND NOT APPLE)
	target_compile_options(ultraSID PRIVATE -fno-math-errno -msse4.2 $<$<CONFIG:Debug>:-g -O0> $<$<CONFIG:Development>:-g>)
	# System libcurl via dlopen (a link-time -lcurl carries the builder's
	# distro symbol versions). JUCE adds JUCE_USE_CURL=0 unless asked to link
	# curl; drop it, the app's JUCE_USE_CURL=1 stands alone
	get_target_property(_defs ultraSID COMPILE_DEFINITIONS)
	list(FILTER _defs EXCLUDE REGEX "^JUCE_USE_CURL=")
	set_target_properties(ultraSID PROPERTIES COMPILE_DEFINITIONS "${_defs}")
	target_compile_definitions(ultraSID PRIVATE JUCE_USE_CURL=1 JUCE_LOAD_CURL_SYMBOLS_LAZILY=1)
	# Everything above the OS layer links statically; the OS set (glibc, GL,
	# X11, ALSA, JACK, freetype, fontconfig) stays dynamic. lld: the distro's
	# GNU ld predates the DWARF forms clang emits
	target_link_options(ultraSID PRIVATE -fuse-ld=lld -static-libstdc++ -static-libgcc)
	# The CPU check must not use the instructions it tests for
	set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/Source/App/CpuCheck_Linux.cpp PROPERTIES COMPILE_OPTIONS "-march=x86-64;-mno-sse4.1;-mno-sse4.2")
endif()

# ---------------------------------------------------------------------------
# On-demand tests / benchmarks (EXCLUDE_FROM_ALL: never part of the normal
# build). Build and run explicitly, e.g.:
#   cmake --build --preset vs --config Release --target mverb_ab_test
#   Builds/vs/Release/mverb_ab_test.exe
# Compile flags mirror the app's, so the tests verify what actually ships.
# ---------------------------------------------------------------------------
# libSidplayEZ golden-output regression + performance test. Compiles the
# library sources itself (they only live inside the app target otherwise);
# the emulation .cpps already carry the "emulation" UNITY_GROUP source
# property from above, so with GROUP unity mode this target gets the same
# merged emulation TU, and therefore app-equivalent performance numbers.
set (sidplayez_test_sources ${source_files})
list (FILTER sidplayez_test_sources INCLUDE REGEX "/libSidplayEZ/.*\\.(c|cpp)$")
ultra_console_app (sidplay_ab_test Tests DSP PGO Tests/sidplay_ab_test.cpp ${sidplayez_test_sources}
	Source/Config/HVSCSource.cpp
	Source/ultra-shared/Config/PakFile.cpp
	Source/ultra-shared/Config/ZipFolder.cpp)
target_compile_definitions (sidplay_ab_test PRIVATE SIDPLAY_TEST_ROOT="${CMAKE_CURRENT_SOURCE_DIR}")
set_target_properties (sidplay_ab_test PROPERTIES UNITY_BUILD ON UNITY_BUILD_MODE GROUP)

# FX / EQ golden-output regression + performance test. The effects are
# header-only and self-contained, so no library sources are needed.
ultra_test (fx_ab_test DSP Tests/fx_ab_test.cpp)
target_compile_definitions (fx_ab_test PRIVATE FX_TEST_ROOT="${CMAKE_CURRENT_SOURCE_DIR}")

# FX mode-loudness measurement (diagnostic, no golden reference): runs the
# sidplay reference WAVs through all four SIDEffects chains and prints EBU
# R128 integrated loudness per mode, the drift ruler for the fxGains trims.
ultra_test (fx_mode_loudness DSP Tests/fx_mode_loudness.cpp
	Source/Audio/SIDEffects.cpp
	Source/Audio/PerceivedLoudness.cpp)
target_compile_definitions (fx_mode_loudness PRIVATE LOUDNESS_TEST_ROOT="${CMAKE_CURRENT_SOURCE_DIR}")

ultra_test (mverb_ab_test DSP Tests/mverb_ab_test.cpp)

# STIL quote-detector scan (diagnostic + regression): runs splitCommentQuotes
# over the real STIL.txt (loose folder or zip) and reports quote statistics;
# fails when the detector regresses
ultra_console_app (stil_scan Tests Tests/stil_scan.cpp
	Source/Config/HVSCSource.cpp
	Source/ultra-shared/Config/PakFile.cpp
	Source/ultra-shared/Config/ZipFolder.cpp
	Source/ultra-shared/Helpers/Regex.cpp)
target_include_directories (stil_scan PRIVATE ${boost_regex_SOURCE_DIR}/include)
target_compile_definitions (stil_scan PRIVATE BOOST_REGEX_STANDALONE STIL_TEST_ROOT="${CMAKE_CURRENT_SOURCE_DIR}")

# PerceivedLoudness validation: EBU Tech 3341 minimum set, dual mono, chunk
# invariance, progressive readout, silence.
ultra_test (perceived_loudness_test DSP Tests/perceived_loudness_test.cpp
	Source/Audio/PerceivedLoudness.cpp)

# ZipFolder round-trip: Writer-built archives, staged mutations, commit
# rewrites and zip64 entry counts, cross-checked against juce::ZipFile and an
# independent CRC implementation.
ultra_console_app (zipfolder_test Tests Tests/zipfolder_test.cpp
	Source/ultra-shared/Config/PakFile.cpp
	Source/ultra-shared/Config/ZipFolder.cpp)

# HVSCUpdater dual-tree regression: one synthetic update script applied to a
# loose folder and to a zip archive must produce identical collections; a
# failing script must leave the zip on disk untouched.
ultra_console_app (hvscupdater_test Tests Tests/hvscupdater_test.cpp
	Source/Database/HVSCUpdater/HVSCTree.cpp
	Source/Database/HVSCUpdater/HVSCUpdater.cpp
	Source/Database/HVSCUpdater/updater_sidtune.cpp
	Source/ultra-shared/Config/PakFile.cpp
	Source/ultra-shared/Config/ZipFolder.cpp)

#
# Tools (see ultrasid_tools.md): the HVSC scanner and the database builder that
# produce ultraSID.db. Never part of the default build, built by name only,
# e.g. cmake --build --preset vs --config Release --target sid_scanner
#

# sid_scanner measures per sub-tune: loudness (LUFS), digi, filter, loop and
# delayed start
juce_add_gui_app (sid_scanner PRODUCT_NAME sid_scanner)

file (GLOB_RECURSE sid_scanner_sources CONFIGURE_DEPENDS
	${CMAKE_CURRENT_SOURCE_DIR}/Tools/sid_scanner/Source/*.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/Tools/sid_scanner/Source/*.h)

target_sources (sid_scanner PRIVATE ${sid_scanner_sources} ${sidplayez_test_sources}
	Source/Audio/PerceivedLoudness.cpp
	Source/Config/HVSCSource.cpp
	Source/Config/TunePatches.cpp
	Source/ultra-shared/Config/PakFile.cpp
	Source/ultra-shared/Config/ZipFolder.cpp
	Source/ultra-shared/Helpers/Regex.cpp)
source_group (TREE ${CMAKE_CURRENT_SOURCE_DIR}/Tools/sid_scanner/Source PREFIX Source FILES ${sid_scanner_sources})

target_include_directories (sid_scanner PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Tools/sid_scanner/Source ${CMAKE_CURRENT_SOURCE_DIR}/Source ${boost_regex_SOURCE_DIR}/include)

target_link_libraries (sid_scanner PRIVATE
						juce::juce_audio_basics
						juce::juce_core
						juce::juce_data_structures
						juce::juce_events
						juce::juce_graphics
						juce::juce_gui_basics

						lime_Logger
						fmt::fmt

						juce::juce_recommended_config_flags
					)

juce_generate_juce_header (sid_scanner)

target_compile_definitions (sid_scanner PRIVATE
								JUCE_MODAL_LOOPS_PERMITTED=0
								JUCE_WEB_BROWSER=0
								JUCE_USE_FLAC=0
								JUCE_USE_CURL=0
								JUCE_USE_MP3AUDIOFORMAT=0
								JUCE_USE_LAME_AUDIO_FORMAT=0
								JUCE_USE_WINDOWS_MEDIA_FORMAT=0
								_CRT_SECURE_NO_WARNINGS
								BOOST_REGEX_STANDALONE
								DONT_SET_USING_JUCE_NAMESPACE=1
								SID_TOOLS_ROOT="${CMAKE_CURRENT_SOURCE_DIR}"
							)

set_target_properties (sid_scanner PROPERTIES EXCLUDE_FROM_ALL TRUE UNITY_BUILD ON UNITY_BUILD_MODE GROUP)
ultra_target (sid_scanner DSP PGO)
if (WIN32)
	# Symbols for the optimized config, so crashes in real rendering speed stay debuggable
	target_compile_options (sid_scanner PRIVATE $<$<CONFIG:Development>:/Z7>)
	target_link_options (sid_scanner PRIVATE $<$<CONFIG:Development>:/DEBUG>)
endif()

# The database builder lives inside sid_scanner (DatabaseBuilder.cpp),
# triggered with: sid_scanner --build-db

# sidid_search finds every SID file a sidid signature matches (existing player
# or an ad-hoc signature line), to vet new sidid.cfg entries, see the usage
# comment in the source
ultra_console_app (sidid_search Tools Tools/sidid_search/sidid_search.cpp
	Source/libSidplayEZ/src/EZ/sidid.cpp
	Source/ultra-shared/Config/PakFile.cpp)

# hvsc_update applies official HVSC update archives to the zip collection
# through the app's own updater chain; Tools/update_hvsc.sh drives it
ultra_console_app (hvsc_update Tools Tools/hvsc_update/hvsc_update.cpp
	Source/Config/HVSCSource.cpp
	Source/Database/HVSCUpdater/HVSCTree.cpp
	Source/Database/HVSCUpdater/HVSCUpdater.cpp
	Source/Database/HVSCUpdater/updater_sidtune.cpp
	Source/Helpers/Unarchiver.cpp
	Source/ultra-shared/Config/PakFile.cpp
	Source/ultra-shared/Config/ZipFolder.cpp)
target_include_directories (hvsc_update PRIVATE ${libarchive_SOURCE_DIR}/libarchive)
target_link_libraries (hvsc_update PRIVATE archive_static liblzma)
target_compile_definitions (hvsc_update PRIVATE LIBARCHIVE_STATIC)

# Regenerates the psid driver hex dump (psiddrv.bin) when psiddrv.a65 changed,
# a no-op otherwise so the target is safe to run every time. Requires cc65
# (ca65/ld65) on PATH when it actually rebuilds. The stamp keeps psiddrv.bin
# out of the build graph (a source includes it), so only this target runs it.
set(psiddrv_dir ${CMAKE_CURRENT_SOURCE_DIR}/Source/libSidplayEZ/src)
set(psiddrv_stamp ${CMAKE_CURRENT_BINARY_DIR}/psiddrv.stamp)
add_custom_command(OUTPUT ${psiddrv_stamp}
	COMMAND ${CMAKE_COMMAND} -P ${psiddrv_dir}/make-psiddrv.cmake
	COMMAND ${CMAKE_COMMAND} -E touch ${psiddrv_stamp}
	DEPENDS ${psiddrv_dir}/psiddrv.a65 ${psiddrv_dir}/make-psiddrv.cmake
	COMMENT "Assembling psiddrv.a65")
add_custom_target(psiddrv DEPENDS ${psiddrv_stamp})

# Solution folders, keeps the IDE's project tree tidy; the generated rc_libs
# land in utility with the rest of the machinery nobody opens
set_target_properties (sid_scanner psiddrv PROPERTIES FOLDER Tools)
if (WIN32)
	set_target_properties (ultraSID_rc_lib sid_scanner_rc_lib PROPERTIES FOLDER utility)
endif()
foreach(_t archive_static fmt fmt-c libgnu liblzma melatonin_inspector melatonin_blur)
	if (TARGET ${_t})
		set_target_properties (${_t} PROPERTIES FOLDER Dependencies)
	endif()
endforeach()
