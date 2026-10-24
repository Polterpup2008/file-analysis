#include "device_recognition.hpp"
#include "device_catalog_snapshot.hpp"
#include <algorithm>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <set>

namespace fa {
namespace {
constexpr size_t maxMatches=64,maxNames=8;
const Json empty=Json::array();
std::string fold(std::string text){
 size_t a=0,b=text.size();
 while(a<b&&(text[a]==' '||text[a]=='\t'||text[a]=='\r'||text[a]=='\n'||text[a]==0))++a;
 while(b>a&&(text[b-1]==' '||text[b-1]=='\t'||text[b-1]=='\r'||text[b-1]=='\n'||text[b-1]==0))--b;
 text=text.substr(a,b-a);for(auto& c:text)if(c>='A'&&c<='Z')c+=('a'-'A');return text;
}
std::string brandKey(std::string text){
 text=fold(std::move(text));
 static const std::map<std::string,std::string> aliases{
  {"samsung electronics","samsung"},{"samsung electronics co., ltd.","samsung"},
  {"lge","lg"},{"lg electronics","lg"},{"nikon corporation","nikon"},
  {"sony corporation","sony"},{"canon inc.","canon"},
  {"pentax corporation","pentax"},{"ricoh imaging company, ltd.","ricoh"},{"ricoh imaging company ltd.","ricoh"},
  {"olympus imaging corp.","olympus"},{"olympus corporation","olympus"},
  {"redmi","xiaomi"},{"poco","xiaomi"},{"mi","xiaomi"}};
 auto it=aliases.find(text);return it==aliases.end()?text:it->second;
}
std::string str(const Json& j,const char* key){auto it=j.find(key);return it!=j.end()&&it->is_string()?it->get<std::string>():"";}
const Json& property(const Json& photo,std::string_view key){
 if(photo.contains("properties"))for(const auto& p:photo["properties"])if(str(p,"key")==key)return p;
 return empty;
}
const Json& sources(const Json& photo,std::string_view key){const auto& p=property(photo,key);return p.is_object()&&p.contains("sources")?p["sources"]:empty;}
std::string group(std::string_view field){return std::string(field.substr(0,field.find_last_of(':')));}
std::string caption(std::string_view brand,std::string_view name){auto label=brandKey(std::string(brand))=="lg"?std::string("LG"):std::string(brand);return fold(std::string(name)).starts_with(fold(label))?std::string(name):label+" "+std::string(name);}
struct Entry{std::string brand,model,name,source;};
struct Catalog{
 std::string status="missing_or_unreadable";
 std::map<std::string,std::vector<Entry>> models;
};
std::shared_ptr<const Catalog> loadCatalog(const fs::path& path){
 // One immutable verified snapshot is shared by all files in a batch.
 static std::mutex mutex;static fs::path cachedPath;static std::shared_ptr<const Catalog> cached;
 std::lock_guard lock(mutex);
 if(cached&&path==cachedPath)return cached;
 auto result=std::make_shared<Catalog>();
 try{
  if(!path.empty()){
   std::ifstream in(path,std::ios::binary|std::ios::ate);
   auto size=in.tellg();
   if(in&&size>0&&size<=4*1024*1024){
    std::string data(static_cast<size_t>(size),'\0');in.seekg(0);in.read(data.data(),size);
    if(!in)result->status="missing_or_unreadable";
    else if(sha256Bytes(std::span(reinterpret_cast<const unsigned char*>(data.data()),data.size()))!=deviceCatalogSha256)result->status="fingerprint_mismatch";
    else{
     result->status="invalid_snapshot";size_t rows=0,start=data.find('\n')+1;
     while(start>0&&start<data.size()){
      auto end=data.find('\n',start);if(end==data.npos)throw std::runtime_error("Catalogue framing");
      std::string_view line(data.data()+start,end-start);std::string cells[4];size_t pos=0;
      for(int i=0;i<4;++i){auto tab=line.find('\t',pos);if((i<3)==(tab==line.npos))throw std::runtime_error("Catalogue columns");auto cell=line.substr(pos,tab==line.npos?line.size()-pos:tab-pos);if(cell.empty()||cell.size()>256||cell.find_first_of("\r\n\0",0,3)!=cell.npos)throw std::runtime_error("Catalogue value");cells[i]=cell;pos=tab+1;}
      if(cells[0]!="google"&&cells[0]!="apple")throw std::runtime_error("Catalogue source");
      result->models[fold(cells[2])].push_back({cells[1],cells[2],cells[3],cells[0]});
      if(++rows>100000)throw std::runtime_error("Catalogue row bound");start=end+1;
     }
     if(rows!=deviceCatalogRows)throw std::runtime_error("Catalogue row count");
     result->status="verified";
    }
   }
  }
 }catch(const std::exception&){result->status="invalid_snapshot";result->models.clear();}
 cachedPath=path;cached=result;return result;
}
Json named(const Entry& entry,std::string match){
 bool apple=entry.source=="apple";
 return {{"brand",entry.brand},{"name",entry.name},{"display_name",caption(entry.brand,entry.name)},
  {"basis",std::move(match)},{"source",apple?"DeviceKit hardware identifier catalogue":"Google Play supported device catalogue"},
  {"reference",apple?"https://github.com/devicekit/DeviceKit/tree/19528aa07e1426626ea9fb2c4a161edf43c4f9e8":"https://storage.googleapis.com/play_public/supported_devices.html"}};
}
std::string vendor(std::string_view field){
 if(field.starts_with("MakerNotes:Canon:"))return "Canon";
 if(field.starts_with("MakerNotes:Sony:"))return "Sony";
 if(field.starts_with("MakerNotes:Pentax:"))return "Pentax";
 if(field.starts_with("MakerNotes:Olympus:"))return "Olympus";
 return {};
}
}

Json recognizeDevices(const Json& photo,const fs::path& path){
 Json result={{"status","not_reported"},{"entries",Json::array()},{"names",Json::array()},{"truncated",false},
  {"catalogue",{{"status","not_needed"},{"snapshot_date",deviceCatalogDate},{"sha256",deviceCatalogSha256},{"records",deviceCatalogRows}}},
  {"limitation","Exact metadata/catalogue matches and reader-decoded model IDs are recorded claims, not proof of a physical device or photographer. No fuzzy, filename, lens, editor or pixel inference. The offline snapshot can omit models; Android catalogue entries also include non-phone devices."}};
 if(!photo.is_object())return result;
 const auto& modelSources=sources(photo,"model");const auto& idSources=sources(photo,"model_id");
 if(modelSources.empty()&&idSources.empty()){
  auto status=str(photo,"device_status");result["status"]=status=="not_evaluated"?"not_evaluated":status=="manufacturer_only"?"manufacturer_only":"not_reported";return result;
 }
 std::shared_ptr<const Catalog> catalog;
 if(!modelSources.empty()){catalog=loadCatalog(path);result["catalogue"]["status"]=catalog->status;}
 std::set<std::string> allMakes;
 for(const auto& s:sources(photo,"make"))if(s["value"].is_string())allMakes.insert(brandKey(s["value"].get<std::string>()));
 std::set<std::string> seenNames;size_t unresolved=0,attempted=0;bool ambiguous=false;
 for(const auto* input:{&modelSources,&idSources})for(const auto& source:*input){
  if(attempted++>=maxMatches){result["truncated"]=true;break;}
  std::string field=str(source,"field");if(field.size()>2048){result["truncated"]=true;++unresolved;continue;}
  std::string value=display(source["value"]),key=fold(value);
  const auto& numeric=source["numeric"];
  bool numericSmall=numeric.is_number()||numeric.is_null()||(numeric.is_string()&&numeric.get_ref<const std::string&>().size()<=256);
  Json item={{"field",field},{"recorded_value",value.size()<=256?source["value"]:Json(nullptr)},{"raw_numeric",numericSmall?numeric:Json(nullptr)},{"status","recorded_only"},{"names",Json::array()}};
  if(value.size()>256||!numericSmall)item["value_note"]="Oversized/structured values are not duplicated into lookup results; original values remain in photo.properties sources and the original metadata field.";
  std::set<std::string> makes;
  for(const auto& s:sources(photo,"make"))if(group(str(s,"field"))==group(field)&&s["value"].is_string())makes.insert(brandKey(s["value"].get<std::string>()));
  if(makes.empty()&&allMakes.size()==1)makes=allMakes;
  bool identifier=input==&idSources;
  if(identifier){
   auto table=vendor(field);
   bool decoded=source["value"].is_string()&&key.size()<=256&&numericSmall&&!numeric.is_null()&&!key.starts_with("unknown")&&key!="n/a"&&key!="none"&&std::any_of(key.begin(),key.end(),[](char c){return c>='a'&&c<='z';})&&key!=fold(display(numeric));
   if(decoded&&!table.empty()){
    // A tag-table namespace is not a manufacturer: e.g. Pentax encodings
    // also cover Ricoh/Samsung bodies. Preserve the decoded string verbatim.
    auto entry=named({"","",value,""},"reader_decoded_model_id");entry["vendor_namespace"]=table;entry["source"]="ExifTool 13.59 vendor model-ID decoding";entry["reference"]="https://exiftool.org/TagNames/"+table+".html";
    item["names"].push_back(entry);item["status"]="matched";
   }else item["status"]="unresolved_model_id";
  }else if(key.size()>256)item["status"]="lookup_value_limit";
  else if(catalog->status!="verified")item["status"]="catalogue_unavailable";
  else{
   auto found=catalog->models.find(key);
   if(found!=catalog->models.end()){
    std::set<std::string> distinct;
    for(const auto& entry:found->second){
     if(!makes.empty()&&!makes.contains(brandKey(entry.brand)))continue;
     auto identity=brandKey(entry.brand)+"\t"+fold(entry.name);
     if(!distinct.insert(identity).second)continue;
     if(item["names"].size()==maxNames){result["truncated"]=true;break;}
     item["names"].push_back(named(entry,makes.empty()?"exact_model_without_manufacturer":"exact_manufacturer_and_model"));
    }
    item["status"]=item["names"].empty()?"manufacturer_mismatch":item["names"].size()==1?"matched":"multiple_matches";
   }else item["status"]="not_in_catalogue";
  }
  if(item["names"].empty())++unresolved;
  if(item["names"].size()>1)ambiguous=true;
  for(const auto& name:item["names"]){auto identity=fold(str(name,"display_name"));if(seenNames.insert(identity).second)result["names"].push_back(name);}
  result["entries"].push_back(std::move(item));
 }
 auto size=result["names"].size();
 if(ambiguous||size>1)result["status"]="multiple_matches";
 else if(size)result["status"]=unresolved||result["truncated"]==true?"partial_match":"matched";
 else result["status"]="recorded_only";
 return result;
}

Json deviceNamesForSharing(const Json& recognition){
 Json names=Json::array();if(!recognition.is_object()||!recognition.contains("names"))return names;
 for(const auto& name:recognition["names"])names.push_back({{"brand",str(name,"brand")},{"name",str(name,"name")},{"basis",str(name,"basis")}});
 return names;
}
}
