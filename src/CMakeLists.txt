include(GetGitRevisionDescription)

enable_language(C ASM)
include(GNUInstallDirs)
# for erasure and compressor plugins
set(CEPH_INSTALL_PKGLIBDIR ${CMAKE_INSTALL_LIBDIR}/${PROJECT_NAME})
set(CEPH_INSTALL_FULL_PKGLIBDIR ${CMAKE_INSTALL_FULL_LIBDIR}/${PROJECT_NAME})
# for mgr plugins
set(CEPH_INSTALL_DATADIR ${CMAKE_INSTALL_FULL_DATADIR}/${PROJECT_NAME})
# so libceph-common can be found
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
if(NOT CMAKE_INSTALL_RPATH)
  set(CMAKE_INSTALL_RPATH "${CEPH_INSTALL_FULL_PKGLIBDIR}")
endif()

# to be compatible with configure_files shared with autoconfig
set(bindir ${CMAKE_INSTALL_FULL_BINDIR})
set(sbindir ${CMAKE_INSTALL_FULL_SBINDIR})
set(libdir ${CMAKE_INSTALL_FULL_LIBDIR})
set(sysconfdir ${CMAKE_INSTALL_FULL_SYSCONFDIR})
set(libexecdir ${CMAKE_INSTALL_FULL_LIBEXECDIR})
set(pkgdatadir ${CMAKE_INSTALL_FULL_DATADIR})
set(datadir ${CEPH_INSTALL_DATADIR})
set(prefix ${CMAKE_INSTALL_PREFIX})

add_definitions("-DCEPH_LIBDIR=\"${CMAKE_INSTALL_FULL_LIBDIR}\"")
add_definitions("-DCEPH_PKGLIBDIR=\"${CMAKE_INSTALL_FULL_PKGLIBDIR}\"")
add_definitions("-DHAVE_CONFIG_H -D__CEPH__ -D_REENTRANT -D_THREAD_SAFE -D__STDC_FORMAT_MACROS")
add_definitions("-D_FILE_OFFSET_BITS=64")
if(LINUX)
  add_definitions("-D_GNU_SOURCE")
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wtype-limits -Wignored-qualifiers -Winit-self")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wpointer-arith -Werror=format-security -fno-strict-aliasing -fsigned-char")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ftemplate-depth-1024")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wnon-virtual-dtor")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unknown-pragmas")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-ignored-qualifiers")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unknown-pragmas")
if(CMAKE_CXX_COMPILER_ID STREQUAL GNU)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -rdynamic")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wstrict-null-sentinel -Woverloaded-virtual")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-new-ttp-matching")
  # cmake does not add '-pie' for executables even if
  # CMAKE_POSITION_INDEPENDENT_CODE is TRUE.
  if(EXE_LINKER_USE_PIE)
    if (NOT WITH_OSD_INSTRUMENT_FUNCTIONS AND NOT HAVE_SEASTAR)
      set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pie")
    endif()
  endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL Clang)
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_EXPORTS_C_FLAG}")
  set(CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -rdynamic -export-dynamic ${CMAKE_EXE_EXPORTS_C_FLAG}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-inconsistent-missing-override -Wno-mismatched-tags")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-private-field")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-address-of-packed-member")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-function")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-local-typedef")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-varargs")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-gnu-designator")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-missing-braces -Wno-parentheses -Wno-deprecated-register")
  if(APPLE)
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -undefined dynamic_lookup")
  endif()
endif(CMAKE_CXX_COMPILER_ID STREQUAL GNU)
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} ${CMAKE_CXX_FLAGS}")

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "Default BUILD_TYPE is RelWithDebInfo, other options are: Debug, Release, and MinSizeRel." FORCE)
endif()

if(WITH_CEPH_DEBUG_MUTEX OR CMAKE_BUILD_TYPE STREQUAL Debug)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCEPH_DEBUG_MUTEX")
endif()

include(CheckCCompilerFlag)
if(CMAKE_CXX_COMPILER_ID STREQUAL GNU)
  CHECK_C_COMPILER_FLAG("-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2" HAS_FORTIFY_SOURCE)
  if(NOT CMAKE_BUILD_TYPE STREQUAL Debug)
    if(HAS_FORTIFY_SOURCE)
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2")
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2")
    endif()
  endif()
  CHECK_C_COMPILER_FLAG(-fstack-protector-strong HAS_STACK_PROTECT)
  if (HAS_STACK_PROTECT)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fstack-protector-strong")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fstack-protector-strong")
  endif()
endif(CMAKE_CXX_COMPILER_ID STREQUAL GNU)

include(SIMDExt)
if(HAVE_INTEL)
  set(CMAKE_ASM_COMPILER  ${PROJECT_SOURCE_DIR}/src/yasm-wrapper)
  if(APPLE)
    set(object_format "macho64")
  else()
    set(object_format "elf64")
  endif()
  set(CMAKE_ASM_FLAGS "-f ${object_format}")
  include(CheckYasm)
  check_yasm_support(${object_format}
    HAVE_GOOD_YASM_ELF64
    HAVE_BETTER_YASM_ELF64)
endif()

execute_process(
  COMMAND yasm -f elf64 ${CMAKE_SOURCE_DIR}/src/common/crc32c_intel_fast_asm.s -o /dev/null
  RESULT_VARIABLE no_yasm
  OUTPUT_QUIET)
if(no_yasm)
  message(STATUS " we do not have a modern/working yasm")
