#pragma once
#include <windows.h>
#include <atomic>
#include <filesystem>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>
#include "json.hpp"
namespace fa {
using Json = nlohmann::ordered_json;
namespace fs = std::filesystem;
struct Handle {
 HANDLE value = INVALID_HANDLE_VALUE;
 explicit Handle(HANDLE h = INVALID_HANDLE_VALUE) : value(h) {}
 ~Handle() { if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value); }
 Handle(const Handle&) = delete; Handle& operator=(const Handle&) = delete;
 operator HANDLE() const { return value; }
};
struct Field {
 std::string category, name, source, warning;
 Json value, raw;
 std::optional<uint64_t> offset, size;
};
struct Block { std::string type, note, sha256; uint64_t offset=0, size=0; };
struct PreviewResource {std::string field,format,sha256,warning;std::vector<unsigned char> bytes;uint32_t width=0,height=0;bool viewable=false;};
struct Result {
 fs::path path; std::string format="Unknown", status="complete";
 uint64_t size=0;
 std::vector<Field> fields;
 std::vector<Block> blocks;
 std::vector<PreviewResource> previews;
 std::string previewStatus="not_evaluated";
 std::vector<std::string> errors;
 Json parser, parserNumeric, generation=Json::array(), provenance, c2paReport, photo, origin, privacy;
 Json json() const;
};
struct Options { fs::path exiftool, c2patool; DWORD parserTimeoutMs=20000; bool useParser=true, useC2pa=true; };
struct ProcessResult { std::string out, err; DWORD exitCode=0; };
std::string utf8(std::wstring_view s);
std::wstring wide(std::string_view s);
std::string display(const Json& j);
std::string sha256File(const fs::path&, std::atomic_bool&);
std::string sha256Bytes(std::span<const unsigned char>);
size_t previewBytes(const Result&);
void extractPreview(const Result&,size_t,const fs::path&,std::atomic_bool* cancel=nullptr);
ProcessResult runParser(const fs::path&, const fs::path&, bool numeric, std::atomic_bool&, DWORD timeout);
ProcessResult runPreviewParser(const fs::path&,const fs::path&,std::atomic_bool&,DWORD timeout,bool recoverMalformed=false);
ProcessResult runC2pa(const fs::path&, const fs::path&, std::atomic_bool&, DWORD timeout);
Json boundedJson(std::string_view text, size_t maxDepth=128, size_t maxNodes=250000);
std::string metadataCategory(std::string_view key);
void analyzeProvenance(Result&, const Options&, std::atomic_bool&);
void summarizeProvenance(Result&);
Json structureGroups(const Result&, bool groupIdat=true);
Json redactLocalPaths(const Json&, const std::vector<fs::path>&);
Result analyze(const fs::path&, const Options&, std::atomic_bool&, const std::function<void(unsigned)>& progress={});
void parseGeneration(Result&);
void summarizePhoto(Result&);
Json compare(const Result&, const Result&, std::atomic_bool* cancel=nullptr);
std::vector<fs::path> enumerate(const fs::path&, bool recursive, std::atomic_bool&, std::vector<std::string>& errors);
void exportReport(const fs::path&, const Json&, const std::string& format, const std::vector<fs::path>& originals={}, std::atomic_bool* cancel=nullptr);
std::string hexPage(const fs::path&, uint64_t offset, size_t count=512);
void extractBlock(const fs::path&, const Block&, const fs::path&, std::atomic_bool* cancel=nullptr);
}
