cmake_minimum_required(VERSION 3.16)
project(MediaDownloader)

set(CMAKE_CXX_STANDARD 20)

# Add the path to your Qt6 installation
set(CMAKE_PREFIX_PATH "C:/Qt/6.10.2/msvc2022_64")

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 COMPONENTS
        Core
        Gui
        Widgets
        Network
        Sql
        Concurrent
        REQUIRED)

add_executable(MediaDownloader
        main.cpp
        src/ui/MainWindow.cpp
        src/ui/MainWindow.h
        src/ui/StartTab.cpp
        src/ui/StartTab.h
        src/ui/ActiveDownloadsTab.cpp
        src/ui/ActiveDownloadsTab.h
        src/ui/DownloadItemWidget.cpp
        src/ui/DownloadItemWidget.h
        src/ui/AdvancedSettingsTab.cpp
        src/ui/AdvancedSettingsTab.h
        src/ui/SortingTab.cpp
        src/ui/SortingTab.h
        src/ui/SortingRuleDialog.cpp
        src/ui/SortingRuleDialog.h
        src/core/ConfigManager.cpp
        src/core/ConfigManager.h
        src/core/ArchiveManager.cpp
        src/core/ArchiveManager.h
        src/core/DownloadManager.cpp
        src/core/DownloadManager.h
        src/core/YtDlpWorker.cpp
        src/core/YtDlpWorker.h
        src/core/GalleryDlWorker.cpp
        src/core/GalleryDlWorker.h
        src/core/YtDlpArgsBuilder.cpp
        src/core/YtDlpArgsBuilder.h
        src/core/GalleryDlArgsBuilder.cpp
        src/core/GalleryDlArgsBuilder.h
        src/core/PlaylistExpander.cpp
        src/core/PlaylistExpander.h
        src/core/SortingManager.cpp
        src/core/SortingManager.h
        src/core/AppUpdater.cpp
        src/core/AppUpdater.h
        src/core/YtDlpUpdater.cpp
        src/core/YtDlpUpdater.h
        src/core/GalleryDlUpdater.cpp
        src/core/GalleryDlUpdater.h
        src/core/MetadataEmbedder.cpp
        src/core/MetadataEmbedder.h
        src/core/Logger.cpp
        src/core/Logger.h
        src/core/StartupWorker.cpp
        src/core/StartupWorker.h
        src/core/UrlValidator.cpp
        src/core/UrlValidator.h
        src/core/Version.cpp
        src/core/Version.h
        src/ui/ToggleSwitch.cpp
        src/ui/ToggleSwitch.h
        src/utils/StringUtils.cpp
        src/utils/StringUtils.h
        src/utils/BrowserUtils.cpp
        src/utils/BrowserUtils.h
        src/utils/LogManager.cpp
        src/utils/LogManager.h
        src/utils/ExtractorJsonParser.cpp
        src/utils/ExtractorJsonParser.h
        src/ui/resources.qrc
        MediaDownloader.rc # Add the resource file here
)

target_link_libraries(MediaDownloader
        Qt::Core
        Qt::Gui
        Qt::Widgets
        Qt::Network
        Qt::Sql
        Qt::Concurrent
)

# Explicitly ensure Qt's autogen steps are enabled for this target.
set_target_properties(MediaDownloader PROPERTIES
        AUTOMOC ON
        AUTORCC ON
        AUTOUIC OFF # Set AUTOUIC to OFF
)

target_include_directories(MediaDownloader PRIVATE
        src
        src/ui
        src/core
        src/utils
)

# Copy bundled binaries to the output directory
set(WINDOWS_BIN_DIR "${CMAKE_SOURCE_DIR}/bin/windows")

# This single custom command will execute all necessary copy operations after the build.
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy "${WINDOWS_BIN_DIR}/yt-dlp.exe" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/"
    COMMAND ${CMAKE_COMMAND} -E copy "${WINDOWS_BIN_DIR}/ffmpeg-8.0.1-essentials_build/bin/ffmpeg.exe" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/"
    COMMAND ${CMAKE_COMMAND} -E copy "${WINDOWS_BIN_DIR}/ffmpeg-8.0.1-essentials_build/bin/ffprobe.exe" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/"
    COMMAND ${CMAKE_COMMAND} -E copy "${WINDOWS_BIN_DIR}/gallery-dl.exe" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/"
    COMMAND ${CMAKE_COMMAND} -E copy "${WINDOWS_BIN_DIR}/aria2-1.37.0-win-64bit-build1/aria2c.exe" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/"
    COMMAND ${CMAKE_COMMAND} -E copy "${WINDOWS_BIN_DIR}/deno.exe" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/extractors_yt-dlp.json" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/extractors_gallery-dl.json" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/"
    COMMENT "Copying external binaries..."
)

