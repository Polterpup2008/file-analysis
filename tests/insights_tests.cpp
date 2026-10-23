#include "insights.hpp"
#include "presentation.hpp"
#include <fstream>
#include <iostream>
using namespace fa;
namespace {
int checks=0;
void check(bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);}
void field(Result& r,std::string name,Json value,std::string category="EXIF"){r.fields.push_back({category,std::move(name),"ExifTool","",std::move(value),nullptr,{},{}});}
bool finding(const Result& r,std::string_view text){for(const auto& f:simpleFindings(r))if((f.title+f.value+f.detail).find(text)!=std::string::npos)return true;return false;}
}
int wmain(int argc,wchar_t** argv){try{
 check(argc==3,"Expected fixtures and ExifTool");fs::path root=argv[1];Options options;options.exiftool=argv[2];options.useC2pa=false;std::atomic_bool cancel=false;
 auto before=sha256File(root/L"camera.png",cancel);auto camera=analyze(root/L"camera.png",options,cancel);
 check(camera.origin["assessment"]=="Capture device recorded in metadata","Camera tags produce a limited origin assessment");
 check(finding(camera,"Review before sharing")&&camera.privacy["counts"]["location"]>0&&camera.privacy["counts"]["identifiers"]>0,"Privacy findings include GPS and camera identifiers");
 check(camera.json()["origin_summary"]==camera.origin&&camera.json()["privacy"]==camera.privacy,"Full JSON preserves origin and privacy evidence");
 check(camera.privacy["findings"].dump().find("EXIF:")!=std::string::npos,"Privacy findings link to original field namespaces");
 auto clean=analyze(root/L"clean.png",options,cancel);check(clean.origin["assessment"]=="Origin not established"&&!finding(clean,"Review before sharing"),"Clean image receives no invented origin or personal-data finding");
 auto generation=analyze(root/L"metadata.png",options,cancel);check(generation.origin["assessment"]=="Generation settings recovered","Generation settings remain distinct from provenance claims");
 Result r;r.format="JPEG";r.path=L"C:\\PRIVATE-PATH\\PRIVATE-FILENAME.jpg";r.status="complete with findings/limitations";r.parser=Json::object();
 field(r,"EXIF:IFD0:Main:Make","Test Camera Maker");field(r,"EXIF:IFD0:Main:Model","Test Camera 5");
 field(r,"EXIF:ExifIFD:Main:SerialNumber","PRIVATE-SERIAL-123");field(r,"EXIF:GPS:Main:GPSLatitude","PRIVATE-LOCATION-48");field(r,"EXIF:GPS:Main:GPSLongitude","PRIVATE-LOCATION-2");
 field(r,"EXIF:IFD0:Main:Artist","PRIVATE-PERSON-NAME");field(r,"XMP:XMP-iptcCore:Main:CreatorContactInfo",{{"Email","PRIVATE-EMAIL@example.invalid"}},"XMP");
 field(r,"EXIF:ExifIFD:Main:UserComment","PRIVATE-COMMENT-CONTENT");field(r,"EXIF:IFD0:Main:Software","Example Editor 5");field(r,"EXIF:IFD1:Main:ThumbnailImage","PRIVATE-THUMBNAIL-PAYLOAD");
 field(r,"ExifTool:Main:Model","Scanner only","Analysis diagnostics");field(r,"EXIF:ExifIFD:Main:DateTimeOriginal","2025:01:02 03:04:05");
 r.parser={{"RawNested",{{"data","PRIVATE-PARSER-DATA"}}}};r.parserNumeric=r.parser;r.c2paReport={{"secret","PRIVATE-C2PA-DATA"}};
 r.provenance={{"detected",true},{"ai_claims",Json::array({{{"action",{{"action","c2pa.created"},{"description","PRIVATE-ACTION-TEXT"}}}}})},{"verification",{{"performed",false},{"signature_integrity","valid"},{"content_binding","valid"},{"certificate_trust","trusted_by_bundled_snapshot"}}}};
 r.generation=Json::array({{{"name","Prompt"},{"value","PRIVATE-PROMPT"},{"source_field","PRIVATE-WORKFLOW"}}});r.errors={"PRIVATE-ERROR-PATH"};r.blocks.push_back({"PRIVATE-BLOCK-TYPE","PRIVATE-BLOCK-NOTE","012345",33,100});
 summarizePhoto(r);summarizeInsights(r);auto snapshot=r.json();
 check(r.origin["assessment"]=="Camera and generation metadata present","Camera tags and AI claims coexist without forced attribution");
 check(r.origin["software_claims"].size()==1&&r.origin["software_claims"][0]["name"]=="Example Editor 5","Recorded editor surfaced separately");
 check(r.privacy["counts"]["people"]==2&&r.privacy["counts"]["previews"]==1&&r.privacy["counts"]["free_text"]>0,"Names, structured contact fields, previews and free text identified");
 check(finding(r,"not a visual detector")&&finding(r,"not an exhaustive personal-data detector"),"Origin and privacy limitations visible in Simple");
 auto share=shareReport({r},{},"0.5.0",2,true);auto text=share.dump();
 for(auto secret:{"PRIVATE-PATH","PRIVATE-FILENAME","PRIVATE-SERIAL","PRIVATE-LOCATION","PRIVATE-PERSON","PRIVATE-EMAIL","PRIVATE-COMMENT","PRIVATE-THUMBNAIL","PRIVATE-PARSER","PRIVATE-C2PA","PRIVATE-ACTION","PRIVATE-PROMPT","PRIVATE-WORKFLOW","PRIVATE-ERROR","PRIVATE-BLOCK"})check(text.find(secret)==text.npos,"Default share report excludes personal values, raw payloads and all duplicate source copies");
 check(text.find("Test Camera 5")!=text.npos&&text.find("Example Editor 5")!=text.npos,"Share summary keeps selected useful camera and software information");
 check(share["files"][0]["path"]=="File 1"&&share["batch_error_count"]==2&&share["cancelled"]==true&&share["files"][0]["error_count"]==1,"Share report labels anonymous file references and preserves error/cancellation counts");
 check(share["files"][0]["provenance"]["signature_integrity"]=="not_verified"&&share["files"][0]["provenance"]["certificate_trust"]=="not_evaluated","Unperformed validation cannot become a verified sharing claim");
 ShareOptions location;location.omitLocation=false;auto locationText=shareReport({r},location,"0.5.0").dump();check(locationText.find("PRIVATE-LOCATION")!=locationText.npos&&locationText.find("PRIVATE-SERIAL")==locationText.npos&&locationText.find("PRIVATE-PERSON")==locationText.npos,"Location selection does not unredact identities or people");
 ShareOptions ids;ids.omitIdentifiers=false;auto idText=shareReport({r},ids,"0.5.0").dump();check(idText.find("PRIVATE-SERIAL")!=idText.npos&&idText.find("PRIVATE-LOCATION")==idText.npos&&idText.find("PRIVATE-PERSON")==idText.npos,"Identifier selection is independent");
 ShareOptions people;people.omitPeople=false;auto peopleText=shareReport({r},people,"0.5.0").dump();check(peopleText.find("PRIVATE-PERSON")!=peopleText.npos&&peopleText.find("PRIVATE-EMAIL")!=peopleText.npos&&peopleText.find("PRIVATE-LOCATION")==peopleText.npos,"Names and nested contact details can be intentionally included");
 check(r.json()==snapshot,"Creating reports does not mutate full evidence");summarizeInsights(r);check(r.json()==snapshot,"Origin/privacy projection is idempotent");
 auto output=root.parent_path()/L"output";fs::create_directories(output);
 for(auto ext:{"json","csv","txt"}){auto dest=output/(std::string("share-summary.")+ext);fs::remove(dest);exportReport(dest,share,ext,{r.path});std::ifstream input(dest);std::string content((std::istreambuf_iterator<char>(input)),{});check(content.find("PRIVATE-")==content.npos&&content.find("Test Camera 5")!=content.npos&&content.find("not a full forensic report")!=content.npos,"All share export formats omit selected personal data and carry the summary limitation");if(std::string_view(ext)=="csv")check(content.find("Batch error count")!=content.npos&&content.find("Cancelled")!=content.npos&&content.find("share_summary")!=content.npos,"CSV share summary preserves batch error/cancellation state and summary mode");}
 check(sha256File(root/L"camera.png",cancel)==before,"Privacy inspection and reports leave original file bytes unchanged");
 std::cout<<"PASS: "<<checks<<" origin/privacy assertions\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<" origin/privacy assertions: "<<e.what()<<"\n";return 1;}}
