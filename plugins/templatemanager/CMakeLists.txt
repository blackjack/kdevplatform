
set(kdevtemplatemanager_config_SRCS
    templateconfig.cpp
    templatepage.cpp 
)

qt5_wrap_ui(kdevtemplatemanager_config_SRCS templateconfig.ui templatepage.ui)

add_library( kdevtemplatemanager_config MODULE ${kdevtemplatemanager_config_SRCS} )

target_link_libraries( kdevtemplatemanager_config KF5::ConfigWidgets KF5::Service KF5::KIOWidgets KF5::NewStuff KF5::Archive KDev::Interfaces KDev::Language)

install(TARGETS kdevtemplatemanager_config DESTINATION ${PLUGIN_INSTALL_DIR}/kdevplatform/${KDEV_PLUGIN_VERSION})
configure_file(kdevtemplatemanager_config.desktop.cmake ${CMAKE_CURRENT_BINARY_DIR}/kdevtemplatemanager_config.desktop)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/kdevtemplatemanager_config.desktop DESTINATION ${SERVICES_INSTALL_DIR})

