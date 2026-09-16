add_library(gt-stacer-core STATIC
    Info/cpu_info.cpp
    Info/memory_info.cpp
    Info/disk_info.cpp
    Info/network_info.cpp
    Info/connection_info.cpp
    Info/process_info.cpp
    Info/system_info.cpp
    Info/gpu_info.cpp
    Info/temperature_info.cpp
    Info/battery_info.cpp
    Tools/service_tool.cpp
    Tools/package_tool.cpp
    Tools/startup_tool.cpp
    Tools/apt_source_tool.cpp
    Tools/notification_tool.cpp
    Tools/power_tool.cpp
    Tools/power_profile_tool.cpp
    Tools/battery_tool.cpp
    Tools/firewall_tool.cpp
    Tools/snapshot_tool.cpp
    Tools/backup_tool.cpp
    Tools/recovery_tool.cpp
    Tools/relief_tool.cpp
    Tools/store_addon_tool.cpp
    Tools/appimage_tool.cpp
    Utils/command_util.cpp
    Utils/file_util.cpp
    Utils/format_util.cpp
)

target_include_directories(gt-stacer-core
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(gt-stacer-core
    PUBLIC Qt6::Core Qt6::Network Qt6::Concurrent
)

target_compile_features(gt-stacer-core PUBLIC cxx_std_17)
