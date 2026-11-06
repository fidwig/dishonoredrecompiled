# Direct3D 9 to Apple Metal 4 translation layer (jdrly/d9mt)

set(D9MT_ROOT "${CMAKE_SOURCE_DIR}/external/d9mt-src")

set(D9MT_SOURCES
  # SM3 & IR parser (dxbc-spirv)
  ${D9MT_ROOT}/third_party/dxbc-spirv/sm3/sm3_converter.cpp
  ${D9MT_ROOT}/third_party/dxbc-spirv/sm3/sm3_disasm.cpp
  ${D9MT_ROOT}/third_party/dxbc-spirv/sm3/sm3_io_map.cpp
  ${D9MT_ROOT}/third_party/dxbc-spirv/sm3/sm3_parser.cpp
  ${D9MT_ROOT}/third_party/dxbc-spirv/sm3/sm3_registers.cpp
  ${D9MT_ROOT}/third_party/dxbc-spirv/sm3/sm3_resources.cpp
  ${D9MT_ROOT}/third_party/dxbc-spirv/sm3/sm3_types.cpp
  ${D9MT_ROOT}/third_party/dxbc-spirv/ir/ir.cpp
  ${D9MT_ROOT}/third_party/dxbc-spirv/ir/ir_builder.cpp
  ${D9MT_ROOT}/third_party/dxbc-spirv/ir/ir_disasm.cpp
  ${D9MT_ROOT}/third_party/dxbc-spirv/ir/ir_divergence.cpp
  ${D9MT_ROOT}/third_party/dxbc-spirv/ir/ir_dominance.cpp
  ${D9MT_ROOT}/third_party/dxbc-spirv/ir/ir_legalize.cpp
  ${D9MT_ROOT}/third_party/dxbc-spirv/ir/ir_serialize.cpp
  ${D9MT_ROOT}/third_party/dxbc-spirv/ir/ir_utils.cpp
  ${D9MT_ROOT}/third_party/dxbc-spirv/ir/passes/ir_pass_arithmetic.cpp
  ${D9MT_ROOT}/third_party/dxbc-spirv/ir/passes/ir_pass_buffer_kind.cpp
  ${D9MT_ROOT}/third_party/dxbc-spirv/ir/passes/ir_pass_cfg_cleanup.cpp
  ${D9MT_ROOT}/third_party/dxbc-spirv/ir/passes/ir_pass_cfg_convert.cpp
  ${D9MT_ROOT}/third_party/dxbc-spirv/ir/passes/ir_pass_cse.cpp
  ${D9MT_ROOT}/third_party/dxbc-spirv/ir/passes/ir_pass_derivative.cpp
  ${D9MT_ROOT}/third_party/dxbc-spirv/ir/passes/ir_pass_descriptor_indexing.cpp
  ${D9MT_ROOT}/third_party/dxbc-spirv/ir/passes/ir_pass_function.cpp
  ${D9MT_ROOT}/third_party/dxbc-spirv/ir/passes/ir_pass_lower_consume.cpp
  ${D9MT_ROOT}/third_party/dxbc-spirv/ir/passes/ir_pass_lower_io.cpp
  ${D9MT_ROOT}/third_party/dxbc-spirv/ir/passes/ir_pass_lower_min16.cpp
  ${D9MT_ROOT}/third_party/dxbc-spirv/ir/passes/ir_pass_propagate_resource_types.cpp
  ${D9MT_ROOT}/third_party/dxbc-spirv/ir/passes/ir_pass_propagate_types.cpp
  ${D9MT_ROOT}/third_party/dxbc-spirv/ir/passes/ir_pass_remove_unused.cpp
  ${D9MT_ROOT}/third_party/dxbc-spirv/ir/passes/ir_pass_scalarize.cpp
  ${D9MT_ROOT}/third_party/dxbc-spirv/ir/passes/ir_pass_scratch.cpp
  ${D9MT_ROOT}/third_party/dxbc-spirv/ir/passes/ir_pass_ssa.cpp
  ${D9MT_ROOT}/third_party/dxbc-spirv/ir/passes/ir_pass_sync.cpp
  ${D9MT_ROOT}/third_party/dxbc-spirv/util/util_float16.cpp
  ${D9MT_ROOT}/third_party/dxbc-spirv/util/util_log.cpp
  ${D9MT_ROOT}/third_party/dxbc-spirv/util/util_md5.cpp
  ${D9MT_ROOT}/third_party/dxbc-spirv/util/util_swizzle.cpp

  # AIR generation & compilation
  ${D9MT_ROOT}/air/air_builder.cpp
  ${D9MT_ROOT}/air/air_metallib.cpp
  ${D9MT_ROOT}/airlib/unix.cpp

  # Bridge (winemetal & d9mtair)
  ${D9MT_ROOT}/bridge/unix/winemetal_unix.c
  ${D9MT_ROOT}/bridge/unix/air_cache.c
  ${D9MT_ROOT}/bridge/winemetal_thunks.c
  ${D9MT_ROOT}/bridge/d9mtair_thunks.c

  # Driver Util
  ${D9MT_ROOT}/driver/util/config/config.cpp
  ${D9MT_ROOT}/driver/util/log/log.cpp
  ${D9MT_ROOT}/driver/util/log/log_debug.cpp
  ${D9MT_ROOT}/driver/util/sha1/sha1.c
  ${D9MT_ROOT}/driver/util/sha1/sha1_util.cpp
  ${D9MT_ROOT}/driver/util/sync/sync_recursive.cpp
  ${D9MT_ROOT}/driver/util/com/com_destruction_notifier.cpp
  ${D9MT_ROOT}/driver/util/com/com_private_data.cpp
  ${D9MT_ROOT}/driver/util/thread.cpp
  ${D9MT_ROOT}/driver/util/util_env.cpp
  ${D9MT_ROOT}/driver/util/util_fatal_log.cpp
  ${D9MT_ROOT}/driver/util/util_file.cpp
  ${D9MT_ROOT}/driver/util/util_flush.cpp
  ${D9MT_ROOT}/driver/util/util_fps_limiter.cpp
  ${D9MT_ROOT}/driver/util/util_gdi.cpp
  ${D9MT_ROOT}/driver/util/util_luid.cpp
  ${D9MT_ROOT}/driver/util/util_matrix.cpp
  ${D9MT_ROOT}/driver/util/util_shared_res.cpp
  ${D9MT_ROOT}/driver/util/util_sleep.cpp
  ${D9MT_ROOT}/driver/util/util_string.cpp
  ${D9MT_ROOT}/driver/util/util_unmap.cpp

  # WSI
  ${D9MT_ROOT}/driver/wsi/wsi_platform.cpp
  ${D9MT_ROOT}/driver/wsi/mac/wsi_platform_mac.cpp

  # CS & Allocator
  ${D9MT_ROOT}/driver/cs/dxvk_cs.cpp
  ${D9MT_ROOT}/driver/dxvk/dxvk_allocator.cpp

  # Metal Backend & Stubs
  ${D9MT_ROOT}/driver/mtl/mtl_context_stub.cpp
  ${D9MT_ROOT}/driver/mtl/mtl_device_stub.cpp
  ${D9MT_ROOT}/driver/mtl/mtl_format.cpp
  ${D9MT_ROOT}/driver/mtl/mtl_memory_profile.cpp
  ${D9MT_ROOT}/driver/mtl/mtl_presenter_stub.cpp
  ${D9MT_ROOT}/driver/mtl/mtl_state_cache.cpp
  ${D9MT_ROOT}/driver/mtl/mtl_state_cache_core.cpp
  ${D9MT_ROOT}/driver/backend/mtl_context_record.cpp
  ${D9MT_ROOT}/driver/backend/mtl_context_transfer.cpp
  ${D9MT_ROOT}/driver/backend/mtl_device_impl.cpp
  ${D9MT_ROOT}/driver/backend/mtl_lifetime.cpp
  ${D9MT_ROOT}/driver/backend/mtl_presenter_impl.cpp
  ${D9MT_ROOT}/driver/backend/mtl_pso_cache.cpp
  ${D9MT_ROOT}/driver/backend/mtl_residency.cpp
  ${D9MT_ROOT}/driver/backend/mtl_ring.cpp
  ${D9MT_ROOT}/driver/backend/mtl_submission.cpp

  # D3D9 Frontend
  ${D9MT_ROOT}/driver/d3d9/d3d9_adapter.cpp
  ${D9MT_ROOT}/driver/d3d9/d3d9_annotation.cpp
  ${D9MT_ROOT}/driver/d3d9/d3d9_bridge.cpp
  ${D9MT_ROOT}/driver/d3d9/d3d9_buffer.cpp
  ${D9MT_ROOT}/driver/d3d9/d3d9_com_guid.cpp
  ${D9MT_ROOT}/driver/d3d9/d3d9_common_buffer.cpp
  ${D9MT_ROOT}/driver/d3d9/d3d9_common_texture.cpp
  ${D9MT_ROOT}/driver/d3d9/d3d9_constant_buffer.cpp
  ${D9MT_ROOT}/driver/d3d9/d3d9_constant_copy.cpp
  ${D9MT_ROOT}/driver/d3d9/d3d9_cursor.cpp
  ${D9MT_ROOT}/driver/d3d9/d3d9_debug.cpp
  ${D9MT_ROOT}/driver/d3d9/d3d9_device.cpp
  ${D9MT_ROOT}/driver/d3d9/d3d9_fixed_function.cpp
  ${D9MT_ROOT}/driver/d3d9/d3d9_format.cpp
  ${D9MT_ROOT}/driver/d3d9/d3d9_initializer.cpp
  ${D9MT_ROOT}/driver/d3d9/d3d9_interface.cpp
  ${D9MT_ROOT}/driver/d3d9/d3d9_main.cpp
  ${D9MT_ROOT}/driver/d3d9/d3d9_monitor.cpp
  ${D9MT_ROOT}/driver/d3d9/d3d9_multithread.cpp
  ${D9MT_ROOT}/driver/d3d9/d3d9_names.cpp
  ${D9MT_ROOT}/driver/d3d9/d3d9_options.cpp
  ${D9MT_ROOT}/driver/d3d9/d3d9_query.cpp
  ${D9MT_ROOT}/driver/d3d9/d3d9_shader.cpp
  ${D9MT_ROOT}/driver/d3d9/d3d9_shader_analysis.cpp
  ${D9MT_ROOT}/driver/d3d9/d3d9_shader_compiler.cpp
  ${D9MT_ROOT}/driver/d3d9/d3d9_shader_validator.cpp
  ${D9MT_ROOT}/driver/d3d9/d3d9_state.cpp
  ${D9MT_ROOT}/driver/d3d9/d3d9_stateblock.cpp
  ${D9MT_ROOT}/driver/d3d9/d3d9_surface.cpp
  ${D9MT_ROOT}/driver/d3d9/d3d9_swapchain.cpp
  ${D9MT_ROOT}/driver/d3d9/d3d9_swvp_emu.cpp
  ${D9MT_ROOT}/driver/d3d9/d3d9_texture.cpp
  ${D9MT_ROOT}/driver/d3d9/d3d9_util.cpp
  ${D9MT_ROOT}/driver/d3d9/d3d9_vertex_declaration.cpp
  ${D9MT_ROOT}/driver/d3d9/d3d9_volume.cpp
  ${D9MT_ROOT}/driver/d3d9/d3d9_window.cpp
  ${D9MT_ROOT}/driver/d3d9/d3dx9_stubs.cpp
)

