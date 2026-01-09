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


set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "")
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
#cfind_package(imgui REQUIRED CONFIG)
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

# 添加 ImGui 子目录
set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../imgui")

# 检查 ImGui 目录是否存在
if(NOT EXISTS ${IMGUI_DIR})
    message(FATAL_ERROR "ImGui directory not found at ${IMGUI_DIR}")
endif()

# 创建 ImGui 静态库
add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    
    # 后端文件（根据你使用的后端选择）
    ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
    ${IMGUI_DIR}/backends/imgui_impl_dx11.cpp
    ${IMGUI_DIR}/misc/cpp/imgui_stdlib.cpp
    # 如果使用其他后端，添加相应的文件
    # ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
    # ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
)

# 设置 ImGui 的包含目录
target_include_directories(imgui PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

# 启用 docking 功能
target_compile_definitions(imgui PUBLIC
    IMGUI_ENABLE_DOCKING
    # 如果需要多视口支持，取消下面注释
    # IMGUI_ENABLE_VIEWPORTS
)

if(WIN32)
    target_link_libraries(imgui PUBLIC
        d3d11
        dxgi
    )
endif()

file(GLOB HLSL_FILES src/HLSL/*.hlsl)

add_library(
	${PROJECT_NAME}
	SHARED
	${HLSL_FILES}
	${SOURCES}
	${CMAKE_CURRENT_BINARY_DIR}/version.rc
	.clang-format
)

# 定义输出路径列表
set(HLSL_OUTPUT_PATHS
    "G:/Games/FO4_SO/mods/TTS True Through Scope/Shaders/XiFeiLi"
    "G:/Games/Fallout 4 ori/Fallout 4/Mo2/mods/TTS/Shaders/XiFeiLi"
	"I:/Games/Fallout 4 Pack/FO4SO_1.5/mods/TTS/Shaders/XiFeiLi"
)

macro(set_hlsl_properties HLSL_FILE SHADER_TYPE ENTRY_POINT)
  # 设置主要的编译属性，使用第一个路径作为编译输出
  list(GET HLSL_OUTPUT_PATHS 0 PRIMARY_OUTPUT_PATH)
  set_source_files_properties(${HLSL_FILE} 
    PROPERTIES 
    VS_SHADER_OBJECT_FILE_NAME "${PRIMARY_OUTPUT_PATH}/%(Filename).cso"
    VS_SHADER_TYPE ${SHADER_TYPE} 
    VS_SHADER_MODEL 5.0 
    VS_SHADER_ENTRYPOINT ${ENTRY_POINT}
  )
  
  # 获取文件名（不含扩展名）
  get_filename_component(SHADER_NAME ${HLSL_FILE} NAME_WE)
  
  # 为每个额外的路径添加自定义命令来复制cso文件
  list(LENGTH HLSL_OUTPUT_PATHS PATH_COUNT)
  if(PATH_COUNT GREATER 1)
    # 跳过第一个路径（已经是编译输出路径）
    math(EXPR LAST_INDEX "${PATH_COUNT} - 1")
    foreach(i RANGE 1 ${LAST_INDEX})
      list(GET HLSL_OUTPUT_PATHS ${i} OUTPUT_PATH)
      add_custom_command(
        TARGET ${PROJECT_NAME}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${OUTPUT_PATH}"
        COMMAND ${CMAKE_COMMAND} -E copy "${PRIMARY_OUTPUT_PATH}/${SHADER_NAME}.cso" "${OUTPUT_PATH}/${SHADER_NAME}.cso"
        COMMENT "复制 ${SHADER_NAME}.cso 到 ${OUTPUT_PATH}"
      )
    endforeach()
  endif()
endmacro()

set_hlsl_properties(src/HLSL/TrueScopeShader.hlsl Pixel main)
set_hlsl_properties(src/HLSL/ThermalVision_PS.hlsl Pixel main)
set_hlsl_properties(src/HLSL/ScopeMV_PS.hlsl Pixel main)
set_hlsl_properties(src/HLSL/ScopeMV_VS.hlsl Vertex main)


target_include_directories(
	${PROJECT_NAME}
	PRIVATE
		#E:/CInclude
		${CMAKE_CURRENT_BINARY_DIR}/include
		${CMAKE_CURRENT_SOURCE_DIR}/src
		${CMAKE_CURRENT_SOURCE_DIR}/src/rendering
		${CMAKE_CURRENT_SOURCE_DIR}/include
		${CMAKE_CURRENT_SOURCE_DIR}/src/UI
		${CMAKE_CURRENT_SOURCE_DIR}/src/UI/Localization
		${CMAKE_CURRENT_SOURCE_DIR}/src/UI/Panels
		#${IMGUI_INCLUDE_DIRS}

)


target_link_libraries(
	${PROJECT_NAME}
	PRIVATE
		minhook::minhook
		CommonLibF4::CommonLibF4
		spdlog::spdlog
		#imgui::imgui
		imgui
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
			/utf-8	# Set Source and Executable character sets to UTF-8
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
		COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${PROJECT_NAME}> "G:/Games/FO4_SO/mods/TTS True Through Scope/F4SE/Plugins/"
		COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_PDB_FILE:${PROJECT_NAME}> "G:/Games/FO4_SO/mods/TTS True Through Scope/F4SE/Plugins/"
		COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${PROJECT_NAME}> "G:/Games/Fallout 4 ori/Fallout 4/Mo2/mods/TTS/F4SE/Plugins/"
		COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_PDB_FILE:${PROJECT_NAME}> "G:/Games/Fallout 4 ori/Fallout 4/Mo2/mods/TTS/F4SE/Plugins/"
		COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${PROJECT_NAME}> "I:/Games/Fallout 4 Pack/FO4SO_1.5/mods/TTS/F4SE/Plugins/"
		COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_PDB_FILE:${PROJECT_NAME}> "I:/Games/Fallout 4 Pack/FO4SO_1.5/mods/TTS/F4SE/Plugins/"
	)

# ---- Build artifacts ----

set(SCRIPT "scripts/archive_artifacts.py")


add_custom_target(
	create_artifact_TTS
	COMMAND
		"${CMAKE_CURRENT_SOURCE_DIR}/${SCRIPT}"
		"--name=${PROJECT_NAME}"
		"--dll=$<TARGET_FILE:${PROJECT_NAME}>"
		"--pdb=$<TARGET_PDB_FILE:${PROJECT_NAME}>"
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

