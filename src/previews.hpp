#pragma once
#include "core.hpp"
namespace fa {
struct RasterPixels {uint32_t width=0,height=0;std::vector<unsigned char> bgra;};
std::vector<unsigned char> decodeBase64(std::string_view,size_t byteLimit);
void analyzePreviews(Result&,const Options&,std::atomic_bool&);
RasterPixels decodePreview(std::span<const unsigned char>);
}