set_source_files_properties(
  ${D9MT_ROOT}/bridge/unix/winemetal_unix.c
  ${D9MT_ROOT}/bridge/unix/air_cache.c
  PROPERTIES COMPILE_FLAGS "-x objective-c"
)

add_library(d9mt STATIC ${D9MT_SOURCES})

target_compile_features(d9mt PUBLIC cxx_std_17)

target_include_directories(d9mt PUBLIC
  ${CMAKE_SOURCE_DIR}/external/d3d9-headers/include
)

target_include_directories(d9mt PRIVATE
  ${D9MT_ROOT}/driver
  ${D9MT_ROOT}/driver/d3d9
  ${D9MT_ROOT}/driver/backend
  ${D9MT_ROOT}/driver/mtl
  ${D9MT_ROOT}/driver/cs
  ${D9MT_ROOT}/driver/wsi
  ${D9MT_ROOT}/driver/util
  ${D9MT_ROOT}/bridge
  ${D9MT_ROOT}/air
  ${D9MT_ROOT}/airlib
  ${D9MT_ROOT}/third_party/dxbc-spirv
  /opt/homebrew/Cellar/llvm@15/15.0.7/include
)

target_compile_definitions(d9mt PRIVATE
  __STDC_CONSTANT_MACROS
  __STDC_FORMAT_MACROS
  __STDC_LIMIT_MACROS
  DXVK_WSI_MAC
)

target_link_libraries(d9mt PUBLIC
  /opt/homebrew/Cellar/llvm@15/15.0.7/lib/libLLVM.dylib
  "-framework Metal"
  "-framework MetalFX"
  "-framework Foundation"
  "-framework QuartzCore"
  "-framework Cocoa"
  "-framework ColorSync"
)

# Dishonored::DirectX9 interface library providing headers and linking d9mt
add_library(Dishonored_DirectX9 INTERFACE)
add_library(Dishonored::DirectX9 ALIAS Dishonored_DirectX9)
target_include_directories(Dishonored_DirectX9 INTERFACE
  ${CMAKE_SOURCE_DIR}/external/d3d9-headers/include
)
target_link_libraries(Dishonored_DirectX9 INTERFACE d9mt)
