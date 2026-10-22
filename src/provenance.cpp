#include "core.hpp"
#include "format_registry.hpp"
#include <algorithm>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
namespace fa {
namespace {
std::string stringAt(const Json& j,std::string_view key) {
 auto it=j.is_object()?j.find(std::string(key)):j.end();return it!=j.end()&&it->is_string()?it->get<std::string>():"";
}
std::string lastPart(std::string_view key) {auto pos=key.rfind(':');return std::string(pos==key.npos?key:key.substr(pos+1));}
std::string docPart(std::string_view key) {
 while(!key.empty()){auto pos=key.find(':');auto token=key.substr(0,pos);if(token.starts_with("Doc")&&token.size()>3&&token.find_first_not_of("0123456789-",3)==token.npos)return std::string(token);if(pos==key.npos)break;key.remove_prefix(pos+1);}return {};
}
std::string manifestFromUri(const Json& value) {
 auto uri=value.is_string()?value.get<std::string>():stringAt(value,"url");auto pos=uri.find("/c2pa/");if(pos!=uri.npos){uri=uri.substr(pos+6);auto slash=uri.find('/');if(slash!=uri.npos)uri.resize(slash);return uri;}return uri.starts_with("urn:")?uri:"";
}
void addField(Result& r,const std::string& category,const std::string& name,Json value,const std::string& source,const std::string& warning={}) {
 r.fields.push_back({category,name,source,warning,std::move(value),nullptr,{},{}});
}
Json manifest(std::string id,std::string source) {
 return {{"id",id},{"source",source},{"claim_generator_info",Json::array()},{"actions",Json::array()},{"ingredients",Json::array()},{"signature_info",nullptr},{"independent_validation","Not independently revalidated; signature details and historical validation records are embedded claims."}};
}
Json exifManifests(const Result& r) {
 std::map<std::string,std::map<std::string,Json>> nodes;
 if(r.parser.is_object())for(auto it=r.parser.begin();it!=r.parser.end();++it)if(it.key().starts_with("JUMBF:")){auto doc=docPart(it.key());if(!doc.empty())nodes[doc][lastPart(it.key())]=it.value();}
 std::map<std::string,std::string> labels;
 for(auto& [doc,node]:nodes)if(node.contains("JUMDType")&&node["JUMDType"].is_string()&&node["JUMDType"].get<std::string>().starts_with("(c2ma)")&&node.contains("JUMDLabel")&&node["JUMDLabel"].is_string())labels[doc]=node["JUMDLabel"].get<std::string>();
 if(labels.size()>256)throw std::runtime_error("C2PA manifest interpretation limit (256) exceeded; raw parser data retained");
 std::map<std::string,std::string> owners;
 for(auto& [doc,node]:nodes){auto parent=doc;while(parent.find('-')!=parent.npos){parent.resize(parent.rfind('-'));if(labels.contains(parent)){owners[doc]=parent;break;}}}
 Json out=Json::array();
 for(auto& [root,id]:labels){auto m=manifest(id,"ExifTool JUMBF document "+root);
  for(auto& [doc,node]:nodes)if(owners.contains(doc)&&owners[doc]==root){
   // ExifTool's document groups associate assertions with their containing manifest.
   if(node.contains("Claim_generator_info")){auto& v=node["Claim_generator_info"];m["claim_generator_info"]=v.is_array()?v:Json::array({v});}
   if(node.contains("Actions")&&node["Actions"].is_array())for(auto& a:node["Actions"])m["actions"].push_back(a);
   if(node.contains("Relationship")||node.contains("ActiveManifest")){
    Json ingredient={{"relationship",node.contains("Relationship")?node["Relationship"]:Json(nullptr)},{"manifest_id",node.contains("ActiveManifest")?manifestFromUri(node["ActiveManifest"]):""},{"source",doc}};
    if(node.contains("ValidationResults"))ingredient["embedded_validation_claims"]=node["ValidationResults"];
    m["ingredients"].push_back(std::move(ingredient));
   }
  }out.push_back(std::move(m));
 }
 return out;
}
Json toolManifests(const Json& report) {
 Json out=Json::array();if(!report.is_object()||!report.contains("manifests")||!report["manifests"].is_object())return out;
 if(report["manifests"].size()>256)throw std::runtime_error("C2PA manifest interpretation limit (256) exceeded; raw validator data retained");
 for(auto it=report["manifests"].begin();it!=report["manifests"].end();++it){if(!it->is_object())throw std::runtime_error("Malformed c2patool manifest object");auto& v=it.value();auto m=manifest(it.key(),"c2patool manifest store");
  if(v.contains("claim_generator_info"))m["claim_generator_info"]=v["claim_generator_info"];
  if(v.contains("claim_generator"))m["claim_generator"]=v["claim_generator"];
  if(v.contains("signature_info"))m["signature_info"]=v["signature_info"];
  if(v.contains("assertions")&&v["assertions"].is_array())for(auto& assertion:v["assertions"]){auto label=stringAt(assertion,"label");if((label=="c2pa.actions"||label.starts_with("c2pa.actions."))&&assertion.contains("data")&&assertion["data"].is_object()){auto& data=assertion["data"];if(data.contains("actions")&&data["actions"].is_array())for(auto& a:data["actions"])m["actions"].push_back(a);}}
  if(v.contains("ingredients")&&v["ingredients"].is_array())for(auto& ingredient:v["ingredients"]){Json i={{"relationship",stringAt(ingredient,"relationship")},{"manifest_id",stringAt(ingredient,"active_manifest")}};if(ingredient.contains("title"))i["title"]=ingredient["title"];if(ingredient.contains("validation_results"))i["embedded_validation_claims"]=ingredient["validation_results"];m["ingredients"].push_back(std::move(i));}
  out.push_back(std::move(m));
 }return out;
}
std::set<std::string> codes(const Json& bucket,std::string_view kind) {
 std::set<std::string> result;if(bucket.is_object()&&bucket.contains(std::string(kind))&&bucket[std::string(kind)].is_array())for(auto& entry:bucket[std::string(kind)]){auto code=stringAt(entry,"code");if(!code.empty())result.insert(code);}return result;
}
bool prefix(const std::set<std::string>& codes,std::string_view p){for(auto& c:codes)if(c.starts_with(p))return true;return false;}
Json validateSummary(const Json& report,bool trustConfigured) {
 Json result={{"performed",true},{"source","Independent c2patool invocation; only top-level validation_results are used"},{"active_manifest",stringAt(report,"active_manifest")},{"signature_integrity","not_reported"},{"content_binding","not_reported"},{"manifest_integrity","not_established"},{"certificate_trust",trustConfigured?"not_reported":"not_evaluated_no_trust_store"},{"timestamp_integrity","not_reported"},{"timestamp_trust","not_reported"},{"tool_validation_state",stringAt(report,"validation_state")},{"network","Disabled: no remote manifests, network OCSP, or identity resolution"}};
 if(!report.contains("validation_results")||!report["validation_results"].is_object()||!report["validation_results"].contains("activeManifest")||!report["validation_results"]["activeManifest"].is_object()) {result["performed"]=false;result["limitation"]="Expected independent validation results are absent";return result;}
 auto& bucket=report["validation_results"]["activeManifest"];if(!bucket.contains("success")||!bucket["success"].is_array()||!bucket.contains("failure")||!bucket["failure"].is_array()){result["performed"]=false;result["limitation"]="Malformed independent validation result buckets";return result;}auto success=codes(bucket,"success"),failure=codes(bucket,"failure");
 if(success.contains("claimSignature.validated"))result["signature_integrity"]="valid";
 if(prefix(failure,"claimSignature."))result["signature_integrity"]="failed";
 if(success.contains("assertion.dataHash.match")||success.contains("assertion.bmffHash.match")||success.contains("assertion.boxesHash.match"))result["content_binding"]="valid";
 bool nonTrustFailure=false;for(auto& c:failure){if(c.find("Hash")!=c.npos||c.starts_with("assertion.hashedURI."))result["content_binding"]="failed";if(!c.starts_with("signingCredential.")&&!c.starts_with("timeStamp."))nonTrustFailure=true;}
 if(result["signature_integrity"]=="valid"&&result["content_binding"]=="valid"&&!nonTrustFailure)result["manifest_integrity"]="valid";
 if(result["signature_integrity"]=="failed"||result["content_binding"]=="failed"||nonTrustFailure)result["manifest_integrity"]="failed";
 if(trustConfigured){if(success.contains("signingCredential.trusted"))result["certificate_trust"]="trusted_by_bundled_snapshot";if(prefix(failure,"signingCredential."))result["certificate_trust"]="untrusted_or_invalid";}
 if(success.contains("timeStamp.validated"))result["timestamp_integrity"]="valid";
 if(success.contains("timeStamp.trusted"))result["timestamp_trust"]="trusted_by_configured_validator";
 if(prefix(failure,"timeStamp.")){result["timestamp_integrity"]="failed_or_untrusted";result["timestamp_trust"]="not_established";}
 result["active_manifest_results"]=bucket;
 if(report["validation_results"].contains("ingredientDeltas"))result["ingredient_validation_deltas"]=report["validation_results"]["ingredientDeltas"];
 result["scope"]="Active manifest signature and current-file content binding. Earlier ingredient manifests retain embedded validation claims; ingredient deltas are separate, and are not a new full verification of every historical signature. Trust is relative to the local trust-list snapshot. No current online revocation check or watermark detection.";
 return result;
}
std::string lower(std::string text){for(auto& c:text)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));return text;}
}
std::string metadataCategory(std::string_view key) {
 auto family=key.substr(0,key.find(':'));
 if(family=="File")return key.find(":System:")!=key.npos?"Filesystem":"File";
 return std::string(parserNamespaceCategory(family));
}
void analyzeProvenance(Result& r,const Options& options,std::atomic_bool& cancel) {
 bool detected=false;for(auto& b:r.blocks)if(b.type=="caBX")detected=true;
 auto fallback=exifManifests(r);detected=detected||!fallback.empty();
 r.provenance={{"detected",detected},{"source","ExifTool and native structure evidence"},{"manifests",fallback},{"active_manifest",nullptr},{"verification",{{"performed",false},{"manifest_integrity","not_verified"},{"certificate_trust","not_evaluated"}}}};
 if(detected&&options.useC2pa){
  auto executable=options.c2patool.empty()?options.exiftool.parent_path().parent_path()/L"c2patool"/L"c2patool.exe":options.c2patool;
  try{
   if(!fs::is_regular_file(executable))throw std::runtime_error("c2patool missing; C2PA claims have not been independently verified");
   auto proc=runC2pa(executable,r.path,cancel,options.parserTimeoutMs);
   r.provenance["validator_diagnostics"]={{"exit_code",proc.exitCode},{"stderr",proc.err},{"expected_bundled_version","0.28.2"}};
   if(!proc.err.empty())r.errors.push_back("c2patool stderr: "+proc.err);
   if(proc.exitCode)r.errors.push_back("c2patool exit code "+std::to_string(proc.exitCode)+"; see validator diagnostics");
   if(!proc.out.empty()){
    r.c2paReport=boundedJson(proc.out);auto manifests=toolManifests(r.c2paReport);
    if(manifests.empty())throw std::runtime_error("No C2PA manifests in validator output despite embedded C2PA evidence");
    r.provenance["manifests"]=std::move(manifests);r.provenance["active_manifest"]=stringAt(r.c2paReport,"active_manifest");r.provenance["source"]="c2patool extraction; native and ExifTool evidence retained separately";
    bool trustedStore=fs::is_regular_file(executable.parent_path()/L"c2pa-trust-list.pem");r.provenance["verification"]=validateSummary(r.c2paReport,trustedStore);
    r.provenance["verification"]["trust_store"]={{"name","Bundled C2PA Conformance Trust List"},{"snapshot_date","2026-10-22"},{"snapshot_sha256","b3949271a07b007e92ef1d18050b55388f5e0d2686a16fd864176c48421af8ae"},{"source","https://github.com/c2pa-org/conformance-public/blob/main/trust-list/C2PA-TRUST-LIST.pem"},{"present",trustedStore},{"limitation","Snapshot is not refreshed automatically; see DEPENDENCIES.json for the pinned distribution."}};
    if(!r.provenance["verification"]["performed"].get<bool>())r.errors.push_back("Independent C2PA validation results unavailable");
    if(r.provenance["verification"]["manifest_integrity"]=="failed")r.errors.push_back("Independent C2PA integrity validation failed; provenance claims may have been altered");
    auto& verification=r.provenance["verification"];if(verification.contains("active_manifest_results")&&!codes(verification["active_manifest_results"],"failure").empty())r.errors.push_back("C2PA validation reported failures; see active manifest validation results");
    if(verification.contains("ingredient_validation_deltas"))for(auto& delta:verification["ingredient_validation_deltas"])if(delta.is_object()&&delta.contains("validationDeltas")&&!codes(delta["validationDeltas"],"failure").empty())r.errors.push_back("C2PA ingredient validation delta contains failures");
   }else throw std::runtime_error("c2patool returned no JSON; C2PA claims remain unverified");
  }catch(const std::exception& e){r.errors.push_back(e.what());r.provenance["verification"]["error"]=e.what();if(cancel)throw;}
 }else r.provenance["verification"]["reason"]=detected?"Validator disabled":"No embedded C2PA evidence recovered; remote-only provenance is not fetched";
 summarizeProvenance(r);
}
void summarizeProvenance(Result& r) {
 auto& p=r.provenance;if(!p.is_object())return;
 p["relationships"]=Json::array();p["ai_claims"]=Json::array();p["watermark_claims"]=Json::array();p["history_order"]=Json::array();
 auto& manifests=p["manifests"]; // ordered_json object insertions invalidate value references.
 std::set<std::string> ids;for(auto& m:manifests)ids.insert(stringAt(m,"id"));std::map<std::string,std::set<std::string>> parents;
 for(auto& m:manifests){auto id=stringAt(m,"id");if(p["verification"].value("performed",false)&&p["active_manifest"]==id)m["independent_validation"]="See separate active-manifest verification result; claimed actions are not proven facts.";
  for(auto& i:m["ingredients"]){auto parent=stringAt(i,"manifest_id"),relationship=stringAt(i,"relationship");p["relationships"].push_back({{"manifest",id},{"ingredient_manifest",parent},{"relationship",relationship},{"resolved",ids.contains(parent)}});if(relationship=="parentOf"&&ids.contains(parent))parents[id].insert(parent);}
  size_t index=0;for(auto& a:m["actions"]){auto sourceType=stringAt(a,"digitalSourceType"),description=stringAt(a,"description");auto type=sourceType.substr(sourceType.find_last_of('/')==sourceType.npos?0:sourceType.find_last_of('/')+1);
   if((sourceType.starts_with("http://cv.iptc.org/newscodes/digitalsourcetype/")||sourceType.starts_with("https://cv.iptc.org/newscodes/digitalsourcetype/"))&&(type=="trainedAlgorithmicMedia"||type=="compositeWithTrainedAlgorithmicMedia"))p["ai_claims"].push_back({{"manifest",id},{"action_index",index},{"action",a},{"basis","Explicit IPTC trained-algorithm digitalSourceType in embedded C2PA action"},{"claim_status","Embedded claim; not independent image-content detection"}});
   if(lower(description).find("synthid")!=description.npos)p["watermark_claims"].push_back({{"manifest",id},{"action_index",index},{"description",description},{"independent_watermark_detection",false},{"interpretation","Embedded action description mentions SynthID; application is recorded as a claim only"}});
   ++index;
  }
 }
 std::set<std::string> emitted;while(emitted.size()<ids.size()){bool progress=false;for(auto& id:ids)if(!emitted.contains(id)&&std::all_of(parents[id].begin(),parents[id].end(),[&](const auto& parent){return emitted.contains(parent);})){p["history_order"].push_back(id);emitted.insert(id);progress=true;}if(!progress){p["history_order"]=nullptr;r.errors.push_back("Cyclic C2PA parent relationship; no history order inferred");break;}}
 p["history_order_basis"]="Topological parentOf ingredient links, not timestamps. Disconnected records do not establish a total chronology.";
 p["claims_limitation"]="Manifest creators, software, identities and actions are claims. Signature integrity and certificate trust are reported separately. No account, exact model, prompt or seed is inferred from provenance.";
 addField(r,"Summary","C2PA manifests",p["manifests"].size(),"Provenance analysis");
 addField(r,"AI provenance","AI provenance result",p["ai_claims"].empty()?"No explicit trained-algorithm AI provenance claim recovered. This is not a determination that the image is human-made.":"Embedded C2PA actions explicitly claim AI-generated media. Model parameters are reported separately.","Derived from embedded C2PA claims");
 for(auto& c:p["ai_claims"])addField(r,"AI provenance","AI action claim ("+stringAt(c,"manifest")+")",c["action"],"Embedded C2PA action","Embedded claim; cryptographic results are separate.");
 for(auto& c:p["watermark_claims"])addField(r,"AI provenance","SynthID action description",c["description"],"Embedded C2PA action","No independent watermark detection was performed.");
 auto& v=p["verification"];addField(r,"C2PA validation","Active manifest integrity",v.value("manifest_integrity",std::string("not_verified")),"c2patool");
 addField(r,"C2PA validation","Active signing certificate trust",v.value("certificate_trust",std::string("not_evaluated")),"c2patool","Trust is relative to the bundled trust-list snapshot; no online revocation fetch.");
 addField(r,"C2PA validation","Verification scope",v.value("scope",std::string("Independent validation not performed. Embedded validation records are claims only.")),"Analysis");
}
Json structureGroups(const Result& r,bool groupIdat) {
 Json result=Json::array();for(size_t i=0;i<r.blocks.size();){auto& first=r.blocks[i];size_t end=i+1;if(groupIdat&&first.type=="IDAT")while(end<r.blocks.size()&&r.blocks[end].type=="IDAT"&&r.blocks[end-1].offset+r.blocks[end-1].size==r.blocks[end].offset)++end;
  uint64_t total=0;size_t crcValid=0,crcBad=0;for(size_t n=i;n<end;++n){total+=r.blocks[n].size;if(r.blocks[n].note.find("CRC valid")!=std::string::npos)++crcValid;if(r.blocks[n].note.find("CRC MISMATCH")!=std::string::npos)++crcBad;}
  result.push_back({{"type",first.type},{"count",end-i},{"first_block_index",i},{"offset",first.offset},{"total_bytes",total},{"crc_valid_count",crcValid},{"crc_mismatch_count",crcBad},{"note",end-i>1?"Consecutive IDAT chunks grouped for display; every original block, hash, offset and CRC result is retained in blocks[]":first.note}});i=end;
 }return result;
}
Json redactLocalPaths(const Json& report,const std::vector<fs::path>& originals) {
 std::vector<std::pair<std::string,std::string>> replacements;std::map<std::string,std::string> stems;size_t number=0;
 for(auto& path:originals){++number;auto full=utf8(fs::absolute(path).lexically_normal().wstring());auto parent=utf8(fs::absolute(path).parent_path().wstring());auto filename=utf8(path.filename().wstring());stems[utf8(path.stem().wstring())]="[file-"+std::to_string(number)+"]";for(auto s:{full,parent,filename}){if(s.empty())continue;auto replacement=s==parent?"[local-directory]":"[file-"+std::to_string(number)+"]";replacements.emplace_back(s,replacement);std::replace(s.begin(),s.end(),'\\','/');replacements.emplace_back(s,replacement);}}
 std::sort(replacements.begin(),replacements.end(),[](auto& a,auto& b){return a.first.size()>b.first.size();});
 auto redact=[&](std::string s){
  if(auto it=stems.find(s);it!=stems.end())return it->second;
  for(auto& [needle,replacement]:replacements){size_t pos=0;while((pos=s.find(needle,pos))!=s.npos){s.replace(pos,needle.size(),replacement);pos+=replacement.size();}}
  // Linear scanning avoids regex backtracking/stack growth on large hostile metadata.
  std::string out;out.reserve(s.size());
  for(size_t i=0;i<s.size();){auto alpha=[](char c){return (c>='A'&&c<='Z')||(c>='a'&&c<='z');};auto slash=[](char c){return c=='/'||c=='\\';};
   bool boundary=i==0||!(alpha(s[i-1])||(s[i-1]>='0'&&s[i-1]<='9'));
   bool drive=boundary&&i+2<s.size()&&alpha(s[i])&&s[i+1]==':'&&slash(s[i+2]);
   bool unc=i+1<s.size()&&s[i]=='\\'&&s[i+1]=='\\';
   if(drive||unc){i+=drive?3:2;while(i<s.size()&&std::string_view("\r\n\t\"<>|,;}]").find(s[i])==std::string_view::npos)++i;out+="[local-path]";}
   else out+=s[i++];
  }return out;
 };
 std::function<Json(const Json&,size_t)> walk=[&](const Json& j,size_t depth)->Json{
  if(depth>128)throw std::runtime_error("Privacy export nesting limit exceeded");
  if(j.is_string())return redact(j.get<std::string>());
  if(j.is_array()){Json a=Json::array();for(auto& v:j)a.push_back(walk(v,depth+1));return a;}
  if(j.is_object()){Json o=Json::object();size_t collision=0;for(auto it=j.begin();it!=j.end();++it){auto key=redact(it.key());while(o.contains(key))key=redact(it.key())+" [redacted key "+std::to_string(++collision)+"]";o[key]=walk(it.value(),depth+1);}return o;}
  return j;
 };
 auto result=walk(report,0);result["privacy"]={{"local_paths_redacted",true},{"scope","Textual local paths and original basenames are redacted recursively, including parser output, diagnostics and comparison data."},{"limitation","This is path redaction, not anonymization. Embedded authors, identifiers, URLs, binary/hex/base64 payloads and arbitrary personal metadata may remain; review before sharing."}};return result;
}
}
