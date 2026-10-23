#include "presentation.hpp"
#include <fstream>
#include <iostream>
using namespace fa;
namespace {
int checks=0;
void check(bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);}
const Json& property(const Result& r,std::string_view key){for(const auto& p:r.photo.at("properties"))if(p.at("key").get<std::string>()==key)return p;throw std::runtime_error("Missing photo property: "+std::string(key));}
bool finding(const Result& r,std::string_view text){for(const auto& f:simpleFindings(r))if((f.title+f.value+f.detail).find(text)!=std::string::npos)return true;return false;}
void embedded(Result& r,std::string name,Json value){r.fields.push_back({"EXIF",std::move(name),"ExifTool","",std::move(value),nullptr,{},{}});}
}
int wmain(int argc,wchar_t** argv){try{
 check(argc==3,"Expected fixtures and ExifTool");fs::path root=argv[1];Options options;options.exiftool=argv[2];options.useC2pa=false;std::atomic_bool cancel=false;
 for(auto file:{L"camera.png",L"camera.jpg",L"camera.tiff"}) {
  auto before=sha256File(root/file,cancel);auto r=analyze(root/file,options,cancel);
  check(r.photo["device_status"]=="model_reported","Real parser recovers capture device across PNG/JPEG/TIFF");
  check(property(r,"model")["values"][0]=="Fixture Phone 4","Exact stored device model retained");
  check(property(r,"make")["values"][0]=="Fixture Camera Co","Device manufacturer retained");
  check(property(r,"lens_model")["values"][0]=="35mm F2.8 fixture lens","Lens model retained");
  check(property(r,"capture_time")["values"][0]=="2024:06:15 12:34:56"&&property(r,"capture_offset")["values"][0]=="+02:00","Capture date and timezone remain separate from filesystem times");
  check(property(r,"capture_subseconds")["values"][0]=="123","Fractional capture seconds retained");
  check(property(r,"iso")["values"][0]=="200"&&finding(r,"1/125 s")&&finding(r,"f/2.8"),"Shooting settings interpreted for Simple");
  check(property(r,"flash")["sources"][0]["numeric"]==0,"Zero-valued flash enum retains its numeric evidence");
  check(finding(r,"GPS location recorded")&&finding(r,"0 deg"),"Zero latitude and longitude retained");
  check(finding(r,"FIXTURE-ONLY-001")&&finding(r,"FIXTURE-LENS-002"),"Body and lens identifiers available");
  check(finding(r,"do not prove")&&finding(r,"Local-path redaction does not remove GPS"),"Device attribution and sharing limits are visible");
  check(property(r,"model")["sources"][0]["field"].get<std::string>().starts_with("EXIF:"),"Summary links back to exact original parser field");
  check(r.json()["photo"]==r.photo,"Structured capture evidence retained in JSON");
  check(sha256File(root/file,cancel)==before,"Camera analysis leaves source bytes unchanged");
 }
 auto conflict=analyze(root/L"camera-conflict.png",options,cancel);
 check(property(conflict,"model")["conflict"]==true&&property(conflict,"model")["values"].size()==2,"Contradictory EXIF and XMP device models both retained");
 check(!conflict.photo["warnings"].empty()&&finding(conflict,"Capture metadata disagreement"),"Capture disagreement surfaced in Simple and exported evidence");
 auto xmp=analyze(root/L"camera-xmp-only.png",options,cancel);
 check(property(xmp,"model")["values"][0]=="Conflicting Fixture Model","XMP-only camera information supported");
 auto clean=analyze(root/L"clean.png",options,cancel);
 check(clean.photo["device_status"]=="not_reported"&&finding(clean,"Not recovered from embedded metadata"),"Stripped image does not receive a guessed device");
 check(!finding(clean,"AI generation claim"),"Missing device metadata does not establish AI generation");
 Options native=options;native.useParser=false;auto unavailable=analyze(root/L"camera.png",native,cancel);
 check(unavailable.photo["device_status"]=="not_evaluated"&&finding(unavailable,"metadata reader unavailable"),"Disabled extraction is distinguished from absent tags");
 auto unknown=analyze(root/L"unknown.bin",options,cancel);check(unknown.photo.is_null(),"Unrelated files do not receive photo findings");
 Result synthetic;synthetic.format="JPEG";synthetic.parser=Json::object();
 for(auto name:{"ICC_Profile:ICC-header:Model","ExifTool:Model","Composite:Model","PNG:Model","EXIF:IFD1:Main:Model","EXIF:IFD0:Doc1:Model","JUMBF:Model"})embedded(synthetic,name,"Not the capture camera");
 embedded(synthetic,"EXIF:ExifIFD:Main:LensModel","Some Lens");embedded(synthetic,"XMP:XMP-xmp:CreatorTool","Some Phone editor");summarizePhoto(synthetic);
 check(synthetic.photo["device_status"]=="not_reported","ICC, diagnostic, composite, thumbnail, ingredient and arbitrary model tags ignored");
 check(!finding(synthetic,"Camera / device reportedNot the capture camera"),"Lens and editor do not become a camera body");
 embedded(synthetic,"EXIF:IFD0:Main:Make","Maker");summarizePhoto(synthetic);
 check(synthetic.photo["device_status"]=="manufacturer_only"&&finding(synthetic,"device model not recovered"),"Manufacturer-only result is honest about missing model");
 embedded(synthetic,"EXIF:IFD0:Main:Model","  Test Camera  ");embedded(synthetic,"XMP:XMP-tiff:Main:Model","test camera");summarizePhoto(synthetic);
 check(property(synthetic,"model")["values"].size()==1&&property(synthetic,"model")["sources"].size()==2&&!property(synthetic,"model")["conflict"].get<bool>(),"Equivalent spellings merge display values while preserving both sources");
 auto stable=synthetic.json();summarizePhoto(synthetic);check(synthetic.json()==stable,"Refreshing capture summary is idempotent");
 Result quicktime;quicktime.format="QuickTime container";quicktime.parser=Json::object();embedded(quicktime,"QuickTime:Keys:Main:AndroidMake","Fixture Android");embedded(quicktime,"QuickTime:Keys:Main:AndroidModel","Device Code 04");summarizePhoto(quicktime);
 check(property(quicktime,"model")["values"][0]=="Device Code 04","QuickTime Android device fields retain model codes without guessed marketing names");
 auto output=root.parent_path()/L"output";fs::create_directories(output);Json report={{"files",Json::array({conflict.json()})},{"batch_errors",Json::array()}};
 for(auto ext:{"json","csv","txt"}){auto dest=output/(std::string("photo-report.")+ext);fs::remove(dest);exportReport(dest,report,ext,{conflict.path});std::ifstream in(dest);std::string content((std::istreambuf_iterator<char>(in)),{});check(content.find("Fixture Phone 4")!=content.npos&&content.find("Conflicting Fixture Model")!=content.npos,"All report formats retain both conflicting camera claims");}
 if(fs::exists(root/L"real-Canon.jpg")){auto canon=analyze(root/L"real-Canon.jpg",options,cancel);check(property(canon,"model")["values"][0]=="Canon EOS DIGITAL REBEL"&&finding(canon,"Canon EOS DIGITAL REBEL"),"Real Canon sample identifies reported camera");check(property(canon,"capture_time")["values"][0]=="2003:12:04 06:46:52"&&finding(canon,"No timezone is assumed"),"Real capture time is not silently relabeled UTC");}
 std::cout<<"PASS: "<<checks<<" photo assertions\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<" photo assertions: "<<e.what()<<"\n";return 1;}}