else(no_yasm)
  message(STATUS " we have a modern and working yasm")
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "amd64|x86_64")
    message(STATUS " we are x84_64")
    set(save_quiet ${CMAKE_REQUIRED_QUIET})
    set(CMAKE_REQUIRED_QUIET true)
    include(CheckCXXSourceCompiles)
    check_cxx_source_compiles("
      #if defined(__x86_64__) && defined(__ILP32__)
      #error x32
      #endif
      int main() {}
      " not_arch_x32)
    set(CMAKE_REQUIRED_QUIET ${save_quiet})
    if(not_arch_x32)
      message(STATUS " we are not x32")
      set(HAVE_GOOD_YASM_ELF64 1)
      execute_process(COMMAND yasm -f elf64 -i
        ${CMAKE_SOURCE_DIR}/src/isa-l/include/
        ${CMAKE_SOURCE_DIR}/src/isa-l/erasure_code/gf_vect_dot_prod_avx2.asm
        -o /dev/null
        RESULT_VARIABLE rc
        OUTPUT_QUIET)
      if(NOT rc)
        set(HAVE_BETTER_YASM_ELF64 1)
        message(STATUS " yasm can also build the isa-l stuff")
      endif(NOT rc)
    else(not_arch_x32)
      message(STATUS " we are x32; no yasm for you")
    endif(not_arch_x32)
  else(CMAKE_SYSTEM_PROCESSOR MATCHES "amd64|x86_64")
    message(STATUS " we are not x86_64 && !x32")
  endif(CMAKE_SYSTEM_PROCESSOR MATCHES "amd64|x86_64")
endif(no_yasm)

# require c++17
if(CMAKE_VERSION VERSION_LESS "3.8")
  include(CheckCXXCompilerFlag)
  CHECK_CXX_COMPILER_FLAG("-std=c++17" COMPILER_SUPPORTS_CXX17)
  if(NOT COMPILER_SUPPORTS_CXX17)
    message(FATAL_ERROR
      "The compiler ${CMAKE_CXX_COMPILER} has no C++17 support.")
  endif()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
  include(CheckCCompilerFlag)
  CHECK_C_COMPILER_FLAG("-std=gnu99" COMPILER_SUPPORTS_GNU99)
  if(NOT COMPILER_SUPPORTS_GNU99)
    message(FATAL_ERROR
      "The compiler ${CMAKE_C_COMPILER} has no GNU C99 support.")
  endif()
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99")
else()
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_EXTENSIONS OFF)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CMAKE_C_STANDARD 99)
  # we use `asm()` to inline assembly, so enable the GNU extension
  set(CMAKE_C_EXTENSIONS ON)
  set(C_STANDARD_REQUIRED ON)
endif()

include(CheckCXXSourceCompiles)
CHECK_CXX_SOURCE_COMPILES("
#include <map>
using Map = std::map<int, int>;
int main() {
  Map m;
  m.merge(Map{});
}
" HAVE_STDLIB_MAP_SPLICING)

## Handle diagnostics color if compiler supports them.
CHECK_C_COMPILER_FLAG("-fdiagnostics-color=always"
  COMPILER_SUPPORTS_DIAGNOSTICS_COLOR)

set(DIAGNOSTICS_COLOR "auto"
  CACHE STRING "Used if the C/C++ compiler supports the -fdiagnostics-color option. May have one of three values -- 'auto' (default), 'always', or 'never'. If set to 'always' and the compiler supports the option, 'make [...] | less -R' will make visible diagnostics colorization of compiler output.")

if(COMPILER_SUPPORTS_DIAGNOSTICS_COLOR)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fdiagnostics-color=${DIAGNOSTICS_COLOR}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdiagnostics-color=${DIAGNOSTICS_COLOR}")
endif()

set(EXTRALIBS ${CMAKE_DL_LIBS})
if(NOT APPLE)
  list(APPEND EXTRALIBS rt)
endif()
if(LINUX OR APPLE)
  set(LIB_RESOLV resolv)
  list(APPEND EXTRALIBS ${LIB_RESOLV})
endif()

if(${ENABLE_COVERAGE})
  find_program(HAVE_GCOV gcov)
  if(NOT HAVE_GCOV)
    message(FATAL_ERROR "Coverage Enabled but gcov Not Found")
  endif(NOT HAVE_GCOV)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage -O0")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-arcs -ftest-coverage")
  list(APPEND EXTRALIBS gcov)
endif(${ENABLE_COVERAGE})

include_directories(${NSS_INCLUDE_DIR} ${NSPR_INCLUDE_DIR})

set(GCOV_PREFIX_STRIP 4)

# the src/.git_version file may be written out by make-dist; otherwise
# we pull the git version from .git
option(ENABLE_GIT_VERSION "build Ceph with git version string" ON)
if(${ENABLE_GIT_VERSION})
  get_git_head_revision(GIT_REFSPEC CEPH_GIT_VER)
  git_describe(CEPH_GIT_NICE_VER --always)
  #if building from a source tarball via make-dist
  if(${CEPH_GIT_VER} STREQUAL "GITDIR-NOTFOUND")
    message(STATUS "Ceph/.git directory not found, parsing ${CMAKE_CURRENT_SOURCE_DIR}/.git_version for CEPH_GIT_VER and CEPH_GIT_NICE_VER")
    file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/.git_version CEPH_GIT_SHA_AND_TAG)
    list(GET CEPH_GIT_SHA_AND_TAG 0 CEPH_GIT_VER)
    list(GET CEPH_GIT_SHA_AND_TAG 1 CEPH_GIT_NICE_VER)
  endif(${CEPH_GIT_VER} STREQUAL "GITDIR-NOTFOUND")
  # remove 'v' prefix from raw git version
  string(SUBSTRING ${CEPH_GIT_NICE_VER} 1 -1 CEPH_GIT_NICE_VER)
else(${ENABLE_GIT_VERSION})
  set(CEPH_GIT_VER "no_version")
  set(CEPH_GIT_NICE_VER "Development")
