#ifndef INC_DDS_H_
#define INC_DDS_H_
#include <cstdint>

enum PixelFormat
{
    DDS_FORMAT_R16G16B16A16_FLOAT = 0,
    DDS_FORMAT_R32G32B32A32_FLOAT = 1
};

bool save_dds(const char* path, PixelFormat format, uint32_t texelSizeInBytes, uint32_t width, uint32_t height, const void* data);
#endif