# Deploy Qt dependencies on Windows
if (WIN32 AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    get_filename_component(QT_INSTALL_PATH "${Qt6_DIR}/../../../" ABSOLUTE)
    function(copy_qt_runtime_if_exists SOURCE_FILE TARGET_DIR)
        if (EXISTS "${SOURCE_FILE}")
            add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E make_directory "${TARGET_DIR}")
            add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${SOURCE_FILE}"
                    "${TARGET_DIR}")
        endif ()
    endfunction()

    # Always deploy both release and debug plugin variants when available.
    # This avoids multi-config suffix issues (CMAKE_BUILD_TYPE can be empty on MSVC).
    copy_qt_runtime_if_exists("${QT_INSTALL_PATH}/plugins/platforms/qwindows.dll" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins/platforms/")
    copy_qt_runtime_if_exists("${QT_INSTALL_PATH}/plugins/platforms/qwindowsd.dll" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins/platforms/")
    copy_qt_runtime_if_exists("${QT_INSTALL_PATH}/plugins/sqldrivers/qsqlite.dll" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins/sqldrivers/")
    copy_qt_runtime_if_exists("${QT_INSTALL_PATH}/plugins/sqldrivers/qsqlited.dll" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins/sqldrivers/")
    copy_qt_runtime_if_exists("${QT_INSTALL_PATH}/plugins/tls/qschannelbackend.dll" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins/tls/")
    copy_qt_runtime_if_exists("${QT_INSTALL_PATH}/plugins/tls/qschannelbackendd.dll" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins/tls/")
    copy_qt_runtime_if_exists("${QT_INSTALL_PATH}/plugins/tls/qopensslbackend.dll" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins/tls/")
    copy_qt_runtime_if_exists("${QT_INSTALL_PATH}/plugins/tls/qopensslbackendd.dll" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins/tls/")

    # Explicitly create the imageformats directory
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins/imageformats/")

    foreach (IMAGE_PLUGIN qico qjpeg qpng qwebp)
        set(IMAGE_PLUGIN_RELEASE_PATH "${QT_INSTALL_PATH}/plugins/imageformats/${IMAGE_PLUGIN}.dll")
        if (EXISTS "${IMAGE_PLUGIN_RELEASE_PATH}")
            message(STATUS "Found ${IMAGE_PLUGIN}.dll at: ${IMAGE_PLUGIN_RELEASE_PATH}")
            copy_qt_runtime_if_exists("${IMAGE_PLUGIN_RELEASE_PATH}" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins/imageformats/")
        else ()
            message(WARNING "${IMAGE_PLUGIN}.dll not found at: ${IMAGE_PLUGIN_RELEASE_PATH}. Related image formats may not display correctly.")
        endif ()

        set(IMAGE_PLUGIN_DEBUG_PATH "${QT_INSTALL_PATH}/plugins/imageformats/${IMAGE_PLUGIN}d.dll")
        if (EXISTS "${IMAGE_PLUGIN_DEBUG_PATH}")
            message(STATUS "Found ${IMAGE_PLUGIN}d.dll at: ${IMAGE_PLUGIN_DEBUG_PATH}")
            copy_qt_runtime_if_exists("${IMAGE_PLUGIN_DEBUG_PATH}" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins/imageformats/")
        else ()
            message(WARNING "${IMAGE_PLUGIN}d.dll not found at: ${IMAGE_PLUGIN_DEBUG_PATH}. Related image formats may not display correctly in debug builds.")
        endif ()
    endforeach ()

    foreach (QT_LIB Core Gui Widgets Network Sql Concurrent)
        copy_qt_runtime_if_exists("${QT_INSTALL_PATH}/bin/Qt6${QT_LIB}.dll" "$<TARGET_FILE_DIR:${PROJECT_NAME}>")
        copy_qt_runtime_if_exists("${QT_INSTALL_PATH}/bin/Qt6${QT_LIB}d.dll" "$<TARGET_FILE_DIR:${PROJECT_NAME}>")
    endforeach ()

    # Create qt.conf to explicitly tell Qt where to find plugins
    file(GENERATE OUTPUT "$<TARGET_FILE_DIR:${PROJECT_NAME}>/qt.conf" CONTENT "[Paths]\nPlugins=plugins\n")
endif ()