endif(${ENABLE_GIT_VERSION})

# the src/ceph_release file is 3 lines,
#   <release number, e.g. '12' for luminous>
#   <release name, e.g. 'luminous'>
#   <release type: 'dev' for x.0.z, 'rc' or x.1.z, or 'stable' or x.2.z>
# note that the release name is semi-redundant and must match CEPH_RELEASE_*
# definitions in include/rados.h and common/ceph_strings.c.
file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/ceph_release CEPH_RELEASE_FILE)
list(GET CEPH_RELEASE_FILE 0 CEPH_RELEASE)
list(GET CEPH_RELEASE_FILE 1 CEPH_RELEASE_NAME)
list(GET CEPH_RELEASE_FILE 2 CEPH_RELEASE_TYPE)

option(WITH_OCF "build OCF-compliant cluster resource agent" OFF)
if(WITH_OCF)
  add_subdirectory(ocf)
endif()

option(WITH_CEPHFS_JAVA "build libcephfs Java bindings" OFF)
if(WITH_CEPHFS_JAVA)
  add_subdirectory(java)
endif()

# Python stuff
option(WITH_PYTHON2 "build python2 bindings" ON)
if(WITH_PYTHON2)
  find_package(PythonInterp 2 REQUIRED)
  find_package(PythonLibs ${PYTHON_VERSION_STRING} EXACT REQUIRED)
endif()

set(WITH_PYTHON3 "OFF" CACHE STRING "build python3 bindings with specified python3 version")
if(WITH_PYTHON3)
  if(WITH_PYTHON3 MATCHES "^(1|ON|YES|TRUE|Y)$")
    set(WITH_PYTHON3 "3")
  endif()
  find_package(Python3Interp ${WITH_PYTHON3} REQUIRED)
  find_package(Python3Libs ${PYTHON3_VERSION_STRING} EXACT REQUIRED)
endif()

# the major version of the python bindings as a dependency of other
# targets
if(WITH_PYTHON2)
  set(PY_BINDING_INFIX "")
else()
  set(PY_BINDING_INFIX 3)
endif()

if(HAVE_XIO)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -I${XIO_INCLUDE_DIR}")
  list(APPEND EXTRALIBS ${XIO_LIBRARY} pthread)
endif(HAVE_XIO)

if(HAVE_RDMA)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -I${RDMA_INCLUDE_DIR}")
  list(APPEND EXTRALIBS ${RDMA_LIBRARIES} pthread rt)
endif(HAVE_RDMA)

if(HAVE_DPDK)
  set(CMAKE_CXX_FLAGS "-march=native ${CMAKE_CXX_FLAGS} -I${DPDK_INCLUDE_DIR}")
  list(APPEND EXTRALIBS ${DPDK_LIBRARY})
  if(NOT USE_CRYPTOPP)
    message(FATAL_ERROR "CRYPTOPP must be supported when enable DPDK.")
  endif(NOT USE_CRYPTOPP)
  list(APPEND EXTRALIBS ${DPDK_LIBRARIES})
endif(HAVE_DPDK)

# sort out which allocator to use
if(ALLOCATOR STREQUAL "tcmalloc")
  set(ALLOC_LIBS gperftools::tcmalloc)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-builtin-malloc -fno-builtin-calloc -fno-builtin-realloc -fno-builtin-free")
  set(TCMALLOC_srcs perfglue/heap_profiler.cc)
elseif(ALLOCATOR STREQUAL "tcmalloc_minimal")
  set(ALLOC_LIBS gperftools::tcmalloc_minimal)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-builtin-malloc -fno-builtin-calloc -fno-builtin-realloc -fno-builtin-free")
  set(TCMALLOC_srcs perfglue/disabled_heap_profiler.cc)
elseif(ALLOCATOR STREQUAL "jemalloc")
  set(ALLOC_LIBS JeMalloc::JeMalloc)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-builtin-malloc -fno-builtin-calloc -fno-builtin-realloc -fno-builtin-free")
  set(TCMALLOC_srcs perfglue/disabled_heap_profiler.cc)
elseif(ALLOCATOR STREQUAL "libc")
  set(TCMALLOC_srcs perfglue/disabled_heap_profiler.cc)
endif()

# tcmalloc heap profiler
set(heap_profiler_files ${TCMALLOC_srcs})
add_library(heap_profiler_objs OBJECT ${heap_profiler_files})

if (WITH_BLKIN)
  add_subdirectory(blkin/blkin-lib)
endif(WITH_BLKIN)

# Common infrastructure
configure_file(
  ${CMAKE_SOURCE_DIR}/src/ceph_ver.h.in.cmake
  ${CMAKE_BINARY_DIR}/src/include/ceph_ver.h
  @ONLY)

set(auth_files
  auth/AuthAuthorizeHandler.cc
  auth/AuthClientHandler.cc
  auth/AuthSessionHandler.cc
  auth/AuthMethodList.cc
  auth/cephx/CephxAuthorizeHandler.cc
  auth/cephx/CephxClientHandler.cc
  auth/cephx/CephxProtocol.cc
  auth/cephx/CephxSessionHandler.cc
  auth/none/AuthNoneAuthorizeHandler.cc
  auth/unknown/AuthUnknownAuthorizeHandler.cc
  auth/Crypto.cc
  auth/KeyRing.cc
  auth/RotatingKeyRing.cc)

set(mds_files)
list(APPEND mds_files
  mds/MDSMap.cc
  mds/FSMap.cc
  mds/FSMapUser.cc
  mds/inode_backtrace.cc
  mds/mdstypes.cc
  mds/flock.cc)

