
if(WITH_LLVM)
	set(USE_LLVM_VERSION 20.1.3)
	if (NOT MSVC)
		check_cxx_compiler_flag("-msse -msse2 -mcx16" COMPILER_X86)
		check_cxx_compiler_flag("-march=armv8-a+lse" COMPILER_ARM)
	else()
		set(COMPILER_X86 on) # TODO
	endif()

	if(BUILD_LLVM)
		message(STATUS "LLVM will be built from the submodule.")

		if (ANDROID)
			if (COMPILER_ARM)
				set(LLVM_TARGETS_TO_BUILD "AArch64" CACHE STRING "Semicolon-separated list of targets to build, or \"all\".")
				set(LLVM_TARGET_ARCH "${CMAKE_SYSTEM_PROCESSOR}-none-linux-android${ANDROID_NATIVE_API_LEVEL}" CACHE STRING "")
			else()
				set(LLVM_TARGETS_TO_BUILD "X86" CACHE STRING "Semicolon-separated list of targets to build, or \"all\".")
			endif()
		else()
			set(LLVM_TARGETS_TO_BUILD "AArch64;X86" CACHE STRING "Semicolon-separated list of targets to build, or \"all\".")
		endif()

		option(LLVM_BUILD_RUNTIME "Build the LLVM runtime libraries." OFF)
		option(LLVM_BUILD_TOOLS "Build the LLVM tools. If OFF, just generate build targets." OFF)
		option(LLVM_INCLUDE_BENCHMARKS "Generate benchmark targets. If OFF, benchmarks can't be built." OFF)
		option(LLVM_INCLUDE_DOCS "Generate build targets for llvm documentation." OFF)
		option(LLVM_INCLUDE_EXAMPLES "Generate build targets for the LLVM examples" OFF)
		option(LLVM_INCLUDE_TESTS "Generate build targets for the LLVM unit tests." OFF)
		option(LLVM_INCLUDE_TOOLS "Generate build targets for the LLVM tools." OFF)
		option(LLVM_INCLUDE_UTILS "Generate build targets for the LLVM utils." OFF)
		option(LLVM_CCACHE_BUILD "Set to ON for a ccache enabled build" OFF)
		set(LLVM_ENABLE_WARNINGS OFF CACHE BOOL "Enable compiler warnings.")

		# For Windows x86 (not Windows AArch64).
		# Check on MSVC is needed due to COMPILER_X86, COMPILER_ARM etc. are not set/supported by the MSVC compiler, if used.
		# Furthermore, the MSVC compiler is not available/supported on Windows AArch64
		if(WIN32 AND (COMPILER_X86 OR MSVC))
			set(LLVM_USE_INTEL_JITEVENTS ON)
		endif()

		set(LLVM_DOWNLOAD_BINARY "")

		if (ANDROID)
			string(APPEND LLVM_DOWNLOAD_BINARY llvm-android-)

			if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
				string(APPEND LLVM_DOWNLOAD_BINARY arm64-v8a)
			else()
				string(APPEND LLVM_DOWNLOAD_BINARY x64)
			endif()

			string(APPEND LLVM_DOWNLOAD_BINARY .7z)
		elseif ((WIN32 AND MSVC) OR LINUX)
			string(APPEND LLVM_DOWNLOAD_BINARY llvm-)
			if (WIN32)
				string(APPEND LLVM_DOWNLOAD_BINARY windows-)
			else()
				string(APPEND LLVM_DOWNLOAD_BINARY linux-)
			endif()

			if (COMPILER_X86)
				string(APPEND LLVM_DOWNLOAD_BINARY x64-)
			else()
				string(APPEND LLVM_DOWNLOAD_BINARY aarch64-)
			endif()

			if (WIN32)
				if (USE_MSVC_STATIC_CRT)
					string(APPEND LLVM_DOWNLOAD_BINARY MT)
				else()
					string(APPEND LLVM_DOWNLOAD_BINARY MD)
				endif()
			endif()

			string(APPEND LLVM_DOWNLOAD_BINARY .7z)
		endif()

		if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
			if(COMPILER_X86)
				set(LLVM_USE_INTEL_JITEVENTS ON)
			endif()
			set(LLVM_USE_PERF ON)
		endif()

		if (MSVC)
			add_compile_definitions("$<$<COMPILE_LANGUAGE:CXX>:_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS>")
		endif()

		if (LLVM_DOWNLOAD_BINARY STREQUAL "")
			# LLVM needs to be built out-of-tree
			add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/llvm/llvm ${CMAKE_CURRENT_BINARY_DIR}/llvm_build EXCLUDE_FROM_ALL)
			set(LLVM_DIR "${CMAKE_CURRENT_BINARY_DIR}/llvm_build/lib/cmake/llvm/")
			set(MLIR_DIR "${CMAKE_CURRENT_BINARY_DIR}/llvm_build/lib/cmake/mlir/")
		else()
			set(LLVM_DOWNLOAD_LINK https://github.com/RPCSX/llvm-build/releases/download/${USE_LLVM_VERSION})

			if (NOT EXISTS "${CMAKE_CURRENT_BINARY_DIR}/${USE_LLVM_VERSION}-${LLVM_DOWNLOAD_BINARY}" AND
				NOT EXISTS "${CMAKE_CURRENT_BINARY_DIR}/${USE_LLVM_VERSION}-${LLVM_DOWNLOAD_BINARY}.unpacked")
				message(STATUS "Downloading LLVM")
				file(DOWNLOAD ${LLVM_DOWNLOAD_LINK}/${LLVM_DOWNLOAD_BINARY}
					"${CMAKE_CURRENT_BINARY_DIR}/${USE_LLVM_VERSION}-${LLVM_DOWNLOAD_BINARY}.tmp" SHOW_PROGRESS
					STATUS FILE_STATUS)
				list(GET FILE_STATUS 0 STATUS_CODE)
				if (NOT STATUS_CODE EQUAL 0)
					file(REMOVE "${CMAKE_CURRENT_BINARY_DIR}/${USE_LLVM_VERSION}-${LLVM_DOWNLOAD_BINARY}.tmp")
					message(FATAL_ERROR "Failed to download LLVM")
				endif()

				file(RENAME
					"${CMAKE_CURRENT_BINARY_DIR}/${USE_LLVM_VERSION}-${LLVM_DOWNLOAD_BINARY}.tmp"
					"${CMAKE_CURRENT_BINARY_DIR}/${USE_LLVM_VERSION}-${LLVM_DOWNLOAD_BINARY}"
				)
			endif()

			if(NOT EXISTS "${CMAKE_CURRENT_BINARY_DIR}/${USE_LLVM_VERSION}-${LLVM_DOWNLOAD_BINARY}.unpacked")
				file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${USE_LLVM_VERSION}-${LLVM_DOWNLOAD_BINARY}.dir")
				execute_process(COMMAND ${CMAKE_COMMAND} -E tar xzf "${CMAKE_CURRENT_BINARY_DIR}/${USE_LLVM_VERSION}-${LLVM_DOWNLOAD_BINARY}"
					WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${USE_LLVM_VERSION}-${LLVM_DOWNLOAD_BINARY}.dir" RESULT_VARIABLE STATUS_CODE)

				if (NOT STATUS_CODE EQUAL 0)
					message(FATAL_ERROR "Failed to unpack LLVM")
				endif()
				file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/${USE_LLVM_VERSION}-${LLVM_DOWNLOAD_BINARY}.unpacked")
				file(REMOVE "${CMAKE_CURRENT_BINARY_DIR}/${USE_LLVM_VERSION}-${LLVM_DOWNLOAD_BINARY}")
			endif()

			file(GLOB LLVM_ROOT_DIR_LIST LIST_DIRECTORIES true "${CMAKE_CURRENT_BINARY_DIR}/${USE_LLVM_VERSION}-${LLVM_DOWNLOAD_BINARY}.dir/*")
			list(GET LLVM_ROOT_DIR_LIST 0 LLVM_ROOT_DIR)
			set(LLVM_DIR "${LLVM_ROOT_DIR}/lib/cmake/llvm")
			set(MLIR_DIR "${LLVM_ROOT_DIR}/lib/cmake/mlir")

			if (NOT EXISTS "${LLVM_DIR}")
				message(FATAL_ERROR "Failed to locate LLVM: ${LLVM_ROOT_DIR}")
			endif()
			
			if (NOT EXISTS "${MLIR_DIR}")
				message(FATAL_ERROR "Failed to locate MLIR: ${LLVM_ROOT_DIR}")
			endif()

			if (NOT ANDROID)
				set(Clang_DIR "${LLVM_ROOT_DIR}/lib/cmake/clang")
				if (NOT EXISTS "${Clang_DIR}")
					message(FATAL_ERROR "Failed to locate Clang: ${LLVM_ROOT_DIR}")
				endif()
			endif()
		endif()

		set(STATIC_LINK_LLVM ON CACHE BOOL "Link against LLVM statically. This will get set to ON if you build LLVM from the submodule." FORCE)

		find_package(LLVM ${USE_LLVM_VERSION} CONFIG)
		find_package(MLIR ${USE_LLVM_VERSION} CONFIG)

		if(NOT LLVM_FOUND OR NOT MLIR_FOUND)
			message(FATAL_ERROR "Couldn't build LLVM from the submodule. You might need to run `git submodule update --init`")
		endif()

		if (NOT ANDROID)
			find_package(Clang ${USE_LLVM_VERSION} CONFIG)
			if(NOT Clang_FOUND)
				message(FATAL_ERROR "Couldn't build Clang from the submodule. You might need to run `git submodule update --init`")
			endif()
		endif()
	else()
		message(STATUS "Using prebuilt or system LLVM")

		if (LLVM_DIR AND NOT IS_ABSOLUTE "${LLVM_DIR}")
			# change relative LLVM_DIR to be relative to the source dir
			set(LLVM_DIR ${CMAKE_SOURCE_DIR}/${LLVM_DIR})
		endif()

		if (MLIR_DIR AND NOT IS_ABSOLUTE "${MLIR_DIR}")
			set(MLIR_DIR ${CMAKE_SOURCE_DIR}/${MLIR_DIR})
		endif()

		if (Clang_DIR AND NOT IS_ABSOLUTE "${Clang_DIR}")
			set(Clang_DIR ${CMAKE_SOURCE_DIR}/${Clang_DIR})
		endif()

		find_package(LLVM CONFIG)
		find_package(MLIR CONFIG)

		if (NOT LLVM_FOUND)
			message(FATAL_ERROR "Can't find LLVM libraries from the CMAKE_PREFIX_PATH path or LLVM_DIR. \
													 Enable BUILD_LLVM option to build LLVM from included as a git submodule.")
		endif()

		if (LLVM_VERSION VERSION_LESS 18)
			message(FATAL_ERROR "Found LLVM version ${LLVM_VERSION}. Required version 18 or above.")
		endif()

		if (NOT MLIR_FOUND)
			message(FATAL_ERROR "Can't find MLIR libraries from the CMAKE_PREFIX_PATH path or MLIR_DIR: ${MLIR_DIR}")
		endif()


		if (NOT ANDROID)
			find_package(Clang CONFIG)

			if (NOT Clang_FOUND)
				message(FATAL_ERROR "Can't find Clang from the CMAKE_PREFIX_PATH path or Clang_DIR.")
			endif()
		endif()
	endif()

	if (STATIC_LINK_LLVM)
		if (NOT DEFINED LLVM_TARGETS_TO_BUILD)
			if(COMPILER_ARM)
				set(LLVM_TARGETS_TO_BUILD "AArch64" CACHE STRING "Semicolon-separated list of targets to build, or \"all\".")
			else()
				set(LLVM_TARGETS_TO_BUILD "X86" CACHE STRING "Semicolon-separated list of targets to build, or \"all\".")
			endif()
		endif()

		# For Windows x86 (not Windows AArch64) only when BUILD_LLVM is enabled and
		# for Linux x86 (not Linux AArch64) even if BUILD_LLVM is disabled (precompiled llvm used)
		if(LLVM_USE_INTEL_JITEVENTS OR (CMAKE_SYSTEM_NAME STREQUAL "Linux" AND COMPILER_X86))
			list (APPEND LLVM_ADDITIONAL_LIBS IntelJITEvents)
		endif()

		# For Linux even if BUILD_LLVM is disabled (precompiled llvm used)
		# if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
		# 	list (APPEND LLVM_ADDITIONAL_LIBS PerfJITEvents)
		# endif()

		llvm_map_components_to_libnames(LLVM_LIBS
			${LLVM_TARGETS_TO_BUILD}
			${LLVM_ADDITIONAL_LIBS}
			Core
			ExecutionEngine
			MCJIT
			Passes
		)

		set(MLIR_LIBS MLIRIR MLIRInferTypeOpInterface MLIRFuncDialect MLIRSCFDialect MLIRSCFToControlFlow MLIRAffineAnalysis MLIRAsyncToLLVM)
	else()
		set(LLVM_LIBS LLVM MLIR)
	endif()

	list(APPEND CMAKE_MODULE_PATH "${MLIR_CMAKE_DIR}")
	list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
	
	include(TableGen)
	include(AddLLVM)
	include(AddMLIR)

	if (NOT ANDROID)
		list(APPEND CMAKE_MODULE_PATH "${CLANG_CMAKE_DIR}")
		include(AddClang)

		get_target_property(CLANG_EXECUTABLE clang LOCATION)
	endif()
	# include(HandleLLVMOptions)

	add_library(3rdparty_llvm INTERFACE)
	target_link_libraries(3rdparty_llvm INTERFACE ${LLVM_LIBS})
	target_include_directories(3rdparty_llvm INTERFACE ${LLVM_INCLUDE_DIRS})
	separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
	target_compile_definitions(3rdparty_llvm INTERFACE ${LLVM_DEFINITIONS_LIST} LLVM_AVAILABLE)

	add_library(3rdparty_mlir INTERFACE)
	target_link_libraries(3rdparty_mlir INTERFACE 3rdparty_llvm ${MLIR_LIBS})
	target_include_directories(3rdparty_mlir INTERFACE ${MLIR_INCLUDE_DIRS})
	separate_arguments(MLIR_DEFINITIONS_LIST NATIVE_COMMAND ${MLIR_DEFINITIONS})
	target_compile_definitions(3rdparty_mlir INTERFACE ${MLIR_DEFINITIONS_LIST} MLIR_AVAILABLE)

	add_library(3rdparty::llvm ALIAS 3rdparty_llvm)
	add_library(3rdparty::mlir ALIAS 3rdparty_mlir)
else()
	add_library(3rdparty::llvm ALIAS 3rdparty_dummy_lib)
	add_library(3rdparty::mlir ALIAS 3rdparty_dummy_lib)
endif()
