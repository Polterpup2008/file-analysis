#pragma once
#include "core.hpp"
namespace fa {
struct SimpleFinding {
 std::string topic, title, value, detail;
};
// A presentation only projection. The Result and full exports retain all evidence.
std::vector<SimpleFinding> simpleFindings(const Result&);
}
