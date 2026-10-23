#include "previews.hpp"
#include "insights.hpp"
#include <fstream>
#include <iostream>
using namespace fa;
namespace {
int checks=0;void check(bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);}
std::vector<unsigned char> read(const fs::path& path){std::ifstream in(path,std::ios::binary);return {(std::istreambuf_iterator<char>(in)),{}};}
template<class F>bool refused(F f){try{f();return false;}catch(...){return true;}}
}
int wmain(int argc,wchar_t** argv){try{
 check(argc==3,"Expected fixtures and ExifTool");fs::path root=argv[1];Options options;options.exiftool=argv[2];options.useC2pa=false;std::atomic_bool cancel=false;auto expected=read(root/L"embedded-preview.jpg");
 auto plain=analyze(root/L"clean.png",options,cancel);check(plain.previewStatus=="no_supported_tags"&&plain.previews.empty(),"No embedded previews fabricated for a plain image");
 auto text=analyze(root/L"text-preview.png",options,cancel);check(text.previews.empty(),"A textual PreviewImage field is not promoted into binary evidence");
 auto pretend=analyze(root/L"pretend-preview.png",options,cancel);check(pretend.previews.empty(),"A forged binary-data marker in PNG text cannot impersonate a binary preview tag");
 for(auto file:{L"camera-preview.png",L"camera-preview.jpg"}){auto before=sha256File(root/file,cancel);auto result=analyze(root/file,options,cancel);
  check(result.previewStatus=="recovered"&&result.previews.size()==1,"Real ExifTool binary extraction for PNG and JPEG");const auto& image=result.previews[0];
  check(image.bytes==expected&&image.sha256==sha256Bytes(expected),"Recovered JPEG bytes and hash exactly match embedded fixture");
  check(image.width==160&&image.height==96&&image.viewable&&image.format=="JPEG","Microsoft decoder identifies actual preview dimensions");
  auto pixels=decodePreview(image.bytes);check(pixels.width==160&&pixels.height==96&&pixels.bgra.size()==160*96*4,"Native raster decoder produces bounded BGRA pixels");
  check(image.field.find("ThumbnailImage")!=image.field.npos&&image.field.find("EXIF:")==0,"Source namespace retained");auto metadata=result.json()["embedded_previews"]["items"][0];check(metadata["offset"].is_null()&&metadata["size"]==expected.size()&&metadata["sha256"]==image.sha256,"Reports retain size/hash without invented source offsets");
  check(previewBytes(result)==expected.size(),"Resource bytes included in memory accounting");
  auto output=root.parent_path()/L"output";fs::create_directories(output);auto dest=output/(std::wstring(file)+L"-preview.jpg");fs::remove(dest);extractPreview(result,0,dest,&cancel);check(read(dest)==expected,"Preview extraction writes exact snapshot bytes");
  check(refused([&]{extractPreview(result,0,dest);}),"Existing output is protected");check(refused([&]{extractPreview(result,0,root/file);}),"Inspected original is protected");
  cancel=true;check(refused([&]{extractPreview(result,0,output/L"cancelled-preview.jpg",&cancel);}),"Resource export respects cancellation");cancel=false;
  auto report=shareReport({result},{},"0.6.0");check(!report["files"][0].contains("embedded_previews")&&report.dump().find("base64:")==std::string::npos,"Share summary excludes binary resources");check(sha256File(root/file,cancel)==before,"Source bytes unchanged by recovery/viewing/extraction");
 }
 auto bad=analyze(root/L"bad-preview.png",options,cancel);check(bad.previews.size()==1&&!bad.previews[0].viewable&&bad.previews[0].format=="JPEG"&&!bad.previews[0].warning.empty(),"Malformed JPEG payload remains extractable with honest viewing warning");
 auto rejected=analyze(root/L"refused-preview.png",options,cancel);check(rejected.previewStatus=="not_recovered"&&rejected.previews.empty()&&!rejected.errors.empty(),"Reader-refused thumbnail is distinct from no embedded preview records");
 auto unsupported=analyze(root/L"unsupported-preview.png",options,cancel);check(unsupported.previews.size()==1&&!unsupported.previews[0].viewable&&unsupported.previews[0].format=="Unknown"&&unsupported.previews[0].bytes==std::vector<unsigned char>({'n','o','t',' ','a',' ','v','a','l','i','d',' ','i','m','a','g','e',' ','p','a','y','l','o','a','d'}),"Unsupported binary payload is retained exactly without claiming an image decoder succeeded");
 Options native=options;native.useParser=false;auto noReader=analyze(root/L"camera-preview.png",native,cancel);check(noReader.previewStatus=="not_evaluated"&&noReader.previews.empty(),"Unavailable extraction is distinct from no preview tags");
 auto red=decodePreview(read(root/L"clean.png"));check(red.width==1&&red.height==1&&red.bgra==std::vector<unsigned char>({0,0,255,255}),"PNG pixel interpretation verified");
 for(auto large:{L"oversize-preview.png",L"pixel-budget-preview.png"}){std::string error;try{decodePreview(read(root/large));}catch(const std::exception& e){error=e.what();}check(error.find("viewing limit exceeded")!=std::string::npos,"Declared image dimensions are refused before pixel decompression");}
 check(refused([&]{decodePreview(std::vector<unsigned char>{'M','Z','!'});}),"Non-image executable bytes are never opened or executed");
 check(decodeBase64("AAEC/w==",4)==std::vector<unsigned char>({0,1,2,255}),"Binary zero and non-UTF8 bytes survive base64");
 for(auto malformed:{"a","AA=A","AA==AAAA","AB==","AAB=","????","===="})check(refused([&]{decodeBase64(malformed,100);}),"Malformed base64 rejected");
 check(refused([&]{decodeBase64("AAAA",2);}),"Payload budget enforced before retention");
 std::cout<<"PASS: "<<checks<<" embedded preview assertions\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<" embedded preview assertions: "<<e.what()<<"\n";return 1;}}
