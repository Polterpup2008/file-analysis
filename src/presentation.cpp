#include "presentation.hpp"
#include <algorithm>
#include <cctype>
#include <map>
#include <set>

namespace fa {
namespace {
const Json emptyArray=Json::array(),emptyObject=Json::object();
const Json& member(const Json& j,const char* key,const Json& fallback=emptyObject) {
 auto it=j.is_object()?j.find(key):j.end();return it!=j.end()?*it:fallback;
}
std::string str(const Json& j,const char* key) {
 const auto& v=member(j,key);return v.is_string()?v.get<std::string>():"";
}
std::string tail(std::string s) {
 auto pos=s.find_last_of(':');if(pos!=s.npos)s.erase(0,pos+1);
 for(auto& c:s)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));return s;
}
std::string actionName(const Json& a) {
 auto name=str(a,"action");
 static const std::map<std::string,std::string> names{{"c2pa.created","Created"},{"c2pa.opened","Opened source"},{"c2pa.edited","Edited"},{"c2pa.converted","Converted"},{"c2pa.saved","Saved"},{"c2pa.published","Published"},{"c2pa.cropped","Cropped"},{"c2pa.resized","Resized"}};
 auto it=names.find(name);return it!=names.end()?it->second:name.empty()?"Unspecified action":name;
}
std::string software(const Json& m) {
 std::string result;const auto& info=member(m,"claim_generator_info",emptyArray);
 auto add=[&](const Json& item){auto name=item.is_string()?item.get<std::string>():str(item,"name");auto version=str(item,"version");if(name.empty())return;if(!result.empty())result+="; ";result+=name;if(!version.empty())result+=" ("+version+")";};
 if(info.is_array())for(const auto& item:info)add(item);else add(info);return result;
}
}
std::vector<SimpleFinding> simpleFindings(const Result& r) {
 std::vector<SimpleFinding> out;
 auto add=[&](std::string topic,std::string title,std::string value,std::string detail){out.push_back({std::move(topic),std::move(title),std::move(value),std::move(detail)});};
 // Findings are never suppressed by the simpler presentation, including parser failures.
 std::set<std::string> warnings;
 for(const auto& e:r.errors)if(warnings.insert(e).second)add("Attention","Analysis finding",e,e+"\r\n\r\nDetailed view contains the associated parser and structural evidence.");
 if(r.status=="cancelled"||r.status=="error")add("Attention","Incomplete analysis",r.status=="cancelled"?"Analysis was cancelled":"Analysis did not finish","Only recovered results are shown. Missing metadata cannot be interpreted as absent after an incomplete analysis.");
 const auto& p=r.provenance;
 const auto& ai=member(p,"ai_claims",emptyArray);
 const bool aiClaim=ai.is_array()&&!ai.empty();
 if(aiClaim) {
  std::string descriptions;std::set<std::string> seen;
  for(const auto& claim:ai){const auto& action=member(claim,"action");if(str(action,"action")!="c2pa.created")continue;auto description=str(action,"description");if(!description.empty()&&seen.insert(description).second){if(!descriptions.empty())descriptions+="; ";descriptions+=description;}}
  add("AI","AI generation claim",descriptions.empty()?"AI-generated content recorded in provenance":descriptions,"Embedded Content Credentials explicitly label an action as involving trained-algorithm media.\r\n\r\nThis is a recorded claim, not an independent visual AI detector. Signature checks are reported separately. No exact model or account is inferred from this claim.");
 }
 const auto& watermarks=member(p,"watermark_claims",emptyArray);std::set<std::string> seenWatermarks;
 for(const auto& claim:watermarks){auto description=str(claim,"description");if(!description.empty()&&seenWatermarks.insert(description).second)add("AI","Watermark action recorded",description,"The provenance records this action. No independent watermark detection was performed. This does not establish that a detectable SynthID watermark is currently present.");}

 const auto& manifests=member(p,"manifests",emptyArray);
 const bool detected=member(p,"detected")==true;
 if(detected||!manifests.empty()) {
  const auto& links=member(p,"relationships",emptyArray);size_t resolved=0;for(const auto& link:links)if(member(link,"resolved")==true&&str(link,"relationship")=="parentOf")++resolved;
  add("Provenance","Content Credentials",std::to_string(manifests.size())+" record(s); "+std::to_string(resolved)+" source link(s)","Embedded C2PA provenance records describe how the file was created or processed. Source links connect records; they do not guarantee a complete editing history.\r\n\r\nSelect a recorded history row to read its actions. Detailed > Provenance retains the full manifests and source links.");
  const auto& verification=member(p,"verification");const bool performed=member(verification,"performed")==true;
  auto signature=str(verification,"signature_integrity"),binding=str(verification,"content_binding"),integrity=str(verification,"manifest_integrity");
  std::string checked=!performed?"Not independently verified":integrity=="failed"?"Failed - provenance or file integrity problem":integrity=="valid"&&signature=="valid"&&binding=="valid"?"Signature and file binding passed":"Incomplete - see Detailed validation results";
  add("Verification","Current record integrity",checked,"Independent check of the active record's signature and its binding to this file.\r\n\r\nSignature: "+(signature.empty()?"not verified":signature)+"\r\nFile binding: "+(binding.empty()?"not verified":binding)+"\r\n\r\nA valid signature does not prove that recorded actions are factual. Earlier records' embedded validation claims are not fresh independent checks of every historical signature.");
  auto trust=str(verification,"certificate_trust");
  std::string trusted=!performed?"Not evaluated":trust=="trusted_by_bundled_snapshot"?"Trusted by bundled certificate list":trust=="untrusted_or_invalid"?"Signer untrusted or invalid":"Trust not established";
  auto date=str(member(verification,"trust_store"),"snapshot_date");
  add("Verification","Signing certificate",trusted,"Certificate trust is separate from signature and file integrity.\r\n\r\n"+(date.empty()?std::string():"Bundled trust-list date: "+date+"\r\n")+"Checks run offline. There is no current online revocation check. Trust does not establish the truth of an action or identify a human creator.");

  // Respect parent-link order, but do not turn disconnected records into a chronology.
  std::vector<const Json*> ordered;std::set<std::string> included;
  for(const auto& id:member(p,"history_order",emptyArray))for(const auto& m:manifests)if(member(m,"id")==id){ordered.push_back(&m);included.insert(str(m,"id"));}
  for(const auto& m:manifests)if(!included.contains(str(m,"id")))ordered.push_back(&m);
  std::map<std::string,size_t> numbers;for(size_t i=0;i<ordered.size();++i)numbers[str(*ordered[i],"id")]=i+1;
  for(size_t i=0;i<ordered.size();++i){const auto& m=*ordered[i];bool active=member(p,"active_manifest")==member(m,"id");std::string actions,detail;
   for(const auto& action:member(m,"actions",emptyArray)){auto description=str(action,"description");auto label=actionName(action);auto value=description.empty()?label:description;if(!actions.empty())actions+=" -> ";actions+=value;detail+=label+(description.empty()?"":": "+description)+"\r\n";}
   for(const auto& link:links)if(str(link,"manifest")==str(m,"id")){auto source=numbers.find(str(link,"ingredient_manifest"));if(str(link,"relationship")=="parentOf")detail+="\r\nSource: "+(source==numbers.end()?std::string("record not recovered"):"record "+std::to_string(source->second))+".\r\n";}
   auto app=software(m);if(!app.empty())detail+="\r\nCredential-writing software claimed: "+app+"\r\n";
   auto signer=str(member(m,"signature_info"),"common_name");if(!signer.empty())detail+="Certificate name recorded: "+signer+"\r\n";
   detail+="\r\nThese are embedded action and software claims. Record numbers are reading labels, not timestamps; only source links establish relative order. ";detail+=active?"Independent checks for this current record appear in Verification.":"Historical validation records do not independently verify this earlier record.";
   add("History","Record "+std::to_string(i+1)+(active?" (current)":""),actions.empty()?"No action descriptions recovered":actions,detail);
  }
 }

 bool generationPresent=!r.generation.empty();
 if(generationPresent){
  std::set<std::pair<std::string,std::string>> seen;
  for(const auto& g:r.generation){auto name=str(g,"name");const auto& value=member(g,"value");
   if(name.starts_with("Node ")||name.starts_with("AUTOMATIC1111-compatible"))continue;
   if(value.is_object()||value.is_array()){add("Generation",name,"Embedded workflow available","A recognized generation workflow or API prompt was recovered. Switch to Detailed to inspect its full JSON and use Export generation.\r\n\r\nSource field: "+str(g,"source_field")+"\r\nEmbedded settings can be edited; they are not proof of generator identity.");continue;}
   auto valueText=display(value);if(seen.insert({name,valueText}).second)add("Generation",name,valueText,name+": "+valueText+"\r\n\r\nRecovered embedded setting; it can be edited or forged.\r\nSource field: "+str(g,"source_field"));
  }
 }else if(aiClaim||detected)add("Generation","Prompt / model / seed","Not recovered","No model, prompt or seed parameters were recovered by the implemented recognizers (ComfyUI and AUTOMATIC1111-compatible metadata). Missing settings do not negate AI provenance, and do not determine whether the file was generated by AI.");

 static const std::map<std::string,std::string> notable{
  {"software","Editing software"},{"creatortool","Creator application"},{"processingsoftware","Processing software"},{"historysoftwareagent","Editing history software"},
  {"artist","Creator claim"},{"author","Author claim"},{"creator","Creator claim"},{"ownername","Owner claim"},{"copyright","Copyright claim"},{"rights","Rights claim"},{"license","License"},{"webstatement","Rights URL"},
  {"description","Description"},{"imagedescription","Image description"},{"usercomment","Embedded comment"},{"comment","Embedded comment"},{"title","Embedded title"},
  {"bodyserialnumber","Camera serial number"},{"serialnumber","Device serial number"},{"lensserialnumber","Lens serial number"},{"gpslatitude","GPS latitude"},{"gpslongitude","GPS longitude"}
 };
 std::set<std::pair<std::string,std::string>> seenMetadata;
 for(const auto& f:r.fields){
  // Scanner values, ordinary file properties and raw C2PA structures belong in Detailed.
  if(f.category=="Analysis diagnostics"||f.category=="Derived values"||f.category=="File"||f.category=="Filesystem"||f.category=="Hashes"||f.category=="Provenance"||f.category=="AI provenance"||f.category=="C2PA validation"||f.category=="Summary"||f.category=="Limitations"||f.category=="Generation parameters")continue;
  auto name=tail(f.name);auto it=notable.find(name);if(it==notable.end()||f.value.is_null())continue;
  auto value=display(f.value);if(value.empty())continue;
  if(name=="usercomment"&&generationPresent)continue;
  if(seenMetadata.insert({it->second,value}).second)add("Embedded metadata",it->second,value,it->second+": "+value+"\r\n\r\nEmbedded field: "+f.name+"\r\nReader: "+f.source+"\r\n\r\nMetadata can be edited. Names, software and rights statements are claims, not proof of identity or ownership.");
 }
 std::map<std::string,size_t> unknown;for(const auto& b:r.blocks)if(b.note.find("Unknown")!=std::string::npos)++unknown[b.type];
 for(const auto& [type,count]:unknown)add("Structure","Unrecognized block",type+" - "+std::to_string(count)+" block(s)","The scanner recovered this block but does not recognize its purpose. Unknown data is not by itself evidence of tampering.\r\n\r\nDetailed > Structure provides its bytes, offsets and extraction tools.");
 if(out.empty())add("Findings","No additional clues recovered","No provenance, generation settings or selected embedded claims found","The implemented readers did not recover additional clues for Simple view. This does not establish that metadata never existed, that the file is human-made, or that its contents are authentic.\r\n\r\nDetailed contains every extracted field, format details, hashes and inspection tools. Exports always contain the full analysis.");
 return out;
}
}
