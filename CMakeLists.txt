# 子项目 CMakeLists.txt

cmake_minimum_required(VERSION 3.22)


# ---- Options ----

option(COPY_BUILD "Copy the build output to the Fallout 4 directory." ON)

# ---- Cache build vars ----

macro(set_from_environment VARIABLE)
	if (NOT DEFINED ${VARIABLE} AND DEFINED ENV{${VARIABLE}})
		set(${VARIABLE} $ENV{${VARIABLE}})
	endif ()
endmacro()

set_from_environment(Fallout4Path)
set_from_environment(VCPKG_ROOT)

if (DEFINED VCPKG_ROOT)
	set(CMAKE_TOOLCHAIN_FILE "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
	set(VCPKG_TARGET_TRIPLET "x64-windows-static-md" CACHE STRING "")
else ()
	message(
		WARNING
		"Variable VCPKG_ROOT is not set. Continuing without vcpkg."
	)
endif ()

set(Boost_USE_STATIC_RUNTIME OFF CACHE BOOL "")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL" CACHE STRING "")

if (MSVC)
	if (NOT ${CMAKE_GENERATOR} STREQUAL "Ninja")
		add_compile_options(
			/MP	# Build with Multiple Processes
		)
	endif ()
endif ()

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_DEBUG OFF)

set(Boost_USE_STATIC_LIBS ON)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    add_definitions(-D_AMD64_ -D_WIN64)
else()
    add_definitions(-D_X86_)
endif()

project(
	TrueThroughScope
	VERSION 0.0.1
	LANGUAGES CXX
)

find_package(spdlog REQUIRED CONFIG)
find_package(imgui REQUIRED CONFIG)
find_package(nlohmann_json REQUIRED CONFIG)
find_library(DETOURS_LIBRARY detours REQUIRED)
find_package(minhook CONFIG REQUIRED)

configure_file(
	${CMAKE_CURRENT_SOURCE_DIR}/cmake/Version.h.in
	${CMAKE_CURRENT_BINARY_DIR}/include/Version.h
	@ONLY
)

configure_file(
	${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.rc.in
	${CMAKE_CURRENT_BINARY_DIR}/version.rc
	@ONLY
)

if (NOT TARGET CommonLibF4)
	add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../CommonLibF4" CommonLibF4)
endif ()

include(cmake/sourcelist.cmake)

source_group(
	TREE ${CMAKE_CURRENT_BINARY_DIR}
	FILES ${CMAKE_CURRENT_BINARY_DIR}/include/Version.h
)


file(GLOB HLSL_FILES src/HLSL/*.hlsl)

add_library(
	${PROJECT_NAME}
	SHARED
	${HLSL_FILES}
	${SOURCES}
	${CMAKE_CURRENT_BINARY_DIR}/version.rc
	.clang-format
)

macro(set_hlsl_properties HLSL_FILE SHADER_TYPE)
  set_source_files_properties(${HLSL_FILE} 
  PROPERTIES VS_SHADER_OBJECT_FILE_NAME 
  "${Fallout4Path}/Data/Shaders/XiFeiLi/%(Filename).cso" 
  VS_SHADER_TYPE ${SHADER_TYPE} VS_SHADER_MODEL 5.0 VS_SHADER_ENTRYPOINT main)
endmacro()

set_hlsl_properties(src/HLSL/MirrorShader_VS.hlsl Vertex)
set_hlsl_properties(src/HLSL/MirrorShader_PS.hlsl Pixel)

target_include_directories(
	${PROJECT_NAME}
	PRIVATE
		#E:/CInclude
		${CMAKE_CURRENT_BINARY_DIR}/include
		${CMAKE_CURRENT_SOURCE_DIR}/src
		${CMAKE_CURRENT_SOURCE_DIR}/include
		${IMGUI_INCLUDE_DIRS}

)


target_link_libraries(
	${PROJECT_NAME}
	PRIVATE
		minhook::minhook
		CommonLibF4::CommonLibF4
		spdlog::spdlog
		imgui::imgui
		${DETOURS_LIBRARY}
)

target_precompile_headers(
	${PROJECT_NAME}
	PRIVATE
		src/PCH.h
)


link_directories(${CMAKE_CURRENT_SOURCE_DIR}/lib)

if (MSVC)
	target_compile_options(
		${PROJECT_NAME}
		PRIVATE
			/sdl	# Enable Additional Security Checks
			#/utf-8	# Set Source and Executable character sets to UTF-8
			/Zi	# Debug Information Format

			/permissive-	# Standards conformance
			/Zc:preprocessor	# Enable preprocessor conformance mode

			/W4	# Warning level
			/WX-# Warning level (warnings are errors)

			"$<$<CONFIG:DEBUG>:>"
			"$<$<CONFIG:RELEASE>:/Zc:inline;/JMC-;/Ob3>"
	)

	target_link_options(
		${PROJECT_NAME}
		PRIVATE
			#/WX-# Treat Linker Warnings as Errors
			"$<$<CONFIG:DEBUG>:/INCREMENTAL;/OPT:NOREF;/OPT:NOICF>"
			"$<$<CONFIG:RELEASE>:/INCREMENTAL:NO;/OPT:REF;/OPT:ICF;/DEBUG:FULL>"
	)
endif ()



add_custom_command(
		TARGET ${PROJECT_NAME}
		POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${PROJECT_NAME}> "${Fallout4Path}/Data/F4SE/Plugins/"
		COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_PDB_FILE:${PROJECT_NAME}> "${Fallout4Path}/Data/F4SE/Plugins/"
		COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${PROJECT_NAME}> "G:/Games/FO4_SO/mods/Fake Through Scope testing/F4SE/Plugins/"
		COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_PDB_FILE:${PROJECT_NAME}> "G:/Games/FO4_SO/mods/Fake Through Scope testing/F4SE/Plugins/"
	)

# ---- Build artifacts ----

set(SCRIPT "scripts/archive_artifacts.py")


add_custom_target(
	create_artifact_TTS
	COMMAND
		"${CMAKE_CURRENT_SOURCE_DIR}/${SCRIPT}"
		"--name=\"${PROJECT_NAME}\""
		"--dll=\"$<TARGET_FILE:${PROJECT_NAME}>\""
		"--pdb=\"$<TARGET_PDB_FILE:${PROJECT_NAME}>\""
	WORKING_DIRECTORY
		${CMAKE_CURRENT_BINARY_DIR}
	SOURCES
		${SCRIPT}
)

add_dependencies(
	create_artifact_TTS
	${PROJECT_NAME}
)

source_group(
	TREE ${CMAKE_CURRENT_SOURCE_DIR}
	FILES ${SCRIPT}
)

