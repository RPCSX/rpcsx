#version 460

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

#include "tiler.glsl"

void main() {
    uvec3 pos = gl_GlobalInvocationID;
    uint64_t tiledSliceOffset = 0;
    uint64_t linearSliceOffset = 0;
    if (config.tiledSurfaceSize != 0) {
        tiledSliceOffset = pos.z * config.tiledSurfaceSize;
        linearSliceOffset = pos.z * config.linearSurfaceSize;
        pos.z = 0;
    }

    uint64_t tiledByteOffset = getTiledBitOffset1D(
        config.tileMode,
        pos,
        config.dataSize,
        config.bitsPerElement
    ) / 8;

    tiledByteOffset += tiledSliceOffset;

    uint64_t linearByteOffset = computeLinearElementByteOffset(
        pos,
        0,
        config.dataSize.x,
        config.dataSize.x * config.dataSize.y,
        config.bitsPerElement,
        1 << config.numFragments
    );

    linearByteOffset += linearSliceOffset;

    switch ((config.bitsPerElement + 7) / 8) {
    case 1:
        buffer_reference_uint8_t(config.dstAddress + tiledByteOffset).data = buffer_reference_uint8_t(config.srcAddress + linearByteOffset).data;
        break;

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
