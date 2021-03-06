project(git)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${git_SOURCE_DIR}/cmake)

add_definitions(-DKDE_DEFAULT_DEBUG_AREA=9525)

add_subdirectory(tests)
add_subdirectory(icons)

set(kdevgit_PART_SRCS
    stashmanagerdialog.cpp
    stashpatchsource.cpp
    gitmessagehighlighter.cpp
    gitclonejob.cpp
    gitplugin.cpp
    gitjob.cpp
    gitplugincheckinrepositoryjob.cpp
)

qt5_wrap_ui(kdevgit_PART_SRCS stashmanagerdialog.ui)
add_library(kdevgit MODULE ${kdevgit_PART_SRCS})

target_link_libraries(kdevgit
    ${KDEVPLATFORM_UTIL_LIBRARIES}
    ${KDEVPLATFORM_INTERFACES_LIBRARIES}
    ${KDEVPLATFORM_VCS_LIBRARIES}
    ${KDEVPLATFORM_PROJECT_LIBRARIES}
    KF5::SonnetUi
)

install(TARGETS kdevgit DESTINATION ${PLUGIN_INSTALL_DIR}/kdevplatform/${KDEV_PLUGIN_VERSION} )


########### install files ###############
configure_file(kdevgit.desktop.cmake ${CMAKE_CURRENT_BINARY_DIR}/kdevgit.desktop)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/kdevgit.desktop DESTINATION ${SERVICES_INSTALL_DIR} )
