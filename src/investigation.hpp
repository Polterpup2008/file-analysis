#pragma once
#include "core.hpp"
namespace fa {
enum class MatchKind {File,Field,Generation,Error,Preview};
struct SearchMatch {size_t file=0,index=0;MatchKind kind=MatchKind::File;std::string category,name,snippet,source;};
struct SearchResults {std::vector<SearchMatch> matches;size_t scannedFiles=0;bool cancelled=false,truncated=false;};
// Searches retained evidence only. No file reads, network access or rescanning.
SearchResults searchBatch(const std::vector<Result>&,std::wstring_view query,std::string_view category,std::atomic_bool&,size_t limit=5000);
std::string compactValue(const Json&,size_t limit=400);
std::string compactText(std::string_view,size_t limit=400);
}
