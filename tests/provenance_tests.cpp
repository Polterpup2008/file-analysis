#include "core.hpp"
#include "format_registry.hpp"
#include <fstream>
#include <iostream>
#include <set>
using namespace fa;
namespace {
int checks=0;
void check(bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);}
bool contains(const std::vector<std::string>& values,std::string_view needle){for(auto& v:values)if(v.find(needle)!=v.npos)return true;return false;}
void save(const fs::path& path,std::string bytes){std::ofstream out(path,std::ios::binary);out.write(bytes.data(),bytes.size());}
std::string load(const fs::path& path){std::ifstream in(path,std::ios::binary);return {std::istreambuf_iterator<char>(in),{}};}
Json fallbackFixture(){
 Json j=Json::object();auto set=[&](std::string doc,std::string name,Json value){j["JUMBF:CBOR:"+doc+":PNG-JUMBF:"+name]=value;};
 set("Doc1-1","JUMDType","(c2ma)-0011");set("Doc1-1","JUMDLabel","urn:fixture:origin");
 set("Doc1-1-2","Claim_generator_info",{{"name","Fixture AI software"},{"version","1"}});
 set("Doc1-1-3-2","Actions",Json::array({{{"action","c2pa.created"},{"digitalSourceType","http://cv.iptc.org/newscodes/digitalsourcetype/trainedAlgorithmicMedia"},{"description","Fixture generation claim"}},{{"action","c2pa.edited"},{"description","Applied imperceptible SynthID watermark."}}}));
 set("Doc1-2","JUMDType","(c2ma)-0011");set("Doc1-2","JUMDLabel","urn:fixture:edited");
 set("Doc1-2-3-2","Actions",Json::array({{{"action","c2pa.converted"},{"description","Converted to PNG"}}}));
 set("Doc1-2-3-3","Relationship","parentOf");set("Doc1-2-3-3","ActiveManifest",{{"url","self#jumbf=/c2pa/urn:fixture:origin"}});
 set("Doc1-2-3-3","ValidationResults",{{"activeManifest",{{"success",Json::array({{{"code","claimSignature.validated"}},{{"code","signingCredential.trusted"}}})},{"failure",Json::array()}}}});
 return j;
}
}
int wmain(int argc,wchar_t** argv){try{
 check(argc==4||argc==5,"Expected fixtures, ExifTool, c2patool, optional evidence path");fs::path root=argv[1];Options options;options.exiftool=argv[2];options.c2patool=argv[3];std::atomic_bool stop=false;
 check(pngChunk("caBX")&&pngChunk("caBX")->category=="Provenance","caBX registered as C2PA JUMBF");
 check(metadataCategory("ExifTool:Main:ExifTool:NewUUID")=="Analysis diagnostics","Generated UUID is a diagnostic");
 check(metadataCategory("ExifTool:Main:ExifTool:Now")=="Analysis diagnostics","Scan time is not EXIF");
 check(metadataCategory("EXIF:IFD0:Main:Make")=="EXIF","Actual EXIF classification");
 check(metadataCategory("JUMBF:CBOR:Doc1:Actions")==pngChunk("caBX")->category,"Shared native/parser provenance category");
 check(metadataCategory("Composite:Main:Megapixels")=="Derived values","Computed values identified");
 check(metadataCategory("File:System:Main:Directory")=="Filesystem","Filesystem separated from image metadata");
 Result fallback;fallback.parser=fallbackFixture();Options disabled=options;disabled.useC2pa=false;analyzeProvenance(fallback,disabled,stop);
 check(fallback.provenance["manifests"].size()==2,"Two ExifTool manifests reconstructed");
 if(fallback.provenance["relationships"].size()!=1)std::cerr<<fallback.provenance.dump(2)<<"\n";
 check(fallback.provenance["relationships"].size()==1,"Ingredient relationship reconstructed");
 check(fallback.provenance["relationships"][0]["resolved"]==true,"Relationship resolves to existing manifest");
 check(fallback.provenance["history_order"][0]=="urn:fixture:origin","History order follows parentOf, not map order");
 check(fallback.provenance["ai_claims"].size()==1,"Explicit AI digitalSourceType recovered");
 check(fallback.provenance["watermark_claims"].size()==1&&!fallback.provenance["watermark_claims"][0]["independent_watermark_detection"].get<bool>(),"SynthID is a recorded claim, never independent detection");
 check(fallback.provenance["verification"]["performed"]==false,"Embedded success records cannot establish fresh validation");
 check(fallback.provenance["active_manifest"].is_null(),"Active manifest not guessed from ExifTool ordering");
 parseGeneration(fallback);check(fallback.generation.empty(),"Provenance is independent from model parameters");
 check(fallback.json().dump().find("No useful generation metadata")==std::string::npos,"Old misleading generation summary removed");
 Result cycle=fallback;cycle.provenance["manifests"][0]["ingredients"].push_back({{"relationship","parentOf"},{"manifest_id","urn:fixture:edited"}});summarizeProvenance(cycle);check(cycle.provenance["history_order"].is_null()&&contains(cycle.errors,"Cyclic"),"Cycle is reported without invented history");
 Result absent=fallback;absent.fields.clear();Options missing=options;missing.c2patool=root/L"missing-c2patool.exe";analyzeProvenance(absent,missing,stop);check(absent.provenance["manifests"].size()==2&&contains(absent.errors,"missing"),"Claims survive missing validator with explicit error");
 Result a,b;a.fields.push_back({"Analysis diagnostics","ExifTool:Now","ExifTool","","first",nullptr,{},{}});b.fields.push_back({"Analysis diagnostics","ExifTool:Now","ExifTool","","second",nullptr,{},{}});check(compare(a,b)["changes"].empty(),"Volatile diagnostics excluded from forensic comparisons");
 auto grouped=analyze(root/L"repeated-idat.png",options,stop);auto groups=structureGroups(grouped);check(grouped.blocks.size()==5&&groups.size()==3&&groups[1]["count"]==3,"Repeated IDAT compacted without losing raw blocks");check(groups[1]["crc_valid_count"]==3,"All grouped CRC results retained");check(structureGroups(grouped,false).size()==5,"Individual chunk view reversible");
 auto malformed=analyze(root/L"malformed-c2pa.png",options,stop);check(malformed.blocks[1].note.find("C2PA")!=std::string::npos&&malformed.blocks[1].note.find("Unknown")==std::string::npos,"caBX recognized even when payload is malformed");check(!malformed.errors.empty()&&malformed.provenance["verification"]["manifest_integrity"]!="valid","Malformed C2PA never validated");
 auto signedImage=analyze(root/L"c2pa-signed.jpg",options,stop);check(signedImage.provenance["verification"]["performed"]==true,"Real official validator invoked on signed fixture");check(signedImage.provenance["verification"]["signature_integrity"]=="valid","Real claim signature validates");check(signedImage.provenance["verification"]["manifest_integrity"]=="valid","Real signed asset binding validates");check(signedImage.provenance["verification"]["certificate_trust"]=="untrusted_or_invalid","Valid signature is distinct from untrusted signer");
 auto unicodePath=root/L"c2pa-\u753b\u50cf-\u00e9 & literal.jpg";fs::copy_file(root/L"c2pa-signed.jpg",unicodePath,fs::copy_options::overwrite_existing);auto unicode=analyze(unicodePath,options,stop);check(unicode.provenance["verification"]["manifest_integrity"]=="valid","C2PA validator supports Unicode, spaces and shell metacharacters without a shell");
 bool settingsRejected=false;try{runC2pa(root/L"nonexistent-tool-dir"/L"c2patool.exe",root/L"c2pa-signed.jpg",stop,1000);}catch(const std::exception& e){settingsRejected=std::string(e.what()).find("offline settings missing")!=std::string::npos;}check(settingsRejected,"Missing offline settings fail closed before starting a process");
 // Alter an actual encoded scan byte while preserving every C2PA assertion and signature byte.
 auto tamperedPath=root/L"c2pa-tampered.jpg";auto bytes=load(root/L"c2pa-signed.jpg");bool changed=false;for(auto& block:signedImage.blocks)if(block.type=="Entropy data"&&block.size>10){bytes[static_cast<size_t>(block.offset+5)]^=1;changed=true;break;}check(changed,"Found real image scan data for tampering test");save(tamperedPath,bytes);
 auto tampered=analyze(tamperedPath,options,stop);check(tampered.provenance["verification"]["content_binding"]=="failed"&&tampered.provenance["verification"]["manifest_integrity"]=="failed","Tampered image fails cryptographic file binding");check(tampered.provenance["verification"]["signature_integrity"]=="valid","Tampering test distinguishes claim signature from file integrity");
 Json privacy={{"files",Json::array({{{"path","C:\\Users\\Fixture\\Downloads\\image.png"},{"nested",{{"SourceFile","C:/Users/Fixture/Downloads/image.png"},{"Directory","C:/Users/Fixture/Downloads"},{"BaseName","image"},{"file","image.png"}}},{"errors",Json::array({"Cannot open D:\\Private Folder\\secret.png (Win32 5)","UNC \\\\server\\private\\file.png"})}}})},{"comparison",{{"left","C:\\Users\\Fixture\\Downloads\\image.png"},{"right","D:\\Private\\other.png"}}},{"public_url","https://cv.iptc.org/newscodes/digitalsourcetype/trainedAlgorithmicMedia"},{"author","Embedded author remains"}};
 auto privateReport=redactLocalPaths(privacy,{fs::path(L"C:\\Users\\Fixture\\Downloads\\image.png")});auto serialized=privateReport.dump();check(serialized.find("Fixture")==std::string::npos&&serialized.find("secret.png")==std::string::npos&&serialized.find("server")==std::string::npos&&serialized.find("image.png")==std::string::npos,"Recursive path, basename, diagnostic and UNC redaction");check(privateReport["public_url"]==privacy["public_url"],"Public evidence URLs are not mistaken for Windows paths");check(privateReport["author"]==privacy["author"],"Privacy option does not silently erase author evidence");check(privacy["files"][0]["path"]=="C:\\Users\\Fixture\\Downloads\\image.png","Redaction leaves original report unchanged");
 Json hostile={{"path",std::string("Z:\\")+std::string(2*1024*1024,'x')}};check(redactLocalPaths(hostile,{})["path"]=="[local-path]","Large hostile path redaction bounded without regex recursion");
 bool rejected=false;try{boundedJson(std::string(200,'['));}catch(...){rejected=true;}check(rejected,"JSON nesting bounded");
 auto output=root.parent_path()/L"output";fs::create_directories(output);
 if(argc==5){auto evidence=fs::path(argv[4]);auto originalHash=sha256File(evidence,stop);auto real=analyze(evidence,options,stop);
  check(real.errors.empty(),"Gemini PNG analyzes without errors");check(real.provenance["manifests"].size()==2&&real.provenance["relationships"].size()==1,"Gemini's two linked manifests reconstructed");check(real.provenance["verification"]["manifest_integrity"]=="valid","Gemini active manifest independently validates");check(real.provenance["verification"]["certificate_trust"]=="trusted_by_bundled_snapshot","Gemini signing chain trusted by bundled snapshot");check(real.provenance["ai_claims"].size()==2&&!real.provenance["watermark_claims"].empty(),"Gemini AI and SynthID claims surfaced");check(real.generation.empty(),"No model, prompt or seed invented for Gemini");check(real.blocks.size()==344&&structureGroups(real).size()<10,"Gemini chunk display compacted");check(sha256File(evidence,stop)==originalHash,"Gemini original remains byte-for-byte unchanged");
  Json report={{"application","File Analysis"},{"version","0.3.0"},{"schema_version",2},{"files",Json::array({real.json()})},{"batch_errors",Json::array()}};for(auto ext:{"json","csv","txt"}){auto dest=output/(std::string("gemini-redacted.")+ext);fs::remove(dest);exportReport(dest,redactLocalPaths(report,{evidence}),ext,{evidence});auto exported=load(dest);check(exported.find(utf8(evidence.parent_path().wstring()))==std::string::npos&&exported.find(utf8(evidence.filename().wstring()))==std::string::npos,"Gemini share export contains no local user path");}
  std::cout<<"Gemini: "<<real.blocks.size()<<" chunks, "<<structureGroups(real).size()<<" display groups, "<<real.provenance["manifests"].size()<<" manifests, "<<real.errors.size()<<" errors\n";
 }
 std::cout<<"PASS: "<<checks<<" provenance assertions\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<" provenance assertions: "<<e.what()<<"\n";return 1;}}
