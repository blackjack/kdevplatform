macro(SVN_CREATE_TEST name)
    add_executable(${name} ${ARGN})
    ecm_mark_as_test(${name})
    target_link_libraries(${name}
                              Qt5::Test
                              Qt5::Gui
                              KDev::Tests
                              KDev::Util
                              KDev::Vcs
                         )
endmacro(SVN_CREATE_TEST)

svn_create_test(svnrecursiveadd svnrecursiveadd.cpp)

svn_create_test(svnimport svnimport.cpp)
