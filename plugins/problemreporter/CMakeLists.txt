
add_definitions( -DKDE_DEFAULT_DEBUG_AREA=9519 )


########### next target ###############

set(kdevproblemreporter_PART_SRCS
    problemreporterplugin.cpp
    problemtreeview.cpp
    problemmodel.cpp
    problemhighlighter.cpp
    watcheddocumentset.cpp
#modeltest.cpp
)

add_library(kdevproblemreporter MODULE ${kdevproblemreporter_PART_SRCS})
target_link_libraries(kdevproblemreporter KF5::TextEditor KF5::Parts KDev::Language KDev::Interfaces KDev::Util KDev::Project)

install(TARGETS kdevproblemreporter DESTINATION ${PLUGIN_INSTALL_DIR}/kdevplatform/${KDEV_PLUGIN_VERSION})


########### install files ###############

configure_file(kdevproblemreporter.desktop.cmake ${CMAKE_CURRENT_BINARY_DIR}/kdevproblemreporter.desktop)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/kdevproblemreporter.desktop DESTINATION ${SERVICES_INSTALL_DIR})
install(FILES kdevproblemreporter.rc DESTINATION ${KXMLGUI_INSTALL_DIR}/kdevproblemreporter)
