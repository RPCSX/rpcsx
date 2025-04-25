cmake_minimum_required(VERSION 3.20)
project(moltenvk NONE)
include(ExternalProject)

if (CMAKE_SYSTEM_NAME MATCHES "iOS")
	ExternalProject_Add(moltenvk
		GIT_REPOSITORY https://github.com/KhronosGroup/MoltenVK.git
		GIT_TAG 6f892b6
		BUILD_IN_SOURCE YES
		SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/MoltenVK
		CONFIGURE_COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/MoltenVK/fetchDependencies" --ios
		BUILD_COMMAND xcodebuild build -quiet -project "${CMAKE_CURRENT_SOURCE_DIR}/MoltenVK/MoltenVKPackaging.xcodeproj" -scheme "MoltenVK Package \(iOS only\)" -configuration "Release" -arch "arm64"
		COMMAND ""
		INSTALL_COMMAND ""
		BUILD_BYPRODUCTS ""
	)
else()
	ExternalProject_Add(moltenvk
		GIT_REPOSITORY https://github.com/KhronosGroup/MoltenVK.git
		GIT_TAG 81541f6
		BUILD_IN_SOURCE 1
		SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/MoltenVK
		CONFIGURE_COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/MoltenVK/fetchDependencies" --macos
		BUILD_COMMAND xcodebuild build -quiet -project "${CMAKE_CURRENT_SOURCE_DIR}/MoltenVK/MoltenVKPackaging.xcodeproj" -scheme "MoltenVK Package \(macOS only\)" -configuration "Release" -arch "${CMAKE_HOST_SYSTEM_PROCESSOR}"
		COMMAND ln -f "${CMAKE_CURRENT_SOURCE_DIR}/MoltenVK/MoltenVK/dylib/macOS/libMoltenVK.dylib" "${CMAKE_CURRENT_SOURCE_DIR}/MoltenVK/Build/Products/Release/dynamic/libMoltenVK.dylib"
		INSTALL_COMMAND ""
		BUILD_BYPRODUCTS "${CMAKE_CURRENT_SOURCE_DIR}/MoltenVK/Build/Products/Release/dynamic/libMoltenVK.dylib"
	)
endif()
