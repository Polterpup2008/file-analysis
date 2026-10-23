#include "core.hpp"
#include <algorithm>
#include <cctype>
#include <map>
#include <set>

namespace fa {
namespace {
struct Property { const char* key; const char* title; const char* tags; const char* note; bool identity; };
constexpr Property properties[]{
 {"make","Device manufacturer","make|androidmake","",true},
 {"model","Device model","model|androidmodel","The recorded model is shown as stored; internal model codes are not guessed or expanded.",true},
 {"lens_make","Lens manufacturer","lensmake","",true},
 {"lens_model","Lens model","lensmodel|lens","Lens names are recorded claims; they do not independently identify the camera body.",true},
 {"capture_time","Capture time recorded","datetimeoriginal","This is an embedded capture-time claim, not a filesystem timestamp. No timezone is assumed when none is recorded.",false},
 {"capture_offset","Capture timezone offset","offsettimeoriginal","Offset for DateTimeOriginal; no timezone is inferred from GPS or the computer clock.",false},
 {"capture_subseconds","Capture fractional seconds","subsectimeoriginal","Fractional part of the recorded capture time.",false},
 {"exposure","Exposure time","exposuretime","Exposure duration in seconds (s).",false},
 {"aperture","Aperture","fnumber","Lens f-number.",false},
 {"iso","ISO","iso|photographicsensitivity","Recorded sensitivity setting.",false},
 {"focal_length","Focal length","focallength","Recorded lens focal length.",false},
 {"focal_length_35mm","35 mm equivalent focal length","focallengthin35mmformat|focallengthin35mmfilm","Recorded 35 mm equivalent, not a sensor-size inference.",false},
 {"flash","Flash","flash","Recorded flash state.",false},
 {"exposure_program","Exposure program","exposureprogram","Recorded shooting program.",false},
 {"exposure_mode","Exposure mode","exposuremode","Recorded exposure control mode.",false},
 {"white_balance","White balance","whitebalance","Recorded white-balance mode.",false},
 {"body_serial","Camera serial number","serialnumber|bodyserialnumber|cameraserialnumber","Device identifier; may be personal information.",true},
 {"lens_serial","Lens serial number","lensserialnumber","Lens identifier; may be personal information.",true},
 {"latitude","GPS latitude","gpslatitude","Recorded coordinate; reference/direction is retained separately. No location lookup is performed.",false},
 {"latitude_ref","GPS latitude reference","gpslatituderef","North/south reference for the latitude.",false},
 {"longitude","GPS longitude","gpslongitude","Recorded coordinate; reference/direction is retained separately. No location lookup is performed.",false},
 {"longitude_ref","GPS longitude reference","gpslongituderef","East/west reference for the longitude.",false},
 {"altitude","GPS altitude","gpsaltitude","Recorded altitude; its reference is retained separately.",false},
 {"altitude_ref","GPS altitude reference","gpsaltituderef","Reference for the recorded altitude.",false}
};
std::string lower(std::string s) { for(auto& c:s)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));return s; }
std::string trimmed(std::string s) {
 auto white=[](unsigned char c){return c==0||std::isspace(c);};
 while(!s.empty()&&white(static_cast<unsigned char>(s.back())))s.pop_back();
 size_t start=0;while(start<s.size()&&white(static_cast<unsigned char>(s[start])))++start;return s.substr(start);
}
bool listed(std::string_view list,std::string_view name) { return ("|"+std::string(list)+"|").find("|"+std::string(name)+"|")!=std::string::npos; }
std::vector<std::string> parts(const std::string& name) {
 std::vector<std::string> out;size_t start=0;
 for(size_t end=0;end<=name.size();++end)if(end==name.size()||name[end]==':'){out.push_back(name.substr(start,end-start));start=end+1;}
 return out;
}
// Never promote an ICC profile's Model, a generation Model, scanner values,
// thumbnails or another embedded document into the main asset's capture device.
bool sourceAllowed(const Field& f,const Property& p,const std::vector<std::string>& path) {
 if(f.source!="ExifTool"||path.size()<2)return false;
 for(const auto& component:path) {
  if(component=="IFD1"||component.find("-IFD1")!=std::string::npos)return false;
  if(component.starts_with("Doc")&&component.size()>3&&std::isdigit(static_cast<unsigned char>(component[3])))return false;
 }
 auto family=path[0];
 if(family=="EXIF")return true;
 if(family=="XMP")return path.size()>2&&listed("XMP-tiff|XMP-exif|XMP-exifEX|XMP-aux",path[1]);
 if(family=="MakerNotes")return p.identity; // Vendor exposure encodings can differ from EXIF.
 if(family=="QuickTime")return listed("make|model",p.key)&&path.size()>2&&listed("Keys|UserData|ItemList",path[1]);
 return false;
}
bool image(const Result& r) {
 if(std::set<std::string>{"PNG","JPEG","TIFF","WebP","GIF","BMP","HEIF container"}.contains(r.format))return true;
 for(const auto& f:r.fields)if(f.name.starts_with("File:")&&f.name.ends_with(":MIMEType")&&f.value.is_string()&&f.value.get_ref<const std::string&>().starts_with("image/"))return true;
 return false;
}
}
void summarizePhoto(Result& r) {
 // Safe to refresh the projection without duplicating its own derived fields.
 std::erase_if(r.fields,[](const Field& f){return f.category=="Photo / device";});
 const std::string limitation="Embedded camera, time and GPS tags can be edited, copied or forged. They do not prove which physical device captured this file, who took it, or that it is a photograph. Missing tags do not establish AI generation. No device is inferred from pixels, filenames, software names or a lens alone.";
 Json found=Json::array(),warnings=Json::array();
 bool make=false,model=false;
 for(const auto& p:properties) {
  Json values=Json::array(),sources=Json::array();std::set<std::string> distinct;
  for(const auto& f:r.fields) {
   auto path=parts(f.name);if(!listed(p.tags,lower(path.back()))||!sourceAllowed(f,p,path))continue;
   if(!f.value.is_string()&&!f.value.is_number())continue;
   auto text=trimmed(display(f.value));auto normalized=lower(text);
   if(text.empty()||listed("unknown|n/a|none|not defined|unspecified|undefined",normalized))continue;
   // Zero is a real GPS coordinate and a valid enum; retain it.
   if(distinct.insert(normalized).second)values.push_back(text);
   sources.push_back({{"field",f.name},{"reader",f.source},{"value",f.value},{"numeric",f.raw},{"warning",f.warning}});
  }
  if(values.empty())continue;
  bool conflict=p.identity&&values.size()>1;
  std::string warning=conflict?"Different recorded "+lower(p.title)+" values; all sources retained. This alone does not prove tampering.":"";
  if(conflict)warnings.push_back(warning);
  found.push_back({{"key",p.key},{"title",p.title},{"values",values},{"sources",sources},{"conflict",conflict},{"warning",warning},{"note",p.note}});
  if(std::string_view(p.key)=="make")make=true;if(std::string_view(p.key)=="model")model=true;
 }
 if(!image(r)&&found.empty()){r.photo=nullptr;return;}
 bool extracted=metadataReadSucceeded(r);
 r.photo={{"device_status",model?"model_reported":make?"manufacturer_only":extracted?"not_reported":"not_evaluated"},{"metadata_reader_available",extracted},{"properties",found},{"warnings",warnings},{"limitation",limitation}};
 for(const auto& item:found) {
  std::string value;for(const auto& v:item["values"]){if(!value.empty())value+=" / ";value+=v.get<std::string>();}
  auto note=item["note"].get<std::string>();auto warning=item["warning"].get<std::string>();
  r.fields.push_back({"Photo / device",item["title"],"Capture metadata summary",warning.empty()?note:warning+" "+note,value,item["sources"],{},{}});
 }
 std::string status=model?"Device model reported in embedded metadata":make?"Manufacturer reported; device model not recovered":extracted?"No device make or model recovered":"Not evaluated: metadata reader unavailable or unsuccessful";
 r.fields.push_back({"Photo / device","Device identification","Capture metadata summary",limitation,status,nullptr,{},{}});
}
}
