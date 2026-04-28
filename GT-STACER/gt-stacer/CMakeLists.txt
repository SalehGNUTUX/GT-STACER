set(APP_SOURCES
    main.cpp
    app.cpp
    Managers/app_manager.cpp
    Managers/setting_manager.cpp
    Managers/tool_manager.cpp
    Managers/info_manager.cpp
    Widgets/circular_gauge.cpp
    Widgets/loading_overlay.cpp
    Widgets/sidebar.cpp
    Dialogs/welcome_dialog.cpp
    Dialogs/edit_source_dialog.cpp
    Pages/Dashboard/dashboard_page.cpp
    Pages/Resources/resources_page.cpp
    Pages/Processes/processes_page.cpp
    Pages/Services/services_page.cpp
    Pages/StartupApps/startup_apps_page.cpp
    Pages/SystemCleaner/system_cleaner_page.cpp
    Pages/Uninstaller/uninstaller_page.cpp
    Pages/AptSourceManager/apt_source_page.cpp
    Pages/Settings/settings_page.cpp
    Pages/Helpers/helpers_page.cpp
)

set(APP_HEADERS
    app.h
    Managers/app_manager.h
    Managers/setting_manager.h
    Managers/tool_manager.h
    Managers/info_manager.h
    Widgets/circular_gauge.h
    Widgets/loading_overlay.h
    Widgets/sidebar.h
    Widgets/sidebar_icons.h
    Dialogs/welcome_dialog.h
    Dialogs/edit_source_dialog.h
    Pages/Dashboard/dashboard_page.h
    Pages/Resources/resources_page.h
    Pages/Processes/processes_page.h
    Pages/Services/services_page.h
    Pages/StartupApps/startup_apps_page.h
    Pages/SystemCleaner/system_cleaner_page.h
    Pages/Uninstaller/uninstaller_page.h
    Pages/AptSourceManager/apt_source_page.h
    Pages/Settings/settings_page.h
    Pages/Helpers/helpers_page.h
)

set(APP_UI_FILES
    Pages/Resources/resources_page.ui
    Pages/Processes/processes_page.ui
    Pages/Services/services_page.ui
    Pages/StartupApps/startup_apps_page.ui
    Pages/SystemCleaner/system_cleaner_page.ui
    Pages/Uninstaller/uninstaller_page.ui
    Pages/AptSourceManager/apt_source_page.ui
    Pages/Settings/settings_page.ui
    Pages/Helpers/helpers_page.ui
)

set_source_files_properties(${APP_UI_FILES} PROPERTIES AUTOUIC_OPTIONS "")

qt6_add_resources(APP_QRC_SOURCES static.qrc)

# ── Translations ──────────────────────────────────────────────────────────
set(TS_FILES
    ${CMAKE_SOURCE_DIR}/translations/gt-stacer_ar.ts
    ${CMAKE_SOURCE_DIR}/translations/gt-stacer_de.ts
    ${CMAKE_SOURCE_DIR}/translations/gt-stacer_fr.ts
    ${CMAKE_SOURCE_DIR}/translations/gt-stacer_hi.ts
    ${CMAKE_SOURCE_DIR}/translations/gt-stacer_it.ts
    ${CMAKE_SOURCE_DIR}/translations/gt-stacer_kn.ts
    ${CMAKE_SOURCE_DIR}/translations/gt-stacer_ml.ts
    ${CMAKE_SOURCE_DIR}/translations/gt-stacer_nl.ts
    ${CMAKE_SOURCE_DIR}/translations/gt-stacer_oc.ts
    ${CMAKE_SOURCE_DIR}/translations/gt-stacer_pl.ts
    ${CMAKE_SOURCE_DIR}/translations/gt-stacer_pt.ts
    ${CMAKE_SOURCE_DIR}/translations/gt-stacer_ru.ts
    ${CMAKE_SOURCE_DIR}/translations/gt-stacer_sv.ts
    ${CMAKE_SOURCE_DIR}/translations/gt-stacer_tr.ts
    ${CMAKE_SOURCE_DIR}/translations/gt-stacer_uk.ts
    ${CMAKE_SOURCE_DIR}/translations/gt-stacer_vi.ts
    ${CMAKE_SOURCE_DIR}/translations/gt-stacer_zh_CN.ts
    ${CMAKE_SOURCE_DIR}/translations/gt-stacer_zh_TW.ts
)

