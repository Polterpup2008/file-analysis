#pragma once
#include <array>
#include <string_view>
namespace fa {
struct PngChunkDefinition { std::string_view type, description, category; };
inline constexpr PngChunkDefinition pngChunkRegistry[]{
 PngChunkDefinition{"IHDR","Image header","Image"}, {"IDAT","Encoded image data","Image"},
 {"IEND","Image end","Image"}, {"PLTE","Palette","Image"},
 {"tEXt","Latin-1 text","PNG text"}, {"iTXt","International text","PNG text"},
 {"zTXt","Compressed text","PNG text"}, {"eXIf","EXIF metadata","EXIF"},
 {"iCCP","ICC profile","ICC"}, {"caBX","C2PA Content Credentials (JUMBF manifest store)","Provenance"},
 {"sRGB","sRGB color space","Image"}, {"gAMA","Gamma","Image"}, {"cHRM","Chromaticity","Image"},
 {"tRNS","Transparency","Image"}, {"bKGD","Background","Image"}, {"pHYs","Pixel dimensions","Image"},
 {"tIME","Image timestamp","Metadata"}, {"hIST","Histogram","Image"}, {"sPLT","Suggested palette","Image"},
 {"sBIT","Significant bits","Image"}, {"acTL","Animation control","Image"}, {"fcTL","Frame control","Image"},
 {"fdAT","Frame data","Image"}, {"cICP","Color information","Image"}, {"mDCV","Mastering display","Image"},
 {"cLLI","Content light level","Image"}
};
inline const PngChunkDefinition* pngChunk(std::string_view type) {
 for(const auto& item:pngChunkRegistry)if(item.type==type)return &item;return nullptr;
}
// The native structure scanner and parser-field classifier use these same definitions.
inline std::string_view parserNamespaceCategory(std::string_view family) {
 if(family=="JUMBF")return pngChunk("caBX")->category;
 if(family=="PNG")return "PNG";
 if(family=="EXIF")return pngChunk("eXIf")->category;
 if(family=="ICC_Profile"||family=="ICC")return pngChunk("iCCP")->category;
 if(family=="XMP"||family=="IPTC")return family;
 if(family=="ExifTool")return "Analysis diagnostics";
 if(family=="Composite")return "Derived values";
 return "Metadata";
}
}
