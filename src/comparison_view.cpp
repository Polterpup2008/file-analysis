#include "comparison_view.hpp"
#include "investigation.hpp"
#include <stdexcept>
namespace fa {
namespace {
const Json absent;
std::string summary(const Json& value){
 if(value.is_null())return "Not present";
 if(value.is_array()&&!value.empty()&&value[0].is_object()&&value[0].contains("value")){
  std::string text;if(value.size()>1)text=std::to_string(value.size())+" occurrences: ";size_t n=0;for(const auto& f:value){if(n)text+="; ";if(n++==3){text+="… select for all occurrences";break;}text+=compactValue(f.value("value",Json()),180);}return text;
 }
 if(value.is_object()&&value.contains("sha256")&&value.contains("type"))return value.value("type",std::string("Block"))+" @ "+display(value.value("offset",Json()))+"; "+display(value.value("size",Json()))+" bytes; SHA-256 "+compactValue(value["sha256"],70);
 if(value.is_object()&&value.contains("manifests")&&value["manifests"].is_array())return std::to_string(value["manifests"].size())+" manifest(s); see independent validation and claims in details";
 return compactValue(value,360);
}
const Json& side(const Json& change,const char* key,std::optional<size_t> block){const auto& value=change.at(key);if(!block)return value;return value.is_array()&&*block<value.size()?value[*block]:absent;}
std::string detailed(const Json& value){
 if(value.is_null())return "Not present in this side.\r\n";
 if(value.is_array()&&!value.empty()&&value[0].is_object()&&value[0].contains("value")){
  std::string text;size_t n=0;for(const auto& field:value){text+="Occurrence "+std::to_string(++n)+"\r\nSource: "+display(field.value("source",Json()))+"\r\nValue:\r\n"+field.value("value",Json()).dump(2,' ',false,Json::error_handler_t::replace)+"\r\nRaw / numeric representation:\r\n"+field.value("raw",Json()).dump(2,' ',false,Json::error_handler_t::replace)+"\r\nOffset: "+display(field.value("offset",Json()))+"; size: "+display(field.value("size",Json()))+"\r\nWarning: "+display(field.value("warning",Json()))+"\r\n\r\n";}return text;
 }
 return value.dump(2,' ',false,Json::error_handler_t::replace)+"\r\n";
}
}
ComparisonViewModel comparisonView(const Json& report,std::atomic_bool* cancel){
 ComparisonViewModel model;if(!report.is_object()||!report.contains("changes")||!report["changes"].is_array())return model;
 auto add=[&](size_t index,std::string section,const Json& left,const Json& right,std::optional<size_t> block={}){
  if(cancel&&*cancel)throw std::runtime_error("Comparison presentation cancelled");if(left==right)return;if(model.entries.size()==5000){model.truncated=true;return;}
  std::string kind=left.is_null()?"Added":right.is_null()?"Removed":"Changed";if(kind=="Added")++model.added;else if(kind=="Removed")++model.removed;else ++model.changed;model.entries.push_back({std::move(section),std::move(kind),summary(left),summary(right),index,block});
 };
 const auto& changes=report["changes"];for(size_t n=0;n<changes.size()&&!model.truncated;++n){const auto& change=changes[n];auto section=change.value("section",std::string("Evidence"));const auto& left=change.at("left");const auto& right=change.at("right");
  if(section=="Structure and raw block hashes"&&left.is_array()&&right.is_array())for(size_t b=0;b<std::max(left.size(),right.size())&&!model.truncated;++b)add(n,"Raw block index "+std::to_string(b)+" (positional)",b<left.size()?left[b]:absent,b<right.size()?right[b]:absent,b);
  else add(n,section,left,right);
 }
 return model;
}
std::string comparisonDetail(const Json& report,const ComparisonEntry& entry){
 const auto& change=report.at("changes").at(entry.changeIndex);const auto& left=side(change,"left",entry.blockIndex);const auto& right=side(change,"right",entry.blockIndex);
 return entry.change+": "+entry.section+"\r\n\r\nLEFT — "+report.value("left",std::string())+"\r\n"+detailed(left)+"\r\nRIGHT — "+report.value("right",std::string())+"\r\n"+detailed(right)+"\r\n"+report.value("limitation",std::string())+"\r\nBlock rows compare corresponding indices, not inferred moves or an editing timeline. Full comparison evidence is retained in Export Results.";
}
}