# Compile .ts to .qm using lrelease
find_program(LRELEASE_EXEC
    NAMES lrelease6 lrelease lrelease-qt6
    HINTS
        "${Qt6_DIR}/../../../bin"
        "$ENV{QTDIR}/bin"
        "/usr/lib/qt6/bin"
        "/usr/lib/x86_64-linux-gnu/qt6/bin"
        "/usr/lib/aarch64-linux-gnu/qt6/bin"
        "/usr/bin"
)
set(QM_DIR "${CMAKE_BINARY_DIR}/i18n")
if(LRELEASE_EXEC)
    file(MAKE_DIRECTORY "${QM_DIR}")
    set(QM_FILES "")
    foreach(TS_FILE ${TS_FILES})
        get_filename_component(QM_NAME ${TS_FILE} NAME_WE)
        set(QM_OUT "${QM_DIR}/${QM_NAME}.qm")
        add_custom_command(
            OUTPUT  "${QM_OUT}"
            COMMAND "${LRELEASE_EXEC}" "${TS_FILE}" -qm "${QM_OUT}"
            DEPENDS "${TS_FILE}"
            COMMENT "Compiling: ${QM_NAME}.qm"
            VERBATIM
        )
        list(APPEND QM_FILES "${QM_OUT}")
    endforeach()
    add_custom_target(gt-stacer-translations ALL DEPENDS ${QM_FILES})
    # Install QM files alongside the binary data
    install(FILES ${QM_FILES}
        DESTINATION "${CMAKE_INSTALL_DATADIR}/gt-stacer/translations"
    )
    message(STATUS "lrelease found: ${LRELEASE_EXEC} — translations will be compiled")
else()
    message(WARNING
        "lrelease not found — translations will not be compiled.\n"
        "  Debian/Ubuntu: sudo apt install qt6-l10n-tools\n"
        "  Fedora:        sudo dnf install qt6-linguist\n"
        "  Arch:          sudo pacman -S qt6-tools")
endif()

qt6_add_executable(gt-stacer
    ${APP_SOURCES}
    ${APP_HEADERS}
    ${APP_UI_FILES}
    ${APP_QRC_SOURCES}
)

if(LRELEASE_EXEC)
    add_dependencies(gt-stacer gt-stacer-translations)
    # Pass the QM output directory so the app can find translations at runtime
    target_compile_definitions(gt-stacer PRIVATE
        APP_TRANSLATIONS_BUILD_DIR="${QM_DIR}"
    )
endif()

target_include_directories(gt-stacer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/Pages
    ${CMAKE_CURRENT_SOURCE_DIR}/Managers
)

target_link_libraries(gt-stacer PRIVATE
    gt-stacer-core
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Charts
    Qt6::Svg
    Qt6::Concurrent
    Qt6::Network
)

target_compile_features(gt-stacer PRIVATE cxx_std_17)

# Set properties for Linux
set_target_properties(gt-stacer PROPERTIES
    OUTPUT_NAME "gt-stacer"
)

# Install rules
include(GNUInstallDirs)
install(TARGETS gt-stacer RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES ${CMAKE_SOURCE_DIR}/applications/gt-stacer.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES ${CMAKE_SOURCE_DIR}/icons/hicolor/256x256/apps/gt-stacer.png
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps)
install(FILES ${CMAKE_SOURCE_DIR}/icons/hicolor/128x128/apps/gt-stacer.png
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/128x128/apps)
install(FILES ${CMAKE_SOURCE_DIR}/icons/hicolor/64x64/apps/gt-stacer.png
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/64x64/apps)
install(FILES ${CMAKE_SOURCE_DIR}/icons/hicolor/48x48/apps/gt-stacer.png
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/48x48/apps)
