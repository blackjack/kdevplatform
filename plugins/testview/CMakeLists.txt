
# add_definitions( -DKDE_DEFAULT_DEBUG_AREA=95xx )

########### next target ###############

set(kdevtestview_PLUGIN_SRCS
    testview.cpp
    testviewplugin.cpp
    testviewdebug.cpp
)

add_library(kdevtestview MODULE ${kdevtestview_PLUGIN_SRCS})
target_link_libraries(kdevtestview KF5::ItemModels KDev::Interfaces KDev::Project KDev::Util KDev::Language)

install(TARGETS kdevtestview DESTINATION ${PLUGIN_INSTALL_DIR}/kdevplatform/${KDEV_PLUGIN_VERSION} )


########### install files ###############

configure_file(kdevtestview.desktop.cmake ${CMAKE_CURRENT_BINARY_DIR}/kdevtestview.desktop)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/kdevtestview.desktop DESTINATION ${SERVICES_INSTALL_DIR} )
install( FILES kdevtestview.rc DESTINATION ${KXMLGUI_INSTALL_DIR}/kdevtestview )
