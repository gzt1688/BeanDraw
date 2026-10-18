# 查找 cargo 并构建 rust_core 静态库（开发大纲第 2/4 章）
#
# 产出（PARENT_SCOPE 变量）：
#   CRAFTMASTER_RUST_LIB         静态库路径（craftmaster_core.lib）
#   CRAFTMASTER_RUST_SYSTEM_LIBS MSVC 链接 Rust 静态库所需系统库
#
# 同时创建全局 custom target `craftmaster_rust_core`，
# 主工程通过 add_dependencies(BeanDraw craftmaster_rust_core) 保证先编译。
#
# 鸿蒙交叉编译（aarch64-ohos）由 build_scripts/build_ohos.sh 处理，
# 本模块仅负责本机（host）构建，保证 Windows 开发流畅。

function(craftmaster_add_rust_core)
  find_program(CARGO_EXECUTABLE cargo
    REQUIRED
    HINTS $ENV{CARGO_HOME}/bin "$ENV{USERPROFILE}/.cargo/bin"
  )

  set(CRAFTMASTER_RUST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../rust_core)

  if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(_profile debug)
    set(_cargo_flags "")
  else()
    set(_profile release)
    set(_cargo_flags "--release")
  endif()

  set(_rust_lib_dir ${CRAFTMASTER_RUST_DIR}/target/${_profile})
  set(_rust_lib ${_rust_lib_dir}/craftmaster_core.lib)

  add_custom_target(craftmaster_rust_core ALL
    COMMAND ${CARGO_EXECUTABLE} build ${_cargo_flags}
            --manifest-path ${CRAFTMASTER_RUST_DIR}/Cargo.toml
    WORKING_DIRECTORY ${CRAFTMASTER_RUST_DIR}
    COMMENT "Building rust_core (${_profile}) static library"
    BYPRODUCTS ${_rust_lib}
    VERBATIM
  )

  if(WIN32)
    # MSVC 链接 Rust std 静态库所需系统库（随 Rust 版本可能微调）
    set(_system_libs ws2_32 userenv advapi32 ntdll bcrypt)
  else()
    set(_system_libs "")
  endif()

  # 导出给调用方
  set(CRAFTMASTER_RUST_LIB ${_rust_lib} PARENT_SCOPE)
  set(CRAFTMASTER_RUST_SYSTEM_LIBS ${_system_libs} PARENT_SCOPE)
endfunction()
