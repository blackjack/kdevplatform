project(vcschangesview)

set(vcschangesview_SRCS
    vcschangesview.cpp
    
    vcschangesviewplugin.cpp
)

add_library(kdevvcschangesviewplugin MODULE ${vcschangesview_SRCS})
target_link_libraries(kdevvcschangesviewplugin ${KDEVPLATFORM_INTERFACES_LIBRARIES} ${KDEVPLATFORM_PROJECT_LIBRARIES} ${KDEVPLATFORM_VCS_LIBRARIES})

install(TARGETS kdevvcschangesviewplugin DESTINATION ${PLUGIN_INSTALL_DIR}/kdevplatform/${KDEV_PLUGIN_VERSION})

configure_file(kdevvcschangesview.desktop.cmake ${CMAKE_CURRENT_BINARY_DIR}/kdevvcschangesview.desktop)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/kdevvcschangesview.desktop DESTINATION ${SERVICES_INSTALL_DIR})
