#include "core.hpp"
#include <iostream>
using namespace fa;
namespace {
int checks=0;void check(bool ok,const std::string& message){++checks;if(!ok)throw std::runtime_error(message);}
bool tag(const Result& r,std::string_view name,std::string_view expected){for(const auto& field:r.fields){auto end=field.name.substr(field.name.find_last_of(':')+1);if(end==name&&display(field.value)==expected)return true;}return false;}
struct Sample {const wchar_t* name;const char* native;const char* type;const char* probe;const char* value;};
}
int wmain(int argc,wchar_t** argv){try{
 check(argc==3,"Expected fixtures and ExifTool");fs::path root=argv[1];Options options;options.exiftool=argv[2];options.useC2pa=false;std::atomic_bool cancel=false;
 Sample samples[]{
 {L"clean.png","PNG","PNG","ImageWidth","1"},{L"camera-preview.jpg","JPEG","JPEG","Make","Fixture Camera Co"},
 {L"upstream-ExifTool.tif","TIFF","TIFF","Make","Canon"},{L"upstream-RIFF.webp","WebP","Extended WEBP","ImageWidth","1"},
 {L"upstream-GIF.gif","GIF","GIF","Title","GIF with XMP"},{L"upstream-BMP.bmp","BMP","BMP","ImageWidth","8"},
 {L"upstream-QuickTime.heic","HEIF container","HEIF","ImageWidth","1596"},{L"upstream-QuickTime.mov","QuickTime container candidate","MOV","Make","PENTAX DIGITAL CAMERA"},
 {L"fixture.mp4","ISO BMFF container","MP4","ImageWidth","320"},{L"upstream-Matroska.mkv","EBML container","MKV","ImageWidth","704"},
 {L"upstream-MP3.mp3","ID3 (audio type unconfirmed)","MP3","Title","ExifTool Test"},{L"upstream-FLAC.flac","FLAC","FLAC","Title","ExifTool test"},
 {L"fixture.wav","WAV","WAV","NumChannels","1"},{L"fixture.pdf","PDF","PDF","Author","Fixture author"},
 {L"fixture.docx","ZIP container","DOCX","Creator","Fixture author"},{L"fixture.zip","ZIP container","ZIP","FileTypeExtension","zip"}
 };
 for(const auto& sample:samples){auto path=root/sample.name;check(fs::exists(path),"Missing public format fixture; run bootstrap-dependencies.ps1");auto before=sha256File(path,cancel);auto result=analyze(path,options,cancel);
  check(result.format==sample.native,"Native identification: "+utf8(sample.name)+" (observed "+result.format+")");
  check(result.parser.is_object()&&tag(result,"FileType",sample.type),"Parser file type: "+utf8(sample.name));
  check(tag(result,sample.probe,sample.value),"Actual metadata field: "+utf8(sample.name)+" / "+sample.probe);
  check(before==sha256File(path,cancel),"Unchanged format fixture: "+utf8(sample.name));
  std::cout<<utf8(sample.name)<<" | "<<result.format<<" | "<<sample.type<<" | "<<sample.probe<<" = "<<sample.value<<" | "<<result.errors.size()<<" finding(s)\n";
 }
 auto pe=analyze(argv[0],options,cancel);check(pe.format=="PE"&&tag(pe,"FileType","Win64 EXE"),"PE executable inspected as data only");
 std::cout<<"PASS: "<<checks<<" format coverage assertions\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<" format assertions: "<<e.what()<<"\n";return 1;}}
