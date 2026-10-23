#include "investigation.hpp"
#include <iostream>
using namespace fa;
namespace {int checks=0;void check(bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);}}
int main(){try{
 std::atomic_bool cancel=false;std::vector<Result> files(3);files[0].path=L"photo-画像.jpg";files[1].path=L"second.png";files[2].path=L"empty.bin";
 files[0].fields.push_back({"Photo / device","Camera model","EXIF","","Fujifilm X-T5",nullptr,{},{}});
 files[1].fields.push_back({"XMP","XMP:CreatorTool","ExifTool","","ÉDITEUR 画像",nullptr,{},{}});
 files[1].fields.push_back({"GPS","EXIF:GPSLatitude","ExifTool","","0 deg 0'",0,{},{}});
 files[1].fields.push_back({"XMP","Duplicate creator","ExifTool","","Other creator",Json{{"nested",Json::array({"Needle in object"})}},{},{}});
 files[1].generation.push_back({{"name","Prompt"},{"value","a red boat"},{"source_field","PNG:parameters"}});files[1].errors.push_back("Parser timed out");
 PreviewResource p;p.field="EXIF:ThumbnailImage";p.format="JPEG";p.sha256="abc123";files[0].previews.push_back(p);
 auto before=files[1].json();auto device=searchBatch(files,L"FUJIFILM","",cancel);check(device.matches.size()==1&&device.matches[0].file==0&&device.matches[0].kind==MatchKind::Field,"Device query returns original field reference");
 auto unicode=searchBatch(files,L"éditeur 画像","",cancel);check(unicode.matches.size()==1&&unicode.matches[0].file==1,"Unicode case-insensitive search preserves non-Latin text");
 auto raw=searchBatch(files,L"needle","",cancel);check(raw.matches.size()==1&&raw.matches[0].index==2,"Nested raw evidence is searchable");
 check(searchBatch(files,L"GPS","",cancel).matches.size()==1,"GPS field names searchable even for zero coordinates");
 check(searchBatch(files,L"EXIF","Photo / device",cancel).matches.size()==1,"Category and source filters combine");
 check(searchBatch(files,L"FUJIFILM","XMP",cancel).matches.empty(),"Category filter excludes unrelated matches");
 check(searchBatch(files,L"red boat","",cancel).matches[0].kind==MatchKind::Generation,"Generation values searched");
 check(searchBatch(files,L"timed out","",cancel).matches[0].kind==MatchKind::Error,"Incomplete-analysis errors remain searchable");
 check(searchBatch(files,L"abc123","",cancel).matches[0].kind==MatchKind::Preview,"Preview evidence references searched without pixel data");
 check(searchBatch(files,L"photo-画像","",cancel).matches[0].kind==MatchKind::File,"Unicode filenames searched");
 auto empty=searchBatch(files,L"","",cancel,2);check(empty.truncated&&empty.matches.size()==2,"Result budget reports truncation instead of silently dropping matches");
 cancel=true;auto stopped=searchBatch(files,L"needle","",cancel);check(stopped.cancelled&&stopped.matches.empty()&&stopped.scannedFiles==0,"Precancelled search stops before scanning");cancel=false;
 check(searchBatch(files,L"does not exist","",cancel).scannedFiles==3,"No-match search still reports completed files");
 std::string longText(70000,'x');longText.replace(65534,6,"NEEDLE");files[0].fields[0].value=longText;auto boundary=searchBatch(files,L"needle","",cancel);check(boundary.matches.size()==2&&boundary.matches[0].snippet.find("NEEDLE")!=std::string::npos&&boundary.matches[0].snippet.size()<1200,"Long-string chunk boundary matches retain bounded context");
 check(files[1].json()==before,"Search does not mutate full evidence");check(compactValue(Json{{"key",std::string(100000,'x')}}).size()<100,"Table preview never serializes a large structured object");
 check(compactText("ééé",3).starts_with("é…"),"Compact table text never splits a UTF-8 code point");
 bool refused=false;try{searchBatch(files,std::wstring(513,L'x'),"",cancel);}catch(...){refused=true;}check(refused,"Overlong query refused explicitly");
 std::cout<<"PASS: "<<checks<<" batch search assertions\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<" batch search assertions: "<<e.what()<<"\n";return 1;}}