set(crush_srcs
  crush/builder.c
  crush/mapper.c
  crush/crush.c
  crush/hash.c
  crush/CrushWrapper.cc
  crush/CrushCompiler.cc
  crush/CrushTester.cc
  crush/CrushLocation.cc)

add_library(crush_objs OBJECT ${crush_srcs})

add_subdirectory(json_spirit)

include_directories(SYSTEM "${CMAKE_SOURCE_DIR}/src/xxHash")
include_directories(SYSTEM "${CMAKE_SOURCE_DIR}/src/rapidjson/include")

if(WITH_SEASTAR)
  find_package(fmt 5.2.1 QUIET)
  if(NOT fmt_FOUND)
    message(STATUS "Could not find fmt, will build it")
    add_subdirectory(seastar/fmt)
  endif()
  find_package(c-ares 1.13.0 QUIET)
  if(NOT c-ares_FOUND)
    message(STATUS "Could not find c-ares, will build it")
    include(Buildc-ares)
    build_c_ares()
  endif()
  macro(find_package name)
    if(NOT TARGET ${name})
      _find_package(${ARGV})
    endif()
  endmacro ()
  set(Seastar_HWLOC OFF CACHE BOOL "" FORCE)
  set(Seastar_STD_OPTIONAL_VARIANT_STRINGVIEW ON CACHE BOOL "" FORCE)
  set(Seastar_CXX_FLAGS "-Wno-sign-compare;-Wno-attributes;-Wno-pessimizing-move;-Wno-address-of-packed-member" CACHE STRING "" FORCE)
  add_subdirectory(seastar)
  # create the directory so cmake won't complain when looking at the imported
  # target: Seastar exports this directory created at build-time
  file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/seastar/gen/include")
  add_subdirectory(crimson)
endif()

set(libcommon_files
  ${CMAKE_BINARY_DIR}/src/include/ceph_ver.h
  ceph_ver.c
  common/AsyncOpTracker.cc
  common/DecayCounter.cc
  common/LogClient.cc
  common/LogEntry.cc
  common/PrebufferedStreambuf.cc
  common/BackTrace.cc
  common/perf_counters.cc
  common/perf_histogram.cc
  common/mutex_debug.cc
  common/Mutex.cc
  common/OutputDataSocket.cc
  common/admin_socket.cc
  common/admin_socket_client.cc
  common/bloom_filter.cc
  common/Readahead.cc
  common/cmdparse.cc
  common/escape.c
  common/url_escape.cc
  common/io_priority.cc
  common/Clock.cc
  common/ceph_time.cc
  common/mempool.cc
  common/Throttle.cc
  common/Timer.cc
  common/Finisher.cc
  common/environment.cc
  common/sctp_crc32.c
  common/crc32c.cc
  common/crc32c_intel_baseline.c
  xxHash/xxhash.c
  common/assert.cc
  common/run_cmd.cc
  common/WorkQueue.cc
  common/ConfUtils.cc
  common/MemoryModel.cc
  common/fd.cc
  common/xattr.c
  common/str_list.cc
  common/str_map.cc
  common/snap_types.cc
  common/errno.cc
  common/TrackedOp.cc
  common/SloppyCRCMap.cc
  common/types.cc
  common/iso_8601.cc
  log/Log.cc
  log/SubsystemMap.cc
  mon/MonCap.cc
  mon/MonClient.cc
  mon/MonMap.cc
  mon/MonSub.cc
  mgr/MgrClient.cc
  msg/simple/Accepter.cc
  msg/DispatchQueue.cc
  msg/Message.cc
  mon/PGMap.cc
  mgr/ServiceMap.cc
  osd/ECMsgTypes.cc
  osd/HitSet.cc
  common/RefCountedObj.cc
  msg/Messenger.cc
  msg/simple/Pipe.cc
  msg/simple/PipeConnection.cc
  msg/simple/SimpleMessenger.cc
  msg/async/AsyncConnection.cc
  msg/async/AsyncMessenger.cc
  msg/async/Event.cc
  msg/async/EventSelect.cc
  msg/async/Stack.cc
  msg/async/PosixStack.cc
  msg/async/net_handler.cc
  msg/QueueStrategy.cc
  ${xio_common_srcs}
  ${async_rdma_common_srcs}
  ${dpdk_common_srcs}
  msg/msg_types.cc
  common/reverse.c
  common/hobject.cc
  osd/OSDMap.cc
  osd/OSDMapMapping.cc
  common/histogram.cc
  osd/osd_types.cc
  osd/PGPeeringEvent.cc
  osd/OpRequest.cc
  common/blkdev.cc
  common/common_init.cc
  common/pipe.c
  common/ceph_argparse.cc
  common/ceph_context.cc
  common/code_environment.cc
  common/dout.cc
  common/signal.cc
  common/Thread.cc
  common/Formatter.cc
  common/HTMLFormatter.cc
  common/HeartbeatMap.cc
  common/PluginRegistry.cc
  common/ceph_fs.cc
  common/ceph_hash.cc
  common/ceph_strings.cc
  common/ceph_frag.cc
  common/options.cc
  common/config.cc
  common/utf8.c
  common/mime.c
  common/strtol.cc
  common/page.cc
  common/lockdep.cc
  common/version.cc
  common/hex.cc
  common/entity_name.cc
  common/ceph_crypto.cc
  common/ceph_crypto_cms.cc
  common/ceph_json.cc
  common/ipaddr.cc
  common/pick_address.cc
  common/address_helper.cc
  common/linux_version.c
  common/TracepointProvider.cc
  common/Cycles.cc
  common/scrub_types.cc
  common/bit_str.cc
  osdc/Striper.cc
  osdc/Objecter.cc
  librbd/Features.cc
  ${mds_files})
set_source_files_properties(ceph_ver.c
  APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_BINARY_DIR}/src/include/ceph_ver.h)
