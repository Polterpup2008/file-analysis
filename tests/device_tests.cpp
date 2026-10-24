#include "presentation.hpp"
#include "insights.hpp"
#include "device_catalog_snapshot.hpp"
#include <fstream>
#include <iostream>

using namespace fa;
namespace {
int checks=0;
void check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
void field(Result& r,std::string name,Json value,Json numeric=nullptr){r.fields.push_back({"EXIF",std::move(name),"ExifTool","",std::move(value),std::move(numeric),{},{}});}
Result recorded(std::string make,std::string model){Result r;r.format="JPEG";r.parser=Json::object();if(!make.empty())field(r,"EXIF:IFD0:Main:Make",make,make);if(!model.empty())field(r,"EXIF:IFD0:Main:Model",model,model);return r;}
const Json& recognition(const Result& r){return r.photo.at("recognition");}
bool names(const Result& r,std::string_view name){return recognition(r)["names"].dump().find(name)!=std::string::npos;}
bool finding(const Result& r,std::string_view text){for(const auto& f:simpleFindings(r))if((f.title+f.value+f.detail).find(text)!=std::string::npos)return true;return false;}
}
int wmain(int argc,wchar_t** argv){try{
 check(argc==4,"Expected fixtures, ExifTool and the pinned model catalogue");
 fs::path root=argv[1],catalog=argv[3];Options options;options.exiftool=argv[2];options.useC2pa=false;std::atomic_bool cancel=false;
 struct Sample{const char* make;const char* code;const char* expected;};
 for(const auto& s:std::vector<Sample>{{"Samsung","SM-S918B","Galaxy S23 Ultra"},{"Apple","iPhone15,2","iPhone 14 Pro"},{"Google","Pixel 8","Pixel 8"},{"Xiaomi","23021RAAEG","Redmi Note 12"},{"OnePlus","CPH2581","OnePlus 12"},{"Huawei","VOG-L29","P30 Pro"},{"LGE","LM-G820","LG G8 ThinQ"}}){
  auto r=recorded(s.make,s.code);summarizePhoto(r,catalog);
  check(names(r,s.expected),"An exact recorded manufacturer/model resolves to the sourced retail name");
  check(recognition(r)["status"]=="matched"&&recognition(r)["catalogue"]["status"]=="verified","Exact lookup is explicit and its snapshot is verified");
  check(recognition(r)["entries"][0]["recorded_value"]==s.code,"Friendly names do not replace the original model evidence");
  check(finding(r,s.expected)&&finding(r,s.code),"Simple view includes both the readable name and recorded code");
  check(recognition(r)["entries"][0]["names"][0]["basis"]=="exact_manufacturer_and_model","Recognition states how a match was made");
 }
 auto samsung=recorded("Samsung","SM-S918B");summarizePhoto(samsung,catalog);summarizeInsights(samsung);
 check(recognition(samsung)["catalogue"]["records"]==deviceCatalogRows&&recognition(samsung)["catalogue"]["sha256"].get<std::string>()==deviceCatalogSha256,"Full report retains the pinned catalogue identity");
 auto before=samsung.json();summarizePhoto(samsung,catalog);summarizeInsights(samsung);check(samsung.json()==before,"Refreshing device recognition is idempotent");
 auto share=shareReport({samsung},{},"1.1.0");
 check(share["schema_version"]==6&&share["files"][0]["recognized_device_names"][0]["name"]=="Galaxy S23 Ultra","Sharing keeps the useful recognized device name");
 check(share.dump().find("EXIF:IFD0:Main:Model")==std::string::npos&&share.dump().find(std::string(deviceCatalogSha256))==std::string::npos,"Share summary does not copy whole raw recognition records");
 auto folded=recorded(" SAMSUNG "," sm-s918b ");summarizePhoto(folded,catalog);check(names(folded,"Galaxy S23 Ultra")&&recognition(folded)["entries"][0]["recorded_value"]==" sm-s918b ","ASCII casing/outer whitespace normalize for lookup while raw values survive");
 auto noMake=recorded("","SM-S918B");summarizePhoto(noMake,catalog);check(names(noMake,"Galaxy S23 Ultra")&&recognition(noMake)["entries"][0]["names"][0]["basis"]=="exact_model_without_manufacturer","An unambiguous model-only lookup is labeled without inventing a recorded manufacturer");
 auto mismatch=recorded("Unrelated Manufacturer","SM-S918B");summarizePhoto(mismatch,catalog);check(recognition(mismatch)["names"].empty()&&recognition(mismatch)["entries"][0]["status"]=="manufacturer_mismatch","A known code cannot override a contradictory recorded manufacturer");
 for(auto code:{"SM-S918B-extra","DSC-NOT-A-REAL-MODEL","iPhone999,999"}){auto r=recorded("Samsung",code);summarizePhoto(r,catalog);check(recognition(r)["names"].empty()&&finding(r,code),"Unlisted codes remain explicit and are never prefix/fuzzy guesses");}
 auto ambiguous=recorded("","A1");summarizePhoto(ambiguous,catalog);check(recognition(ambiguous)["status"]=="multiple_matches"&&recognition(ambiguous)["names"].size()>1,"A shared model code is shown as multiple candidates");check(recognition(ambiguous)["entries"][0]["names"].size()==8&&recognition(ambiguous)["truncated"]==true,"Candidate limits are explicit instead of silently choosing one name");
 auto conflicting=recorded("Samsung","SM-S918B");field(conflicting,"XMP:XMP-tiff:Main:Make","Apple","Apple");field(conflicting,"XMP:XMP-tiff:Main:Model","iPhone15,2","iPhone15,2");summarizePhoto(conflicting,catalog);
 check(names(conflicting,"Galaxy S23 Ultra")&&names(conflicting,"iPhone 14 Pro"),"Manufacturer/model pairs use the same source group when claims conflict");check(recognition(conflicting)["status"]=="multiple_matches"&&finding(conflicting,"Capture metadata disagreement"),"Both contradictory identities remain visible");
 auto longValue=recorded("Samsung",std::string(300,'X'));summarizePhoto(longValue,catalog);check(recognition(longValue)["entries"][0]["status"]=="lookup_value_limit"&&recognition(longValue)["names"].empty(),"Oversized model values are retained but not used for lookup");check(recognition(longValue)["entries"][0]["recorded_value"].is_null()&&longValue.photo["properties"][1]["sources"][0]["value"].get<std::string>().size()==300,"Large model values are not multiplied into lookup records; original source remains complete");
 auto ids=recorded("Canon","");field(ids,"MakerNotes:Canon:Main:CanonModelID","EOS 20D",0x80000175u);summarizePhoto(ids,catalog);summarizeInsights(ids);
 check(ids.photo["device_status"]=="model_identifier_reported"&&names(ids,"EOS 20D"),"A decoded Canon body ID works without a standard Model tag");check(finding(ids,"EOS 20D")&&ids.origin["assessment"]=="Capture device recorded in metadata","Recovered vendor identity reaches Simple and origin summaries");check(recognition(ids)["entries"][0]["raw_numeric"]==0x80000175u,"Numeric body IDs remain available beside decoded names");
 for(const auto& sample:std::vector<std::pair<std::string,std::string>>{{"MakerNotes:Sony:Main:SonyModelID","ILCE-7M4"},{"MakerNotes:Pentax:Main:PentaxModelID","K-3"},{"MakerNotes:Olympus:Main:CameraType2","E-M1"}}){auto r=recorded("","");field(r,sample.first,sample.second,123);summarizePhoto(r,catalog);check(names(r,sample.second),"Other allowlisted vendor body identifiers are retained and decoded");}
 for(const auto& maker:{"PENTAX Corporation","RICOH IMAGING COMPANY, LTD.","Samsung"}){auto r=recorded(maker,"");field(r,"MakerNotes:Pentax:Main:PentaxModelID","K10D",76830);summarizePhoto(r,catalog);check(recognition(r)["names"][0]["brand"]==""&&recognition(r)["names"][0]["display_name"]=="K10D"&&recognition(r)["names"][0]["vendor_namespace"]=="Pentax","A decoded maker-note name does not invent a manufacturer/model combination");}
 auto structuredId=recorded("Canon","");field(structuredId,"MakerNotes:Canon:Main:CanonModelID","EOS 20D",Json{{"unexpected","value"}});summarizePhoto(structuredId,catalog);check(recognition(structuredId)["names"].empty(),"Structured raw values are not mistaken for decoded numeric identifiers");
 auto unknownId=recorded("Canon","");field(unknownId,"MakerNotes:Canon:Main:CanonModelID","Unknown (0xffff)",65535);summarizePhoto(unknownId,catalog);check(recognition(unknownId)["names"].empty()&&recognition(unknownId)["entries"][0]["status"]=="unresolved_model_id","Unknown vendor identifiers are not promoted to retail model names");
 for(auto tag:{"ICC_Profile:ICC-header:Main:Model","EXIF:IFD1:Main:Model","EXIF:IFD0:Doc1:Model","XMP:XMP-xmp:Main:Model","MakerNotes:Sony:Main:CanonModelID","MakerNotes:Canon:Main:CameraType"}){auto r=recorded("","");field(r,tag,"SM-S918B",42);summarizePhoto(r,catalog);check(recognition(r)["names"].empty(),"Profiles, thumbnails, embedded documents, wrong namespaces and body-type enums do not identify the capture device");}
 auto dng=recorded("Nikon","");field(dng,"EXIF:IFD0:Main:UniqueCameraModel","Nikon D850","Nikon D850");summarizePhoto(dng,catalog);check(dng.photo["device_status"]=="model_reported"&&finding(dng,"Nikon D850"),"DNG UniqueCameraModel is retained as a recorded model without a guessed alias");
 auto many=recorded("Samsung","");for(int i=0;i<100;++i)field(many,"EXIF:IFD0:Main:Copy"+std::to_string(i)+":Model","SM-S918B","SM-S918B");summarizePhoto(many,catalog);check(recognition(many)["entries"].size()==64&&recognition(many)["truncated"]==true,"Duplicate-source expansion has a visible input bound");
 auto absent=recorded("","");summarizePhoto(absent,catalog);check(recognition(absent)["status"]=="not_reported","Missing camera evidence is distinct from a failed model-name lookup");
 absent.parser=nullptr;summarizePhoto(absent,catalog);check(recognition(absent)["status"]=="not_evaluated","Unavailable metadata extraction is never described as absent device tags");
 auto unavailable=recorded("Samsung","SM-S918B");summarizePhoto(unavailable,root/L"does-not-exist-model-catalog.tsv");check(recognition(unavailable)["names"].empty()&&finding(unavailable,"Model catalogue unavailable")&&finding(unavailable,"SM-S918B"),"Missing catalogue does not remove recorded device evidence or hide lookup failure");
 auto bad=root.parent_path()/L"output"/L"device-catalog-corrupt.tsv";fs::create_directories(bad.parent_path());{std::ofstream out(bad,std::ios::binary);out<<"tampered catalogue";}
 summarizePhoto(unavailable,bad);check(recognition(unavailable)["catalogue"]["status"]=="fingerprint_mismatch"&&recognition(unavailable)["names"].empty(),"Modified catalogue bytes fail closed before name parsing");
 for(auto file:{L"device-samsung.png",L"device-apple.png",L"device-xiaomi.png",L"device-samsung.jpg",L"device-samsung.tiff",L"device-canon-id.png",L"device-sony-id.png"}){
  auto path=root/file;auto hash=sha256File(path,cancel);auto r=analyze(path,options,cancel);
  check(!recognition(r)["names"].empty(),"Real parser output reaches name lookup across PNG/JPEG/TIFF and Canon maker notes");check(sha256File(path,cancel)==hash,"Device recognition leaves original bytes unchanged");
  if(std::wstring_view(file)==L"device-canon-id.png")check(names(r,"EOS 20D")&&r.photo["device_status"]=="model_identifier_reported","Actual Canon maker-note decoding is used when Model is missing");
 }
 Json observations=Json::array();size_t recoveredIds=0;
 for(auto file:{L"device-upstream-Canon.jpg",L"device-upstream-Canon1DmkIII.jpg",L"device-upstream-Sony.jpg",L"device-upstream-Pentax.jpg",L"device-upstream-OlympusE1.jpg",L"device-upstream-Nikon.nef",L"device-upstream-Panasonic.rw2",L"device-upstream-FujiFilm.raf",L"device-upstream-Apple.jpg",L"device-upstream-Google.jpg"}){
  auto path=root/file;auto hash=sha256File(path,cancel);auto r=analyze(path,options,cancel);
  check(r.photo.is_object()&&r.photo["device_status"]!="not_reported"&&r.photo["device_status"]!="not_evaluated","Upstream camera/phone/RAW metadata provides actual device evidence");check(sha256File(path,cancel)==hash,"Real metadata fixture bytes remain unchanged");
  for(const auto& entry:recognition(r)["entries"])if(entry["status"]=="matched"&&entry["names"][0]["basis"]=="reader_decoded_model_id")++recoveredIds;
  observations.push_back({{"file",utf8(path.filename().wstring())},{"photo",r.photo}});
 }
 check(recoveredIds>=3,"Several real vendor fixtures exercise decoded model-ID recovery");
 auto output=root.parent_path()/L"output";{std::ofstream out(output/L"device-real-observations.json");out<<observations.dump(2);}
 auto report=Json{{"schema_version",6},{"files",Json::array({samsung.json(),conflicting.json(),ids.json()})}};
 for(auto ext:{"json","csv","txt"}){auto dest=output/(std::string("device-report.")+ext);fs::remove(dest);exportReport(dest,report,ext);std::ifstream in(dest);std::string text((std::istreambuf_iterator<char>(in)),{});check(text.find("Galaxy S23 Ultra")!=text.npos&&text.find("iPhone 14 Pro")!=text.npos&&text.find("SM-S918B")!=text.npos,"All full exports preserve friendly names, original codes and conflicting claims");}
 std::cout<<"PASS: "<<checks<<" device recognition assertions\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<" device assertions: "<<e.what()<<"\n";return 1;}}
