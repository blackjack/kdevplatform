add_definitions( -DKDE_DEFAULT_DEBUG_AREA=9504 )
add_subdirectory(examples)
add_subdirectory(tests)

########### next target ###############

set(sublime_LIB_SRCS
    area.cpp
    areaindex.cpp
    container.cpp
    controller.cpp
    document.cpp
    mainwindow.cpp
    mainwindow_p.cpp
    mainwindowoperator.cpp
    urldocument.cpp
    tooldocument.cpp
    view.cpp
    sublimedefs.cpp
    aggregatemodel.cpp
    holdupdates.cpp

    idealcontroller.cpp
    ideallayout.cpp
    idealtoolbutton.cpp
    idealdockwidget.cpp
    idealbuttonbarwidget.cpp
)

add_library(KDevPlatformSublime ${sublime_LIB_SRCS})
add_library(KDev::Sublime ALIAS KDevPlatformSublime)

generate_export_header(KDevPlatformSublime EXPORT_FILE_NAME sublimeexport.h)
target_link_libraries(KDevPlatformSublime
        LINK_PRIVATE KF5::KIOWidgets
        LINK_PUBLIC KF5::Parts KF5::KDELibs4Support)
set_target_properties(KDevPlatformSublime PROPERTIES VERSION ${KDEVPLATFORM_LIB_VERSION} SOVERSION ${KDEVPLATFORM_LIB_SOVERSION} EXPORT_NAME Sublime)
install(TARGETS KDevPlatformSublime EXPORT KDevPlatformTargets ${INSTALL_TARGETS_DEFAULT_ARGS} )


########### install files ###############

install(FILES
    area.h
    areaindex.h
    areawalkers.h
    container.h
    controller.h
    document.h
    mainwindow.h
    mainwindowoperator.h
    urldocument.h
    sublimedefs.h
    tooldocument.h
    view.h
    ${CMAKE_CURRENT_BINARY_DIR}/sublimeexport.h
    DESTINATION ${INCLUDE_INSTALL_DIR}/kdevplatform/sublime COMPONENT Devel)