add_library(common-objs OBJECT ${libcommon_files})

CHECK_C_COMPILER_FLAG("-fvar-tracking-assignments" HAS_VTA)
add_subdirectory(auth)
add_subdirectory(common)
add_subdirectory(crush)
add_subdirectory(msg)
add_subdirectory(arch)

if(FREEBSD)
  list(APPEND libcommon_files common/freebsd_errno.cc)
elseif(DARWIN)
  list(APPEND libcommon_files common/darwin_errno.cc)
elseif(SUN)
  list(APPEND libcommon_files common/solaris_errno.cc)
elseif(AIX)
  list(APPEND libcommon_files common/aix_errno.cc)
endif()

if(HAVE_ARM)
  list(APPEND libcommon_files arch/arm.c)
elseif(HAVE_INTEL)
  list(APPEND libcommon_files arch/intel.c)
elseif(HAVE_POWER8)
  list(APPEND libcommon_files arch/ppc.c)
endif()

if(HAVE_INTEL)
  list(APPEND libcommon_files
    common/crc32c_intel_fast.c)
  if(HAVE_GOOD_YASM_ELF64)
    list(APPEND libcommon_files
      common/crc32c_intel_fast_asm.s
      common/crc32c_intel_fast_zero_asm.s)
  endif(HAVE_GOOD_YASM_ELF64)
elseif(HAVE_POWER8)
  list(APPEND libcommon_files
    common/crc32c_ppc.c
    common/crc32c_ppc_asm.S
    common/crc32c_ppc_fast_zero_asm.S)
endif(HAVE_INTEL)

if(LINUX)
  list(APPEND libcommon_files msg/async/EventEpoll.cc)
  message(STATUS " Using EventEpoll for events.")
elseif(FREEBSD OR APPLE)
  list(APPEND libcommon_files msg/async/EventKqueue.cc)
  message(STATUS " Using EventKqueue for events.")
endif(LINUX)

if(WITH_LTTNG AND WITH_EVENTTRACE)
  message(STATUS " Using EventTrace class.")
  add_definitions("-DWITH_EVENTTRACE")
  list(APPEND libcommon_files common/EventTrace.cc)
endif()

set(common_mountcephfs_files
  common/armor.c
  common/safe_io.c
  common/module.c
  common/addr_parsing.c)
add_library(common_mountcephfs_objs OBJECT
  ${common_mountcephfs_files})

if(WITH_PROFILER)
  list(APPEND libcommon_files
    perfglue/cpu_profiler.cc)
else()
  list(APPEND libcommon_files
    perfglue/disabled_stubs.cc)
endif()

add_library(common-objs OBJECT ${libcommon_files})
set(ceph_common_objs
  $<TARGET_OBJECTS:common-auth-objs>
  $<TARGET_OBJECTS:common-common-objs>
  $<TARGET_OBJECTS:common-msg-objs>
  $<TARGET_OBJECTS:common_buffer_obj>
  $<TARGET_OBJECTS:common_texttable_obj>
  $<TARGET_OBJECTS:compressor_objs>
  $<TARGET_OBJECTS:common-objs>
  $<TARGET_OBJECTS:common_mountcephfs_objs>
  $<TARGET_OBJECTS:global_common_objs>
  $<TARGET_OBJECTS:crush_objs>)
set(ceph_common_deps
  json_spirit erasure_code arch crc32
  ${LIB_RESOLV}
  Boost::thread
  Boost::system
  Boost::regex
  Boost::random
  Boost::program_options
  Boost::date_time
  Boost::iostreams
  ${BLKID_LIBRARIES}
  ${Backtrace_LIBRARIES}
  ${BLKIN_LIBRARIES}
  ${CRYPTO_LIBS}
  ${GSSAPI_LIBRARIES}
  ${CMAKE_THREAD_LIBS_INIT}
  ${CMAKE_DL_LIBS})
if(HAVE_UDEV)
  list(APPEND ceph_common_deps ${UDEV_LIBRARIES})
endif()

if(HAVE_VERBS)
  list(APPEND ceph_common_deps IBVerbs::verbs)
endif()

if(HAVE_RDMACM)
  list(APPEND ceph_common_deps RDMA::RDMAcm)
endif()

if(NOT WITH_SYSTEM_BOOST)
  list(APPEND ceph_common_deps ${ZLIB_LIBRARIES})
endif()

if(HAVE_QATZIP)
  list(APPEND ceph_common_deps ${QATZIP_LIBRARIES})
endif()

if(WITH_DPDK)
  list(APPEND ceph_common_deps common_async_dpdk)
endif()

add_library(common STATIC ${ceph_common_objs})
target_link_libraries(common ${ceph_common_deps})

add_library(ceph-common SHARED ${ceph_common_objs})
target_link_libraries(ceph-common ${ceph_common_deps})
# appease dpkg-shlibdeps
set_target_properties(ceph-common PROPERTIES
  SOVERSION 0
  INSTALL_RPATH "")
if(NOT APPLE AND NOT FREEBSD)
  # Apple uses Mach-O, not ELF. so this option does not apply to APPLE.
  #
  # prefer the local symbol definitions when binding references to global
  # symbols. otherwise we could reference the symbols defined by the application
  # with the same name, instead of using the one defined in libceph-common.
  # in other words, we require libceph-common to use local symbols, even if redefined
  # in application".
  set_property(
    TARGET ceph-common
    APPEND APPEND_STRING
    PROPERTY LINK_FLAGS "-Wl,-Bsymbolic -Wl,-Bsymbolic-functions")
endif()

install(
  TARGETS ceph-common
  LIBRARY
  DESTINATION ${CEPH_INSTALL_PKGLIBDIR}
  NAMELINK_SKIP)

