# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.13.1)
set(SHIELD ssd1306_128x32)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(external_lib)



FILE(GLOB app_sources src/*.cc)
target_sources(app PRIVATE ${app_sources})

zephyr_include_directories(src)
zephyr_library_include_directories(${ZEPHYR_BASE}/samples/bluetooth)

#zephyr_cc_option(-lstdc++)

# The external static library that we are linking with does not know
# how to build for this platform so we export all the flags used in
# this zephyr build to the external build system.
#
# Other external build systems may be self-contained enough that they
# do not need any build information from zephyr. Or they may be
# incompatible with certain zephyr options and need them to be
# filtered out.



list(GET ZEPHYR_RUNNER_ARGS_jlink 1 MCPU_FLAG ) #Here we assume that '-mcpu' will be the second argument. This might be wrong. 
string(REPLACE "=" ";" MCPU_FLAG_LIST ${MCPU_FLAG})
list(GET MCPU_FLAG_LIST 0 MCPU)

set(TARGET cortex_m_generic) #set(TARGET ${BOARD})
set(TARGET_ARCH cortex-m4)

set(TF_SRC_DIR /Users/valentin/research/toolchains/tflite-micro) #path to folder containing tensorflow
set(TF_MAKE_DIR ${TF_SRC_DIR}/tensorflow/lite/micro/tools/make)
set(TF_LIB_DIR ${TF_MAKE_DIR}/gen/${TARGET}_${TARGET_ARCH}_default/lib)
set(extra_project_flags "-mcpu=${TARGET_ARCH} -mthumb -mno-thumb-interwork -mfpu=fpv5-sp-d16 -mfloat-abi=softfp") #I had to remove -DTF_LITE_STATIC_MEMORY to make hello_world run. 

zephyr_get_include_directories_for_lang_as_string(       C C_includes)
zephyr_get_system_include_directories_for_lang_as_string(C C_system_includes)
zephyr_get_compile_definitions_for_lang_as_string(       C C_definitions)
zephyr_get_compile_options_for_lang_as_string(           C C_options)

set(external_project_cflags
  "${C_includes} ${C_definitions} ${optC_optionsions} ${C_system_includes} ${extra_project_flags} -DCMSIS_NN"
)

zephyr_get_include_directories_for_lang_as_string(       CXX CXX_includes)
zephyr_get_system_include_directories_for_lang_as_string(CXX CXX_system_includes)
zephyr_get_compile_definitions_for_lang_as_string(       CXX CXX_definitions)
zephyr_get_compile_options_for_lang_as_string(           CXX CXX_options)


set(external_project_cxxflags
  "${CXX_includes} ${CXX_definitions} ${CXX_options} ${CXX_system_includes} ${extra_project_flags} -DCMSIS_NN"
)

include(ExternalProject)

# Add an external project to be able download and build the third
# party library. In this case downloading is not necessary as it has
# been committed to the repository.




set(submake gmake)

ExternalProject_Add(
  tf_project                 # Name for custom target
  #PREFIX     ${mylib_build_dir} # Root dir for entire project
  SOURCE_DIR ${TF_SRC_DIR}
  BINARY_DIR ${TF_SRC_DIR} # This particular build system is invoked from the root
  CONFIGURE_COMMAND ""    # Skip configuring the project, e.g. with autoconf
  BUILD_COMMAND
  ${submake} -f tensorflow/lite/micro/tools/make/Makefile
  TARGET=${TARGET}
  TARGET_ARCH=${TARGET_ARCH}
  OPTIMIZED_KERNEL_DIR=cmsis_nn
  TARGET_TOOLCHAIN_ROOT=${GNUARMEMB_TOOLCHAIN_PATH}/bin/
  TARGET_TOOLCHAIN_PREFIX=arm-none-eabi-
  #PREFIX=${mylib_build_dir}
  CC=${CMAKE_C_COMPILER}
  CXX=${CMAKE_CXX_COMPILER}
  AR=${CMAKE_AR}
  CCFLAGS=${external_project_cflags} 
  CXXFLAGS=${external_project_cxxflags} 
  microlite 
  INSTALL_COMMAND ""      # This particular build system has no install command
  BUILD_BYPRODUCTS ${TF_LIB_DIR}/libtensorflow-microlite.a
  )

# Create a wrapper CMake library that our app can link with
add_library(tf_lib STATIC IMPORTED GLOBAL)

add_dependencies(
  tf_lib
  tf_project
  )
set_target_properties(tf_lib PROPERTIES IMPORTED_LOCATION             ${TF_LIB_DIR}/libtensorflow-microlite.a)
set_target_properties(tf_lib PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${TF_SRC_DIR};${TF_SRC_DIR}/tensorflow/lite/micro;${TF_MAKE_DIR}/downloads/flatbuffers/include") 

#zephyr_include_directories(
#  ${GNUARMEMB_TOOLCHAIN_PATH}/arm-none-eabi/include/
#  ${GNUARMEMB_TOOLCHAIN_PATH}/arm-none-eabi/include/c++/9.2.1
#  ${GNUARMEMB_TOOLCHAIN_PATH}/arm-none-eabi/include/c++/9.2.1/arm-none-eabi
#)


target_link_libraries(app PUBLIC tf_lib)


