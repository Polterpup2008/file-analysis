#include "investigation.hpp"
#include <algorithm>
#include <stdexcept>
namespace fa {
namespace {
std::string excerpt(std::wstring_view text,size_t start,size_t length){
 size_t a=start>70?start-70:0,b=std::min(text.size(),std::max(start+length,a+250));b=std::min(b,a+320);
 if(a&&text[a]>=0xdc00&&text[a]<=0xdfff)--a;if(b<text.size()&&b&&text[b-1]>=0xd800&&text[b-1]<=0xdbff)++b;
 return (a?"…":"")+utf8(text.substr(a,b-a))+(b<text.size()?"…":"");
}
struct Matcher {
 std::wstring query;std::atomic_bool& cancel;
 std::optional<std::string> text(std::string_view value){
  if(query.empty())return compactValue(std::string(value.substr(0,1024)),300);
  // Chunk conversion/search so a long metadata string has cancellation points.
  constexpr size_t chunk=65536,overlap=4096;size_t first=0;
  while(first<value.size()&&!cancel){size_t end=std::min(value.size(),first+chunk);while(end<value.size()&&(static_cast<unsigned char>(value[end])&0xc0)==0x80)++end;
   auto part=wide(value.substr(first,end-first));int length=0;SetLastError(ERROR_SUCCESS);int at=FindNLSStringEx(LOCALE_NAME_INVARIANT,FIND_FROMSTART|NORM_IGNORECASE,part.data(),static_cast<int>(part.size()),query.data(),static_cast<int>(query.size()),&length,nullptr,nullptr,0);
   if(at>=0)return (first?"…":"")+excerpt(part,static_cast<size_t>(at),static_cast<size_t>(length))+(end<value.size()?"…":"");
   if(GetLastError()!=ERROR_SUCCESS)throw std::runtime_error("Windows Unicode search failed");if(end==value.size())break;first=end-overlap;while(first<end&&(static_cast<unsigned char>(value[first])&0xc0)==0x80)++first;
  }return {};
 }
 std::optional<std::string> json(const Json& value){
  if(cancel)return {};if(value.is_string())return text(value.get_ref<const std::string&>());
  if(value.is_structured()){for(auto it=value.begin();it!=value.end();++it){if(value.is_object())if(auto match=text(it.key()))return it.key().substr(0,100)+": "+*match;if(auto match=json(it.value()))return match;if(cancel)break;}return {};}
  return text(display(value));
 }
};
}
std::string compactText(std::string_view value,size_t limit){if(value.size()<=limit)return std::string(value);size_t end=limit;while(end&&(static_cast<unsigned char>(value[end])&0xc0)==0x80)--end;return std::string(value.substr(0,end))+"… [select for full value]";}
std::string compactValue(const Json& value,size_t limit){
 if(value.is_structured())return std::string(value.is_array()?"Array (":"Object (")+std::to_string(value.size())+" entries); select for full value";
 return value.is_string()?compactText(value.get_ref<const std::string&>(),limit):display(value);
}
SearchResults searchBatch(const std::vector<Result>& files,std::wstring_view query,std::string_view category,std::atomic_bool& cancel,size_t limit){
 if(query.size()>512)throw std::runtime_error("Batch search query is limited to 512 characters");if(!limit||limit>5000)throw std::runtime_error("Batch search result limit must be 1 to 5000");
 SearchResults out;Matcher matcher{std::wstring(query),cancel};
 auto add=[&](size_t file,MatchKind kind,size_t index,std::string_view cat,std::string_view name,const Json& value,const Json& raw,std::string_view source,std::string_view warning){
  if(cancel||(!category.empty()&&category!=cat))return;
  auto match=matcher.json(value);if(!match&&!raw.is_null())match=matcher.json(raw);if(!match)match=matcher.text(name);if(!match)match=matcher.text(source);if(!match)match=matcher.text(warning);if(!match)return;
  if(out.matches.size()==limit){out.truncated=true;return;}out.matches.push_back({file,index,kind,std::string(cat.substr(0,100)),std::string(name.substr(0,300)),std::move(*match),std::string(source.substr(0,200))});
 };
 for(size_t i=0;i<files.size()&&!cancel&&!out.truncated;++i){const auto& file=files[i];
  add(i,MatchKind::File,0,"File","File name",utf8(file.path.filename().wstring()),nullptr,"File identity","");
  for(size_t n=0;n<file.fields.size()&&!cancel&&!out.truncated;++n){const auto& f=file.fields[n];add(i,MatchKind::Field,n,f.category,f.name,f.value,f.raw,f.source,f.warning);}
  for(size_t n=0;n<file.generation.size()&&!cancel&&!out.truncated;++n){const auto& g=file.generation[n];add(i,MatchKind::Generation,n,"Generation",g.value("name",std::string("Generation")),g.value("value",Json()),nullptr,g.value("source_field",std::string()),"");}
  for(size_t n=0;n<file.errors.size()&&!cancel&&!out.truncated;++n)add(i,MatchKind::Error,n,"Error","Analysis finding",file.errors[n],nullptr,"Analysis","");
  for(size_t n=0;n<file.previews.size()&&!cancel&&!out.truncated;++n){const auto& p=file.previews[n];add(i,MatchKind::Preview,n,"Embedded preview",p.field,p.format+" "+p.sha256,nullptr,"ExifTool binary extraction",p.warning);}
  if(!cancel&&!out.truncated)++out.scannedFiles;
 }
 out.cancelled=cancel.load();return out;
}
}
