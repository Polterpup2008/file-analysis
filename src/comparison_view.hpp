#pragma once
#include "core.hpp"
namespace fa {
struct ComparisonEntry {std::string section,change,left,right;size_t changeIndex=0;std::optional<size_t> blockIndex;};
struct ComparisonViewModel {std::vector<ComparisonEntry> entries;size_t added=0,removed=0,changed=0;bool truncated=false;};
ComparisonViewModel comparisonView(const Json&,std::atomic_bool* cancel=nullptr);
std::string comparisonDetail(const Json&,const ComparisonEntry&);
}