if(${WITH_LTTNG})
  add_subdirectory(tracing)
  add_dependencies(common-objs oprequest-tp)
endif(${WITH_LTTNG})

add_subdirectory(global)

add_subdirectory(lua)

# rados object classes
add_subdirectory(cls)

# RADOS client/library
add_subdirectory(osdc)

# heal_profiler
add_subdirectory(perfglue)

add_library(rados_snap_set_diff_obj OBJECT librados/snap_set_diff.cc)

option(WITH_LIBRADOSSTRIPER "build with libradosstriper support" ON)

add_subdirectory(include)
add_subdirectory(librados)
add_subdirectory(libradosstriper)

if(WITH_LIBRADOSSTRIPER)
  add_subdirectory(libradosstriper)
endif()

if(WITH_MGR)
  add_subdirectory(mgr)
endif()

set(librados_config_srcs
  librados-config.cc)
add_executable(librados-config ${librados_config_srcs})
target_link_libraries(librados-config librados global ${BLKID_LIBRARIES} ${CMAKE_DL_LIBS} ${GSSAPI_LIBRARIES})

install(TARGETS librados-config DESTINATION bin)

# virtualenv base directory for ceph-disk and ceph-detect-init
set(CEPH_BUILD_VIRTUALENV $ENV{TMPDIR})
if(NOT CEPH_BUILD_VIRTUALENV)
  set(CEPH_BUILD_VIRTUALENV ${CMAKE_BINARY_DIR})
endif()

add_subdirectory(pybind)
add_subdirectory(ceph-disk)
add_subdirectory(ceph-volume)
add_subdirectory(ceph-detect-init)

## dencoder
CHECK_C_COMPILER_FLAG("-fvar-tracking-assignments" HAS_VTA)
if(HAS_VTA)
  set_source_files_properties(test/encoding/ceph_dencoder.cc
    PROPERTIES COMPILE_FLAGS -fno-var-tracking-assignments)
endif()

set(dencoder_srcs
  test/encoding/ceph_dencoder.cc
  $<TARGET_OBJECTS:common_texttable_obj>
  )
if(${WITH_RADOSGW})
  list(APPEND dencoder_srcs
    rgw/rgw_dencoder.cc
  )
  set(DENCODER_EXTRALIBS
    rgw_a
    cls_rgw_client
  )
endif(${WITH_RADOSGW})
if(WITH_RBD)
  set(DENCODER_EXTRALIBS
    ${DENCODER_EXTRALIBS}
    cls_rbd_client
    rbd_mirror_types
    rbd_types
    rbd_replay_types)
  if(WITH_KRBD)
    set(DENCODER_EXTRALIBS
      ${DENCODER_EXTRALIBS}
      krbd)
  endif()
endif(WITH_RBD)

add_executable(ceph-dencoder ${dencoder_srcs})
target_link_libraries(ceph-dencoder
  global
  os
  osd
  mds
  mon
  journal
  ${DENCODER_EXTRALIBS}
  cls_lock_client
  cls_refcount_client
  cls_log_client
  cls_statelog_client
  cls_version_client
  cls_replica_log_client
  cls_user_client
  cls_journal_client
  cls_timeindex_client
  ${EXTRALIBS}
  ${CMAKE_DL_LIBS}
  )
install(TARGETS ceph-dencoder DESTINATION bin)

# Monitor
add_subdirectory(mon)
set(ceph_mon_srcs
  ceph_mon.cc)
add_executable(ceph-mon ${ceph_mon_srcs}
  $<TARGET_OBJECTS:common_texttable_obj>)
add_dependencies(ceph-mon erasure_code_plugins)
target_link_libraries(ceph-mon mon os global-static ceph-common
  ${EXTRALIBS}
  ${CMAKE_DL_LIBS} ${GSSAPI_LIBRARIES})
install(TARGETS ceph-mon DESTINATION bin)

# OSD/ObjectStore
# make rocksdb statically

if(NOT WITH_SYSTEM_ROCKSDB)
  include(BuildRocksDB)
  build_rocksdb()
endif(NOT WITH_SYSTEM_ROCKSDB)

include(TestBigEndian)
test_big_endian(CEPH_BIG_ENDIAN)
if(NOT CEPH_BIG_ENDIAN)
  set(CEPH_LITTLE_ENDIAN 1)
endif()

add_subdirectory(kv)
add_subdirectory(os)

set(cls_references_files objclass/class_api.cc)
add_library(cls_references_objs OBJECT ${cls_references_files})

add_subdirectory(osd)

set(ceph_osd_srcs
  ceph_osd.cc)
add_executable(ceph-osd ${ceph_osd_srcs})
add_dependencies(ceph-osd erasure_code_plugins)
target_link_libraries(ceph-osd osd os global-static common
  ${BLKID_LIBRARIES})
if(WITH_FUSE)
  target_link_libraries(ceph-osd ${FUSE_LIBRARIES})
endif()
set_target_properties(ceph-osd PROPERTIES
  POSITION_INDEPENDENT_CODE ${EXE_LINKER_USE_PIE}
  INSTALL_RPATH "")
install(TARGETS ceph-osd DESTINATION bin)

if (WITH_CEPHFS)
  add_subdirectory(mds)
  set(ceph_mds_srcs
    ceph_mds.cc)
  add_executable(ceph-mds ${ceph_mds_srcs})
  target_link_libraries(ceph-mds mds ${CMAKE_DL_LIBS} global-static ceph-common
    Boost::thread)
  install(TARGETS ceph-mds DESTINATION bin)
endif()

add_subdirectory(erasure-code)

