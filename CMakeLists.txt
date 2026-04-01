cmake_minimum_required(VERSION 3.16)

project(MediaDownloader VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Qt Configuration ---
# Enable Qt's automatic tools for MOC, RCC, and UIC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find the required Qt6 packages
find_package(Qt6 REQUIRED COMPONENTS Widgets Network Sql Core Concurrent)

# --- Source Files ---
# Use file(GLOB_RECURSE) to automatically find all source and header files.
# This is the key fix: it ensures CMAKE_AUTOMOC can find all Q_OBJECT macros.
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    "src/*.cpp"
    "src/*.h"
)

# The project currently contains two different YtDlpJsonExtractor
# implementations with the same class name. Keep the original top-level
# extractor that Aria2DownloadWorker and MainWindow use, and exclude the
# duplicate core variant from this target to avoid ODR/link conflicts.
list(FILTER SOURCES EXCLUDE REGEX ".*/src/core/YtDlpJsonExtractor\\.(cpp|h)$")

# Likewise, the target contains legacy top-level Aria2/ffmpeg pipeline classes
# plus newer core-side classes with the same names. The app currently uses the
# top-level variants, so exclude the duplicate core implementations from the
# target to keep AUTOMOC from generating conflicting meta-object code.
list(FILTER SOURCES EXCLUDE REGEX ".*/src/core/Aria2Daemon\\.(cpp|h)$")
list(FILTER SOURCES EXCLUDE REGEX ".*/src/core/FfmpegPostProcessor\\.(cpp|h)$")

# --- Include Directories ---
# This ensures that #include "utils/..." or "core/..." works regardless of
# which directory the source file is located in.
include_directories(
    src
)

# --- Executable Target ---
add_executable(MediaDownloader
    # Use WIN32 for GUI applications on Windows to avoid a console window
    # WIN32 # Temporarily disabled to allow console output for debugging

    # Application entry point
    main.cpp

    # All source and header files gathered by the glob
    ${SOURCES}

    # Explicitly include recently added files so link-time availability
    # does not depend on whether the generator re-ran after they were created.
    src/core/ProcessUtils.cpp
    src/ui/RuntimeSelectionDialog.cpp
    src/ui/FormatSelectionDialog.cpp
    src/ui/advanced_settings/BinariesPage.cpp

    # Qt resource file for icons, etc.
    src/ui/resources.qrc

    # Windows application icon and metadata
    MediaDownloader.rc
)

# --- Link Libraries ---
target_link_libraries(MediaDownloader PRIVATE
    Qt6::Widgets
    Qt6::Network
    Qt6::Sql
    Qt6::Core
    Qt6::Concurrent
)

# --- Deployment of Binaries and Resources ---
# This section ensures that all required external executables and data files
# are copied to the build output directory, so the application can find them at runtime.

# Define a helper function to simplify copying files
function(copy_to_output_dir target file)
    add_custom_command(
        TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${file}"
        "$<TARGET_FILE_DIR:${target}>"
        COMMENT "Copying ${file} to output directory"
    )
endfunction()

# Define paths to bundled resources (assuming a 'bin/windows' structure)
set(BIN_DIR_WIN ${CMAKE_CURRENT_SOURCE_DIR}/bin/windows)

# Copy all required binaries and data files
copy_to_output_dir(MediaDownloader "${BIN_DIR_WIN}/yt-dlp.exe")
copy_to_output_dir(MediaDownloader "${BIN_DIR_WIN}/ffmpeg.exe")
copy_to_output_dir(MediaDownloader "${BIN_DIR_WIN}/ffprobe.exe")
copy_to_output_dir(MediaDownloader "${BIN_DIR_WIN}/gallery-dl.exe")
copy_to_output_dir(MediaDownloader "${BIN_DIR_WIN}/aria2c.exe")
copy_to_output_dir(MediaDownloader "${BIN_DIR_WIN}/deno.exe")
copy_to_output_dir(MediaDownloader "${CMAKE_CURRENT_SOURCE_DIR}/extractors_yt-dlp.json")
copy_to_output_dir(MediaDownloader "${CMAKE_CURRENT_SOURCE_DIR}/extractors_gallery-dl.json")

# --- Windows Specific Deployment (Qt Plugins) ---
# This ensures that Qt plugins (for database, image formats, TLS, etc.)
# are available in the build directory, which is critical for both
# Debug and Release builds to function correctly.
if(WIN32)
    # Find windeployqt from Qt6::qmake location
    get_target_property(_qmake_executable Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(_qt_bin_dir "${_qmake_executable}" DIRECTORY)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt_bin_dir}")

    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET MediaDownloader POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
            --dir "$<TARGET_FILE_DIR:MediaDownloader>"
            --no-translations
            --no-compiler-runtime
            --no-opengl-sw
            "$<TARGET_FILE:MediaDownloader>"
            COMMENT "Running windeployqt to copy Qt dependencies..."
        )
    else()
        message(WARNING "windeployqt not found, Qt dependencies will not be copied automatically.")
    endif()
endif()
