#version 460
#define DEBUG

#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_explicit_arithmetic_types : enable
#extension GL_EXT_shader_atomic_int64 : enable
#extension GL_EXT_shader_atomic_float : enable
#extension GL_EXT_shader_image_load_formatted : enable
#extension GL_KHR_memory_scope_semantics : enable
#extension GL_EXT_shared_memory_block : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_null_initializer : enable
#extension GL_EXT_buffer_reference2 : enable
#extension GL_EXT_buffer_reference_uvec2 : enable

#ifdef DEBUG
#extension GL_EXT_debug_printf : enable
#endif

#include "tiler.glsl"

void main() {
    uvec3 pos = gl_GlobalInvocationID;
    uint64_t tiledSliceOffset = 0;
    uint64_t linearSliceOffset = 0;
    int arraySlice = 0;
    int fragmentIndex = 0;
    if (config.tiledSurfaceSize != 0) {
        tiledSliceOffset = pos.z * config.tiledSurfaceSize;
        linearSliceOffset = pos.z * config.linearSurfaceSize;
        pos.z = 0;
    }

    uint64_t tiledByteOffset = getTiledBitOffset2D(
        config.tileMode,
        config.macroTileMode,
        config.dataSize,
        arraySlice,
        config.numFragments,
        config.bitsPerElement,
        pos,
        fragmentIndex
    ) / 8;

    tiledByteOffset += tiledSliceOffset;

    uint64_t linearByteOffset = computeLinearElementByteOffset(
        pos,
        0,
        config.linearDataSize.x,
        config.linearDataSize.x * config.linearDataSize.y,
        config.bitsPerElement,
        1 << config.numFragments
    );

    linearByteOffset += linearSliceOffset;

    uint32_t bpp = (config.bitsPerElement + 7) / 8;
    if (bpp == 1 && (linearByteOffset & 1) != 0) {
        return;
    }

#ifdef DEBUG
    if (config.srcAddress + linearByteOffset + bpp > config.srcEndAddress) {
        debugPrintfEXT("tiler2d: out of src buffer %d x %d x %d, src offset: %lu, src size: %lu", pos.x, pos.y, pos.z,
            linearByteOffset, config.srcEndAddress - config.srcAddress);
        return;
    }

    if (config.dstAddress + tiledByteOffset + bpp > config.dstEndAddress) {
        debugPrintfEXT("tiler2d: out of dst buffer %d x %d x %d, offset %lx, size %lx", pos.x, pos.y, pos.z, tiledByteOffset, config.dstEndAddress - config.dstAddress);
        return;
    }
#endif

    switch (bpp) {
    case 1:
        // buffer_reference_uint8_t(config.dstAddress + tiledByteOffset).data = buffer_reference_uint8_t(config.srcAddress + linearByteOffset).data;
        // break;
    case 2:
        buffer_reference_uint16_t(config.dstAddress + tiledByteOffset).data = buffer_reference_uint16_t(config.srcAddress + linearByteOffset).data;
        break;

    case 4:
        buffer_reference_uint32_t(config.dstAddress + tiledByteOffset).data = buffer_reference_uint32_t(config.srcAddress + linearByteOffset).data;
        break;

    case 8:
        buffer_reference_uint64_t(config.dstAddress + tiledByteOffset).data = buffer_reference_uint64_t(config.srcAddress + linearByteOffset).data;
        break;

    case 16:
        buffer_reference_uint64_t(config.dstAddress + tiledByteOffset).data = buffer_reference_uint64_t(config.srcAddress + linearByteOffset).data;
        buffer_reference_uint64_t(config.dstAddress + tiledByteOffset + 8).data = buffer_reference_uint64_t(config.srcAddress + linearByteOffset + 8).data;
        break;

    case 32:
        buffer_reference_uint64_t(config.dstAddress + tiledByteOffset).data = buffer_reference_uint64_t(config.srcAddress + linearByteOffset).data;
        buffer_reference_uint64_t(config.dstAddress + tiledByteOffset + 8).data = buffer_reference_uint64_t(config.srcAddress + linearByteOffset + 8).data;
        buffer_reference_uint64_t(config.dstAddress + tiledByteOffset + 16).data = buffer_reference_uint64_t(config.srcAddress + linearByteOffset + 16).data;
        buffer_reference_uint64_t(config.dstAddress + tiledByteOffset + 24).data = buffer_reference_uint64_t(config.srcAddress + linearByteOffset + 24).data;
        break;
    }
}