# Support/Tools
if(WITH_TESTS)
  option(WITH_SYSTEM_GTEST "require and build with system gtest and gmock" OFF)
  if(WITH_SYSTEM_GTEST)
    find_package(GTest REQUIRED)
    find_package(GMock REQUIRED)
  else()
    add_subdirectory(googletest/googlemock)
    add_library(GMock::GMock ALIAS gmock)
    add_library(GMock::Main ALIAS gmock_main)
    target_include_directories(gmock INTERFACE
      $<TARGET_PROPERTY:gtest,INTERFACE_INCLUDE_DIRECTORIES>)
    target_include_directories(gmock_main INTERFACE
      $<TARGET_PROPERTY:gtest,INTERFACE_INCLUDE_DIRECTORIES>)
    add_library(GTest::GTest ALIAS gtest)
    add_library(GTest::Main ALIAS gtest_main)
  endif()
endif(WITH_TESTS)

add_subdirectory(compressor)

add_subdirectory(tools)

# dmClock (after gmock)

add_subdirectory(dmclock/src)

option(WITH_DMCLOCK_TESTS
  "enable the build of dmclock-tests and dmclock-data-struct tests binaries"
  OFF)
if(WITH_TESTS AND WITH_DMCLOCK_TESTS)
  # note: add_test is not being called, so dmclock tests aren't part
  # of ceph tests
  set(dmclock_TEST ON CACHE BOOL "" FORCE)
endif()
add_subdirectory(dmclock)

add_subdirectory(compressor)

add_subdirectory(tools)

if(WITH_TESTS)
  add_subdirectory(test)
endif()

add_subdirectory(crypto)

if(WITH_TESTS)
  configure_file(${CMAKE_SOURCE_DIR}/src/ceph-coverage.in
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ceph-coverage @ONLY)
  configure_file(${CMAKE_SOURCE_DIR}/src/ceph-debugpack.in
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ceph-debugpack @ONLY)
endif()

configure_file(${CMAKE_SOURCE_DIR}/src/ceph.in
  ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ceph @ONLY)

configure_file(${CMAKE_SOURCE_DIR}/src/ceph-crush-location.in
  ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ceph-crush-location @ONLY)

configure_file(${CMAKE_SOURCE_DIR}/src/init-ceph.in
  ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/init-ceph @ONLY)

configure_file(ceph-post-file.in
  ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ceph-post-file @ONLY)

configure_file(ceph-crash.in
  ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ceph-crash @ONLY)

if(WITH_TESTS)
  install(PROGRAMS
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ceph-debugpack
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ceph-coverage
    DESTINATION bin)
endif()

install(PROGRAMS
  ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ceph
  ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ceph-crush-location
  ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ceph-post-file
  ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ceph-crash
  ${CMAKE_SOURCE_DIR}/src/ceph-run
  ${CMAKE_SOURCE_DIR}/src/ceph-rest-api
  ${CMAKE_SOURCE_DIR}/src/ceph-clsinfo
  DESTINATION bin)
install(PROGRAMS
  ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/init-ceph
  DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/init.d
  RENAME ceph)

install(FILES
  ${CMAKE_SOURCE_DIR}/share/id_rsa_drop.ceph.com
  ${CMAKE_SOURCE_DIR}/share/id_rsa_drop.ceph.com.pub
  ${CMAKE_SOURCE_DIR}/share/known_hosts_drop.ceph.com
  DESTINATION ${CMAKE_INSTALL_DATADIR}/ceph)

install(PROGRAMS
  ceph_common.sh
  ceph-osd-prestart.sh
  DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/ceph)

install(PROGRAMS
  ${CMAKE_SOURCE_DIR}/src/ceph-create-keys
#  ${CMAKE_SOURCE_DIR}/src/ceph-disk
  DESTINATION sbin)

add_subdirectory(bash_completion)

if(WITH_LIBCEPHFS OR WITH_KRBD)
  set(parse_secret_files
    common/secret.c)
  add_library(parse_secret_objs OBJECT ${parse_secret_files})
endif()

add_subdirectory(client)

