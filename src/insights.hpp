#pragma once
#include "core.hpp"
namespace fa {
struct ShareOptions { bool omitLocation=true,omitIdentifiers=true,omitPeople=true; };
void summarizeInsights(Result&);
Json shareReport(const std::vector<Result>&,const ShareOptions&,std::string_view version,size_t batchErrorCount=0,bool cancelled=false);
}
