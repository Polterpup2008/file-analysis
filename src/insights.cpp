#include "insights.hpp"
#include <algorithm>
#include <cctype>
#include <set>

namespace fa {
namespace {
const Json emptyObject=Json::object(),emptyArray=Json::array();
const Json& member(const Json& j,const char* key,const Json& fallback=emptyObject){auto it=j.is_object()?j.find(key):j.end();return it!=j.end()?*it:fallback;}
std::string str(const Json& j,const char* key){const auto& v=member(j,key);return v.is_string()?v.get<std::string>():"";}
std::string tail(std::string s){auto pos=s.find_last_of(':');if(pos!=s.npos)s.erase(0,pos+1);for(auto& c:s)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));return s;}
bool oneOf(std::string_view value,std::initializer_list<const char*> names){for(auto n:names)if(value==n)return true;return false;}
bool usable(const Field& f){return !f.value.is_null()&&f.value!=Json("")&&f.category!="Analysis diagnostics"&&f.category!="Filesystem"&&f.category!="File"&&f.category!="Photo / device"&&f.category!="Origin summary"&&f.category!="Privacy findings"&&f.category!="Limitations";}
std::string privacyKind(const Field& f){
 auto name=tail(f.name);
 if(name.find("gps")!=name.npos||name.find("latitude")!=name.npos||name.find("longitude")!=name.npos||name.find("location")!=name.npos||oneOf(name,{"city","country","countrycode","sublocation","provincestate","state","address"}))return "location";
 if(name.find("serialnumber")!=name.npos||oneOf(name,{"deviceid","cameraid","imageuniqueid","documentid","instanceid","originaldocumentid"}))return "identifiers";
 if(name.find("email")!=name.npos||name.find("contact")!=name.npos||name.find("phone")!=name.npos||oneOf(name,{"artist","author","creator","ownername","cameraownername","by-line","byline","copyright","rights","personinimage"}))return "people";
 if(oneOf(name,{"thumbnailimage","previewimage","jpgfromraw","otherimage"}))return "previews";
 if(oneOf(name,{"comment","usercomment","description","imagedescription","title","prompt","workflow","parameters","instructions"}))return "free_text";
 return {};
}
const char* kindTitle(std::string_view kind){return kind=="location"?"Location metadata":kind=="identifiers"?"Device / document identifiers":kind=="people"?"Names / contact / rights claims":kind=="previews"?"Embedded preview tags":"Free text to review";}
bool omitted(std::string_view kind,const ShareOptions& o){return (kind=="location"&&o.omitLocation)||(kind=="identifiers"&&o.omitIdentifiers)||(kind=="people"&&o.omitPeople);}
Json exportedField(std::string category,std::string name,Json value,std::string warning={}){return {{"category",category},{"name",name},{"value",value},{"source","Share summary"},{"raw",nullptr},{"offset",nullptr},{"size",nullptr},{"warning",warning}};}
}
void summarizeInsights(Result& r){
 std::erase_if(r.fields,[](const Field& f){return f.category=="Origin summary"||f.category=="Privacy findings";});
 bool camera=oneOf(str(r.photo,"device_status"),{"model_reported","manufacturer_only"});
 const auto& ai=member(r.provenance,"ai_claims",emptyArray);bool aiClaim=ai.is_array()&&!ai.empty(),parameters=!r.generation.empty();
 Json signals=Json::array();if(camera)signals.push_back("capture_device_tags");if(aiClaim)signals.push_back("ai_provenance_claim");if(parameters)signals.push_back("generation_parameters");
 std::string assessment=camera&&(aiClaim||parameters)?"Camera and generation metadata present":aiClaim?"AI involvement recorded in provenance":parameters?"Generation settings recovered":camera?"Capture device recorded in metadata":"Origin not established";
 const std::string limitation="These are recorded metadata signals, not a visual detector or proof of the original device, photographer or complete editing history. Camera and generation metadata can coexist. Metadata can be copied or forged; absent tags do not establish human or AI origin. Signature integrity and signer trust are reported separately.";
 Json software=Json::array();std::set<std::string> seen;
 for(const auto& f:r.fields){if(!usable(f)||f.category=="Derived values"||f.category=="Provenance"||!f.value.is_string())continue;auto name=tail(f.name);if(!oneOf(name,{"software","creatortool","processingsoftware","historysoftwareagent"}))continue;auto value=f.value.get<std::string>();if(!value.empty()&&seen.insert(value).second)software.push_back({{"name",value},{"field",f.name},{"source",f.source}});}
 r.origin={{"assessment",assessment},{"signals",signals},{"software_claims",software},{"limitation",limitation}};
 r.fields.push_back({"Origin summary","Recorded origin signals","Analysis",limitation,assessment,signals,{},{}});
 Json items=Json::array(),counts={{"location",0},{"identifiers",0},{"people",0},{"previews",0},{"free_text",0}};
 for(const auto& f:r.fields){if(!usable(f))continue;auto kind=privacyKind(f);if(kind.empty())continue;items.push_back({{"kind",kind},{"title",kindTitle(kind)},{"field",f.name},{"value",f.value},{"source",f.source}});counts[kind]=counts[kind].get<size_t>()+1;}
 if(parameters&&counts["free_text"]==0)counts["free_text"]=r.generation.size();
 r.privacy={{"counts",counts},{"findings",items},{"limitation","A selected-field privacy review, not an exhaustive personal-data detector. Raw text, embedded previews and unknown data can contain additional personal information. Local-path redaction alone does not remove embedded GPS, names or identifiers."}};
 for(auto it=counts.begin();it!=counts.end();++it)if(it.value()!=0)r.fields.push_back({"Privacy findings",kindTitle(it.key()),"Analysis",r.privacy["limitation"],it.value(),nullptr,{},{}});
}
Json shareReport(const std::vector<Result>& results,const ShareOptions& options,std::string_view version,size_t batchErrorCount,bool cancelled){
 const std::string limitation="This is a selected-field summary, not a full forensic report or guaranteed anonymization. Source paths/names, raw parser data, binary blocks, full provenance, prompts, workflows, free text and raw error messages are omitted. Remaining camera, lens, date, software or other retained values may still contain personal information; review before sharing. Originals are never modified.";
 Json policy={{"omit_location",options.omitLocation},{"omit_identifiers",options.omitIdentifiers},{"omit_people",options.omitPeople}};
 Json report={{"application","File Analysis"},{"version",std::string(version)},{"schema_version",4},{"report_mode","share_summary"},{"redaction_policy",policy},{"limitation",limitation},{"files",Json::array()},{"batch_errors",Json::array()},{"batch_error_count",batchErrorCount},{"cancelled",cancelled}};
 for(size_t i=0;i<results.size();++i){const auto& r=results[i];Json fields=Json::array();auto add=[&](std::string category,std::string name,Json value,std::string warning={}){fields.push_back(exportedField(std::move(category),std::move(name),std::move(value),std::move(warning)));};
  add("Origin summary","Recorded origin signals",str(r.origin,"assessment"),str(r.origin,"limitation"));
  add("Sharing","Redaction policy",policy,limitation);add("Analysis","Error count (details omitted)",r.errors.size(),"Error text is excluded because it can contain paths or personal metadata. Use the full local report to inspect errors.");
  for(const auto& f:r.fields)if(f.category=="Hashes"&&f.name=="SHA-256")add("Hashes","SHA-256",f.value,"A file fingerprint can link this report to the original file; it is not anonymization.");
  Json capture=Json::array();for(const auto& p:member(r.photo,"properties",emptyArray)){auto key=str(p,"key");bool location=oneOf(key,{"latitude","latitude_ref","longitude","longitude_ref","altitude","altitude_ref"}),id=oneOf(key,{"body_serial","lens_serial"});bool ordinary=oneOf(key,{"make","model","lens_make","lens_model","capture_time","capture_offset","capture_subseconds","exposure","aperture","iso","focal_length","focal_length_35mm","flash","exposure_program","exposure_mode","white_balance"});if(!ordinary&&!(location&&!options.omitLocation)&&!(id&&!options.omitIdentifiers))continue;
   Json item={{"key",key},{"title",member(p,"title")},{"values",member(p,"values",emptyArray)},{"conflict",member(p,"conflict")},{"warning",member(p,"warning")},{"note",member(p,"note")}};capture.push_back(item);add("Photo / device",str(p,"title"),member(p,"values",emptyArray),str(p,"note"));
  }
  // Explicitly allowed metadata only. Whole parser objects and raw fields are never copied.
  Json included=Json::array();for(const auto& f:member(r.privacy,"findings",emptyArray)){auto kind=str(f,"kind");if(!oneOf(kind,{"location","identifiers","people"})||omitted(kind,options))continue;included.push_back(f);add("Included metadata",str(f,"field"),member(f,"value"),"Included by the selected export policy; review before sharing.");}
  for(const auto& s:member(r.origin,"software_claims",emptyArray))add("Software claim","Recorded software",member(s,"name"),"An editable software name is not proof of an editing action or identity.");
  const auto& verification=member(r.provenance,"verification");Json provenance={{"detected",member(r.provenance,"detected")==true},{"verification_performed",member(verification,"performed")==true},{"ai_claims_recorded",!member(r.provenance,"ai_claims",emptyArray).empty()},{"signature_integrity",str(verification,"signature_integrity")},{"content_binding",str(verification,"content_binding")},{"certificate_trust",str(verification,"certificate_trust")}};
  // Do not reproduce unperformed success labels from an imported/partial object.
  if(provenance["verification_performed"]!=true){provenance["signature_integrity"]="not_verified";provenance["content_binding"]="not_verified";provenance["certificate_trust"]="not_evaluated";}
  add("Privacy findings","Detected metadata categories",member(r.privacy,"counts"),str(r.privacy,"limitation"));add("Generation","Generation settings present",!r.generation.empty(),"Prompt/workflow contents are intentionally omitted from share summaries.");
  report["files"].push_back({{"path","File "+std::to_string(i+1)},{"format_signature",r.format},{"status",r.status},{"size",r.size},{"fields",fields},{"blocks",Json::array()},{"errors",Json::array()},{"error_count",r.errors.size()},{"generation",Json::array()},{"photo",capture},{"provenance",provenance},{"included_personal_metadata",included}});
 }
 return report;
}
}