if(WITH_LIBCEPHFS)
  find_package(PkgConfig QUIET REQUIRED)
  pkg_check_modules(CAPNG REQUIRED libcap-ng)
  set(libcephfs_srcs libcephfs.cc)
  add_library(cephfs ${CEPH_SHARED} ${libcephfs_srcs})
  target_link_libraries(cephfs PRIVATE client ceph-common
    ${CRYPTO_LIBS} ${EXTRALIBS})
  if(ENABLE_SHARED)
    foreach(name ceph-common client osdc)
      set(CEPHFS_LINK_FLAGS "${CEPHFS_LINK_FLAGS} -Wl,--exclude-libs,lib${name}.a")
    endforeach()
    set_target_properties(cephfs PROPERTIES
      OUTPUT_NAME cephfs
      VERSION 2.0.0
      SOVERSION 2)
    if(NOT APPLE)
      foreach(name ceph-common client osdc)
        set_property(TARGET cephfs APPEND_STRING PROPERTY
          LINK_FLAGS " -Wl,--exclude-libs,lib${name}.a")
      endforeach()
    endif()
  endif(ENABLE_SHARED)
  install(TARGETS cephfs DESTINATION ${CMAKE_INSTALL_LIBDIR})
  install(DIRECTORY
    "${CMAKE_SOURCE_DIR}/src/include/cephfs"
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
  set(ceph_syn_srcs
    ceph_syn.cc
    client/SyntheticClient.cc)
  add_executable(ceph-syn ${ceph_syn_srcs})
  target_link_libraries(ceph-syn client global-static ceph-common)
  install(TARGETS ceph-syn DESTINATION bin)
  if(LINUX)
    add_subdirectory(mount)
  endif()
endif(WITH_LIBCEPHFS)

if(WITH_FUSE)
  set(ceph_fuse_srcs
    ceph_fuse.cc
    client/fuse_ll.cc)
  add_executable(ceph-fuse ${ceph_fuse_srcs})
  target_link_libraries(ceph-fuse ${FUSE_LIBRARIES}
    ${GSSAPI_LIBRARIES} client ceph-common global-static)
  set_target_properties(ceph-fuse PROPERTIES
    COMPILE_FLAGS "-I${FUSE_INCLUDE_DIRS}"
    POSITION_INDEPENDENT_CODE ${EXE_LINKER_USE_PIE})
  install(TARGETS ceph-fuse DESTINATION bin)
  install(PROGRAMS mount.fuse.ceph DESTINATION ${CMAKE_INSTALL_SBINDIR})
endif(WITH_FUSE)

add_subdirectory(journal)

if(WITH_RBD)
  if(WITH_KRBD)
    add_library(krbd STATIC krbd.cc
      $<TARGET_OBJECTS:parse_secret_objs>)
    target_link_libraries(krbd keyutils::keyutils)
  endif()
  add_subdirectory(librbd)
  if(WITH_FUSE)
    add_subdirectory(rbd_fuse)
  endif()

  install(PROGRAMS
    ${CMAKE_SOURCE_DIR}/src/ceph-rbdnamer
    ${CMAKE_SOURCE_DIR}/src/rbd-replay-many
    ${CMAKE_SOURCE_DIR}/src/rbdmap
    DESTINATION ${CMAKE_INSTALL_BINDIR})
  add_subdirectory(rbd_replay)
endif(WITH_RBD)

# RadosGW
if(WITH_KVS)
  add_subdirectory(key_value_store)
endif(WITH_KVS)

if(WITH_RADOSGW)
  set(civetweb_common_files civetweb/src/civetweb.c)
  add_library(civetweb_common_objs OBJECT ${civetweb_common_files})
  target_include_directories(civetweb_common_objs SYSTEM PRIVATE
	"${CMAKE_SOURCE_DIR}/src/civetweb/include")
  set_property(TARGET civetweb_common_objs
    APPEND PROPERTY COMPILE_DEFINITIONS USE_IPV6=1)
  if(USE_OPENSSL)
    set_property(TARGET civetweb_common_objs
      APPEND PROPERTY COMPILE_DEFINITIONS NO_SSL_DL=1)
    target_include_directories(civetweb_common_objs PRIVATE
      "${SSL_INCLUDE_DIR}")
  endif(USE_OPENSSL)
  if (LIBSSL_SONAME)
    set_property(TARGET civetweb_common_objs
      APPEND PROPERTY COMPILE_DEFINITIONS SSL_LIB="${LIBSSL_SONAME}")
    set_property(TARGET civetweb_common_objs
      APPEND PROPERTY COMPILE_DEFINITIONS CRYPTO_LIB="${LIBCRYPTO_SONAME}")
  endif()

  if (OPENSSL_FOUND)
    # Use cmake to determine openssl version, a TODO is to make
    # civetweb itself do this based on openssl_api_compat strings
    if (NOT (OPENSSL_VERSION VERSION_LESS "1.1"))
      message(STATUS "Setting civetweb to use OPENSSL >= 1.1")
      set_property(TARGET civetweb_common_objs
        APPEND PROPERTY COMPILE_DEFINITIONS OPENSSL_API_1_1=1)
    endif()
  endif(OPENSSL_FOUND)
  add_subdirectory(rgw)

endif(WITH_RADOSGW)

install(FILES
  sample.ceph.conf
  DESTINATION ${CMAKE_INSTALL_DOCDIR})

# Now create a usable config.h
configure_file(
  ${CMAKE_SOURCE_DIR}/src/include/config-h.in.cmake
  ${CMAKE_BINARY_DIR}/include/acconfig.h
)

add_subdirectory(brag)

# Everything you need to spin up a cluster with vstart.sh
add_custom_target(vstart-base DEPENDS
    ceph-osd
    ceph-mon
    ceph-mgr
    ceph-authtool
    ceph-conf
    monmaptool
    crushtool
    rados
    cython${PY_BINDING_INFIX}_rados)
if (WITH_MGR)
  add_dependencies(vstart-base ceph-mgr)
endif()

add_custom_target(vstart DEPENDS vstart-base)
if (WITH_RBD)
  add_dependencies(vstart cython${PY_BINDING_INFIX}_rbd)
endif()
if (WITH_CEPHFS)
  add_dependencies(vstart ceph-mds)
endif()
if(WITH_RADOSGW)
  add_dependencies(vstart radosgw radosgw-admin)
endif(WITH_RADOSGW)

if(WITH_LTTNG)
  add_dependencies(vstart tracepoint_libraries)
endif(WITH_LTTNG)

if(WITH_MGR AND WITH_MGR_DASHBOARD_FRONTEND AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64")
  add_dependencies(vstart mgr-dashboard-frontend-build)
endif()

if(WITH_MGR)
  add_dependencies(vstart ceph-volume-venv-setup)
endif()

# Everything you need to run CephFS tests
add_custom_target(cephfs_testing DEPENDS
    vstart
    rados
    cython${PY_BINDING_INFIX}_modules
    cephfs
    cls_cephfs
    ceph-fuse
    ceph-dencoder
    cephfs-journal-tool
    cephfs-data-scan
    cephfs-table-tool)

if (IS_DIRECTORY "${PROJECT_SOURCE_DIR}/.git")
  add_custom_target(
    git-update
    COMMAND git submodule sync
    COMMAND git submodule update --force --init --recursive
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
endif()

add_subdirectory(script)

if(WITH_EMBEDDED)
  add_subdirectory(libcephd)
endif()
