#include "previews.hpp"
#include <wincodec.h>
#include <wrl/client.h>
#include <algorithm>
#include <array>
#include <charconv>
#include <map>
#include <stdexcept>
namespace fa {
namespace {
using Microsoft::WRL::ComPtr;
constexpr size_t payloadLimit=8*1024*1024;
constexpr uint64_t pixelLimit=16*1024*1024;
void require(bool v,const char* message){if(!v)throw std::runtime_error(message);}
void checked(HRESULT hr,const char* message){if(FAILED(hr))throw std::runtime_error(std::string(message)+" (HRESULT "+std::to_string(static_cast<uint32_t>(hr))+")");}
struct Apartment {HRESULT hr=CoInitializeEx(nullptr,COINIT_MULTITHREADED);Apartment(){if(FAILED(hr)&&hr!=RPC_E_CHANGED_MODE)checked(hr,"Image COM initialization failed");}~Apartment(){if(SUCCEEDED(hr))CoUninitialize();}};
std::string tail(std::string_view name){auto p=name.find_last_of(':');return std::string(p==name.npos?name:name.substr(p+1));}
bool previewTag(std::string_view name){auto tag=tail(name);return tag=="ThumbnailImage"||tag=="PreviewImage"||tag=="JpgFromRaw"||tag=="OtherImage";}
bool binaryNamespace(std::string_view name){auto family=name.substr(0,name.find(':'));return family=="EXIF"||family=="MakerNotes"||family=="Photoshop"||family=="MPF"||family=="FlashPix"||family=="Composite";}
struct Format {const char* name;const GUID* container;};
Format formatOf(std::span<const unsigned char> b){
 auto at=[&](std::string_view magic){return b.size()>=magic.size()&&std::equal(magic.begin(),magic.end(),b.begin(),[](char a,unsigned char v){return static_cast<unsigned char>(a)==v;});};
 if(at(std::string_view("\x89PNG\r\n\x1a\n",8)))return {"PNG",&GUID_ContainerFormatPng};
 if(b.size()>=3&&b[0]==255&&b[1]==216&&b[2]==255)return {"JPEG",&GUID_ContainerFormatJpeg};
 if(at("GIF87a")||at("GIF89a"))return {"GIF",&GUID_ContainerFormatGif};
 if(at("BM"))return {"BMP",&GUID_ContainerFormatBmp};
 if(at(std::string_view("II\x2a\0",4))||at(std::string_view("MM\0\x2a",4)))return {"TIFF",&GUID_ContainerFormatTiff};
 return {"Unknown",nullptr};
}
struct Decoder {
 Apartment apartment;
 ComPtr<IWICImagingFactory> factory;ComPtr<IWICStream> stream;ComPtr<IWICBitmapDecoder> decoder;ComPtr<IWICBitmapFrameDecode> frame;
 uint32_t width=0,height=0;
 Decoder(std::span<const unsigned char> bytes){
  require(!bytes.empty()&&bytes.size()<=payloadLimit,"Preview compressed-data limit (8 MiB) exceeded or empty payload");auto format=formatOf(bytes);require(format.container!=nullptr,"Preview is not a supported PNG/JPEG/GIF/BMP/TIFF raster");
  checked(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)),"Windows image factory unavailable");checked(factory->CreateStream(&stream),"Image memory stream failed");checked(stream->InitializeFromMemory(const_cast<BYTE*>(bytes.data()),static_cast<DWORD>(bytes.size())),"Image memory stream initialization failed");
  checked(factory->CreateDecoder(*format.container,&GUID_VendorMicrosoft,&decoder),"Microsoft image decoder unavailable");ComPtr<IWICBitmapDecoderInfo> info;checked(decoder->GetDecoderInfo(&info),"Image decoder information unavailable");GUID vendor{};checked(info->GetVendorGUID(&vendor),"Image decoder vendor unavailable");require(vendor==GUID_VendorMicrosoft,"Only Microsoft image codecs are used for previews");
  checked(decoder->Initialize(stream.Get(),WICDecodeMetadataCacheOnDemand),"Malformed embedded image");UINT frames=0;checked(decoder->GetFrameCount(&frames),"Image frame information unavailable");require(frames>0,"Embedded image has no frames");checked(decoder->GetFrame(0,&frame),"Embedded image first frame unavailable");checked(frame->GetSize(&width,&height),"Image dimensions unavailable");require(width&&height,"Embedded image has zero dimensions");
 }
};
bool boundedPixels(uint32_t w,uint32_t h){return w<=16384&&h<=16384&&uint64_t(w)*h<=pixelLimit;}
}
std::vector<unsigned char> decodeBase64(std::string_view input,size_t limit){
 require(input.size()%4==0,"Malformed base64 length");require(input.size()/4<=limit/3+1,"Preview payload exceeds retention budget");std::array<int,256> map;map.fill(-1);std::string alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";for(size_t i=0;i<alphabet.size();++i)map[static_cast<unsigned char>(alphabet[i])]=static_cast<int>(i);
 std::vector<unsigned char> out;out.reserve(std::min(limit,input.size()/4*3));
 for(size_t i=0;i<input.size();i+=4){int a=map[static_cast<unsigned char>(input[i])],b=map[static_cast<unsigned char>(input[i+1])];bool cpad=input[i+2]=='=',dpad=input[i+3]=='=';int c=cpad?0:map[static_cast<unsigned char>(input[i+2])],d=dpad?0:map[static_cast<unsigned char>(input[i+3])];
  require(a>=0&&b>=0&&c>=0&&d>=0&&(!cpad||dpad),"Malformed base64 alphabet or padding");require((!cpad&&!dpad)||i+4==input.size(),"Base64 padding before final quartet");require((!cpad||(b&15)==0)&&(!dpad||cpad||(c&3)==0),"Noncanonical base64 trailing bits");require(out.size()+1+(!cpad?1:0)+(!dpad?1:0)<=limit,"Preview payload exceeds retention budget");out.push_back(static_cast<unsigned char>((a<<2)|(b>>4)));if(!cpad)out.push_back(static_cast<unsigned char>((b<<4)|(c>>2)));if(!dpad)out.push_back(static_cast<unsigned char>((c<<6)|d));
 }
 return out;
}
RasterPixels decodePreview(std::span<const unsigned char> bytes){
 Decoder image(bytes);require(boundedPixels(image.width,image.height),"Preview viewing limit exceeded (16 megapixels / 16384 per axis); exact-byte extraction remains available");ComPtr<IWICFormatConverter> converter;checked(image.factory->CreateFormatConverter(&converter),"Image format converter unavailable");checked(converter->Initialize(image.frame.Get(),GUID_WICPixelFormat32bppBGRA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom),"Image format conversion failed");
 RasterPixels pixels;pixels.width=image.width;pixels.height=image.height;pixels.bgra.resize(size_t(image.width)*image.height*4);checked(converter->CopyPixels(nullptr,image.width*4,static_cast<UINT>(pixels.bgra.size()),pixels.bgra.data()),"Embedded image pixel decoding failed");for(size_t i=0;i<pixels.bgra.size();i+=4){unsigned alpha=pixels.bgra[i+3];for(size_t c=0;c<3;++c)pixels.bgra[i+c]=static_cast<unsigned char>((pixels.bgra[i+c]*alpha+255*(255-alpha)+127)/255);pixels.bgra[i+3]=255;}return pixels;
}
void analyzePreviews(Result& r,const Options& options,std::atomic_bool& cancel){
 r.previews.clear();r.previewStatus="not_evaluated";if(!options.useParser||!r.parser.is_object())return;std::map<std::string,size_t> eligible;bool pointers=false;for(auto it=r.parser.begin();it!=r.parser.end();++it){if(!binaryNamespace(it.key()))continue;auto name=tail(it.key());if(name=="ThumbnailOffset"||name=="ThumbnailLength"||name=="PreviewImageStart"||name=="PreviewImageLength"||name=="JpgFromRawStart"||name=="OtherImageStart")pointers=true;if(previewTag(it.key())&&it.value().is_string()){const auto& value=it.value().get_ref<const std::string&>();if(value.starts_with("(Binary data ")){size_t n=0;auto parsed=std::from_chars(value.data()+13,value.data()+value.size(),n);if(parsed.ec==std::errc())eligible.emplace(it.key(),n);}}}if(eligible.empty()){r.previewStatus=pointers?"not_recovered":metadataReadSucceeded(r)?"no_supported_tags":"not_evaluated";if(pointers)r.errors.push_back("Embedded preview records present but the reader returned no binary payload; inspect metadata warnings.");return;}r.previewStatus="failed";
 try{
  auto process=runPreviewParser(options.exiftool,r.path,cancel,options.parserTimeoutMs);if(!process.err.empty())r.errors.push_back("Embedded preview reader stderr: "+process.err);if(process.exitCode)r.errors.push_back("Embedded preview reader exit code "+std::to_string(process.exitCode));auto parsed=boundedJson(process.out);require(parsed.is_array()&&parsed.size()==1&&parsed[0].is_object(),"Unexpected embedded preview JSON");size_t retained=0;std::map<std::string,bool> strict;
  for(const auto& item:eligible)if(parsed[0].contains(item.first))strict[item.first]=true;
  if(strict.size()<eligible.size()){
   r.errors.push_back("Some embedded previews were refused by the strict reader; attempting raw recovery without establishing image validity. Strict diagnostics remain above.");
   try{auto recovery=runPreviewParser(options.exiftool,r.path,cancel,options.parserTimeoutMs,true);if(!recovery.err.empty())r.errors.push_back("Preview recovery stderr: "+recovery.err);if(recovery.exitCode)r.errors.push_back("Preview recovery exit code "+std::to_string(recovery.exitCode));auto recovered=boundedJson(recovery.out);require(recovered.is_array()&&recovered.size()==1&&recovered[0].is_object(),"Unexpected preview recovery JSON");for(auto it=recovered[0].begin();it!=recovered[0].end();++it)if(eligible.contains(it.key())&&!strict.contains(it.key()))parsed[0][it.key()]=it.value();}catch(const std::exception& e){if(cancel)throw;r.errors.push_back(std::string("Preview recovery: ")+e.what());}
  }
  for(auto it=parsed[0].begin();it!=parsed[0].end();++it){if(cancel)throw std::runtime_error("Cancelled during embedded preview extraction");if(!eligible.contains(it.key()))continue;require(r.previews.size()<8,"Embedded preview count limit (8) exceeded");require(it.value().is_string(),"Unexpected non-string embedded preview payload");const auto& encoded=it.value().get_ref<const std::string&>();PreviewResource resource;resource.field=it.key();
   if(encoded.starts_with("base64:"))resource.bytes=decodeBase64(std::string_view(encoded).substr(7),payloadLimit-retained);else{require(encoded.size()<=payloadLimit-retained,"Embedded preview retention limit (8 MiB total) exceeded");resource.bytes.assign(encoded.begin(),encoded.end());}require(!resource.bytes.empty(),"Empty embedded preview payload");require(resource.bytes.size()==eligible.at(it.key()),"Preview length differs from the reader's binary-data record; exact bytes were not recovered");retained+=resource.bytes.size();resource.format=formatOf(resource.bytes).name;resource.sha256=sha256Bytes(resource.bytes);
   try{Decoder image(resource.bytes);resource.width=image.width;resource.height=image.height;resource.viewable=boundedPixels(resource.width,resource.height);if(!resource.viewable)resource.warning="Dimensions exceed the viewing limit; exact-byte extraction is available.";}catch(const std::exception& e){resource.warning=e.what();}if(!strict.contains(it.key()))resource.warning="Strict reader refused image framing; payload recovered with relaxed extraction. Decoder availability is separate. "+resource.warning;
   r.previews.push_back(std::move(resource));
  }
  require(!r.previews.empty(),"Preview tags were present but no supported payload was returned");require(r.previews.size()==eligible.size(),"Some recorded previews could not be recovered");r.previewStatus="recovered";
 }catch(const std::exception& e){if(!r.previews.empty())r.previewStatus="partial";r.errors.push_back(std::string("Embedded previews: ")+e.what());if(cancel)throw;}
}
size_t previewBytes(const Result& r){size_t bytes=0;for(const auto& preview:r.previews)bytes+=preview.bytes.size();return bytes;}
}
