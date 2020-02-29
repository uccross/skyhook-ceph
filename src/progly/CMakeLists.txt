# --------------------------------- #
# driver for copyfrom approach to object merging.
# can delete later.
add_executable(transform-copyfrom-merge transform-copyfrom-merge.cc ${CMAKE_SOURCE_DIR}/src/cls/tabular/cls_tabular_utils.cc )
target_link_libraries(transform-copyfrom-merge librados global ${CMAKE_DL_LIBS}
    ${Boost_PROGRAM_OPTIONS_LIBRARY} re2 arrow)
install(TARGETS transform-copyfrom-merge DESTINATION bin)

# --------------------------------- #
# driver for copyfrom approach to object merging.
# can delete later.
add_executable(basic-copyfrom-merge basic-copyfrom-merge.cc ${CMAKE_SOURCE_DIR}/src/cls/tabular/cls_tabular_utils.cc )
target_link_libraries(basic-copyfrom-merge librados global ${CMAKE_DL_LIBS}
    ${Boost_PROGRAM_OPTIONS_LIBRARY} re2 arrow)
install(TARGETS basic-copyfrom-merge DESTINATION bin)

# --------------------------------- #
# driver for copyfrom approach to object merging.
# can delete later.
add_executable(run-copyfrom-merge run-copyfrom-merge.cc ${CMAKE_SOURCE_DIR}/src/cls/tabular/cls_tabular_utils.cc )
target_link_libraries(run-copyfrom-merge librados global ${CMAKE_DL_LIBS}
    ${Boost_PROGRAM_OPTIONS_LIBRARY} re2 arrow)
install(TARGETS run-copyfrom-merge DESTINATION bin)

# --------------------------------- #
# driver for fbu integration deving.
# delete before pr.
# can delete later.
add_executable(kat-tests kat-tests.cc ${CMAKE_SOURCE_DIR}/src/cls/tabular/cls_tabular_utils.cc )
target_link_libraries(kat-tests librados global ${CMAKE_DL_LIBS}
    ${Boost_PROGRAM_OPTIONS_LIBRARY} re2 arrow)
install(TARGETS kat-tests DESTINATION bin)

# --------------------------------- #
# driver for baseline experiments for paper.
# can delete later.
add_executable(run-papertests run-papertests.cc )
target_link_libraries(run-papertests librados global ${CMAKE_DL_LIBS}
    ${Boost_PROGRAM_OPTIONS_LIBRARY} re2)
install(TARGETS run-papertests DESTINATION bin)

# --------------------------------- #
add_executable(run-query run-query.cc query.cc ${CMAKE_SOURCE_DIR}/src/cls/tabular/cls_tabular_utils.cc)
target_link_libraries(run-query librados global ${CMAKE_DL_LIBS}
    ${Boost_PROGRAM_OPTIONS_LIBRARY} re2 arrow)
install(TARGETS run-query DESTINATION bin)

install(PROGRAMS filtering.sh DESTINATION bin
    RENAME tabular-filtering.sh)

install(PROGRAMS convert-tpch-tables.py DESTINATION bin)
install(PROGRAMS rados-store-glob.sh DESTINATION bin)

set(UNITTEST_LIBS gmock_main gmock gtest ${CMAKE_THREAD_LIBS_INIT} ${CMAKE_DL_LIBS})
set(UNITTEST_CXX_FLAGS "-I${CMAKE_SOURCE_DIR}/src/googletest/googlemock/include -I${CMAKE_BINARY_DIR}/src/googletest/googlemock/include -I${CMAKE_SOURCE_DIR}/src/googletest/googletest/include -I${CMAKE_BINARY_DIR}/src/googletest/googletest/include -fno-strict-aliasing")
add_executable(ceph_test_skyhook_query test_query.cc query.cc ${CMAKE_SOURCE_DIR}/src/cls/tabular/cls_tabular_utils.cc)
set_target_properties(ceph_test_skyhook_query PROPERTIES COMPILE_FLAGS
  ${UNITTEST_CXX_FLAGS})
target_link_libraries(ceph_test_skyhook_query
  librados
  global
  ${EXTRALIBS}
  ${BLKID_LIBRARIES}
  ${CMAKE_DL_LIBS}
  radostest
  ${UNITTEST_LIBS}
  re2
  arrow
  )
include_directories(${CMAKE_SOURCE_DIR}/src/googletest/googlemock/include)
install(TARGETS ceph_test_skyhook_query DESTINATION bin)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-function")
