#include "generation_formats.hpp"
#include <algorithm>
#include <cctype>
#include <set>
#include <stdexcept>
namespace fa {
namespace {
std::string tail(std::string name){auto p=name.find_last_of(':');if(p!=name.npos)name.erase(0,p+1);for(auto& c:name)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));return name;}
bool evidence(const Field& f){return f.category!="Analysis diagnostics"&&f.category!="Derived values"&&f.category!="Filesystem"&&f.category!="File"&&f.category!="Limitations";}
bool any(const Json& j,std::initializer_list<const char*> keys){for(auto key:keys)if(j.contains(key)&&!j[key].is_null())return true;return false;}
}
void parseStructuredGeneration(Result& r){
 bool fooocus=false,novel=false;for(const auto& f:r.fields)if(evidence(f)&&f.value.is_string()){
  auto name=tail(f.name);const auto& value=f.value.get_ref<const std::string&>();
  if(name=="fooocus_scheme"&&value=="fooocus")fooocus=true;
  if(name=="software"&&value.starts_with("Fooocus"))fooocus=true;
  if((name=="software"||name=="source")&&value.starts_with("NovelAI"))novel=true;
 }
 std::set<std::pair<std::string,std::string>> seen;size_t added=0;
 auto add=[&](const Field& field,std::string name,const Json& value,std::string_view convention){if(added>=1024)throw std::runtime_error("Structured generation display limit (1024) reached; original metadata retained");++added;r.generation.push_back({{"source_field",field.name},{"name",std::move(name)},{"value",value},{"interpretation","Unverified embedded generation metadata; convention recognition does not authenticate the generator"},{"convention",convention}});};
 for(const auto& field:r.fields){if(!evidence(field)||!field.value.is_string())continue;auto name=tail(field.name);const auto& text=field.value.get_ref<const std::string&>();
  bool invoke=name=="invokeai_metadata"||name=="invokeai_workflow"||name=="invokeai_graph";
  bool focus=fooocus&&(name=="parameters"||name=="usercomment");bool nai=novel&&(name=="comment"||name=="usercomment");if(!invoke&&!focus&&!nai)continue;
  if(focus&&nai){r.errors.push_back("Ambiguous generator markers for "+field.name+"; original metadata retained without assigning a convention.");continue;}
  if(!seen.insert({name,text}).second)continue;
  try{
   if(text.size()>1024*1024)throw std::runtime_error("Structured generation JSON exceeds the 1 MiB limit; original value retained");
   auto json=boundedJson(text,64,20000);if(!json.is_object())throw std::runtime_error("Expected a generation JSON object");
   auto values=[&](std::string_view convention,std::initializer_list<std::pair<const char*,const char*>> fields){for(auto [key,title]:fields)if(json.contains(key)&&!json[key].is_null())add(field,title,json[key],convention);};
   if(invoke){
    if(name=="invokeai_metadata"){
     if(!any(json,{"positive_prompt","negative_prompt","seed","steps","model","generation_mode"}))throw std::runtime_error("InvokeAI key contains no recognized generation properties");
     add(field,"InvokeAI metadata JSON",json,"InvokeAI");
     values("InvokeAI",{{"positive_prompt","Prompt"},{"negative_prompt","Negative prompt"},{"seed","Seed"},{"steps","Steps"},{"cfg_scale","CFG scale"},{"scheduler","Scheduler"},{"sampler","Sampler"},{"width","Width"},{"height","Height"},{"model","Model record"},{"vae","VAE record"},{"loras","LoRAs and weights"},{"controlnets","ControlNet settings"},{"control_layers","Control layers"},{"strength","Denoising strength"},{"refiner_model","Refiner model"},{"generation_mode","Generation mode"},{"app_version","Generator version claimed"}});
     if(json.contains("model")&&json["model"].is_object())for(auto key:{"name","model_name","hash"})if(json["model"].contains(key)&&json["model"][key].is_string())add(field,std::string(key)=="hash"?"Model hash":"Model name claimed",json["model"][key],"InvokeAI");
    }else{
     bool shape=name=="invokeai_graph"?json.contains("nodes")&&json["nodes"].is_object()&&json.contains("edges")&&json["edges"].is_array():json.contains("nodes")&&json["nodes"].is_array()&&json.contains("edges")&&json["edges"].is_array();
     if(!shape)throw std::runtime_error("Unrecognized InvokeAI workflow/graph shape; original value retained");add(field,name=="invokeai_graph"?"InvokeAI graph JSON":"InvokeAI workflow JSON",json,"InvokeAI");
    }
   }else if(focus){
    if(!any(json,{"prompt","full_prompt","seed","base_model"}))throw std::runtime_error("Fooocus marker without recognized generation properties");
    add(field,"Fooocus parameters JSON",json,"Fooocus");values("Fooocus",{{"prompt","Prompt"},{"negative_prompt","Negative prompt"},{"full_prompt","Expanded prompt"},{"full_negative_prompt","Expanded negative prompt"},{"seed","Seed"},{"steps","Steps"},{"guidance_scale","CFG scale"},{"sampler","Sampler"},{"scheduler","Scheduler"},{"resolution","Resolution"},{"base_model","Model"},{"base_model_hash","Model hash"},{"refiner_model","Refiner model"},{"refiner_model_hash","Refiner model hash"},{"refiner_switch","Refiner switch"},{"vae","VAE"},{"loras","LoRAs and weights/hashes"},{"styles","Styles"},{"performance","Performance setting"},{"version","Generator version claimed"}});
    for(auto it=json.begin();it!=json.end();++it)if(it.key().starts_with("lora_combined_"))add(field,it.key(),it.value(),"Fooocus");
   }else if(nai){
    if(!any(json,{"prompt","uc","seed","steps","sampler"}))throw std::runtime_error("NovelAI marker without recognized generation properties");
    add(field,"NovelAI visible metadata JSON",json,"NovelAI visible metadata");values("NovelAI visible metadata",{{"prompt","Prompt"},{"uc","Negative prompt"},{"seed","Seed"},{"steps","Steps"},{"scale","CFG scale"},{"sampler","Sampler"},{"noise_schedule","Scheduler"},{"width","Width"},{"height","Height"},{"strength","Denoising strength"},{"noise","Noise"},{"sm","SMEA setting"},{"sm_dyn","Dynamic SMEA setting"},{"v4_prompt","Structured prompt record"},{"v4_negative_prompt","Structured negative prompt record"}});
   }
  }catch(const std::exception& e){r.errors.push_back("Structured generation metadata in "+field.name+": "+e.what());if(added>=1024)break;}
 }
}
}
