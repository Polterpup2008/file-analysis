#include "core.hpp"
#include "format_registry.hpp"
#include <bcrypt.h>
#include <algorithm>
#include <array>
#include <fstream>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
namespace fa {
namespace {
constexpr uint64_t MiB=1024*1024;
void require(bool ok,const std::string& message) { if(!ok) throw std::runtime_error(message); }
std::string lower(std::string s) { std::transform(s.begin(),s.end(),s.begin(),[](unsigned char c){return char(std::tolower(c));}); return s; }
std::string hex(const unsigned char* p,size_t n) { constexpr char digits[]="0123456789abcdef"; std::string s; s.reserve(n*2); for(size_t i=0;i<n;++i){s+=digits[p[i]>>4];s+=digits[p[i]&15];}return s; }
struct Hash {
 BCRYPT_ALG_HANDLE alg=nullptr; BCRYPT_HASH_HANDLE hash=nullptr;
 explicit Hash(LPCWSTR name) {
  require(BCryptOpenAlgorithmProvider(&alg,name,nullptr,0)>=0,"Hash provider failed");
  if(BCryptCreateHash(alg,&hash,nullptr,0,nullptr,0,0)<0) { BCryptCloseAlgorithmProvider(alg,0); throw std::runtime_error("Hash creation failed"); }
 }
 ~Hash(){if(hash)BCryptDestroyHash(hash);if(alg)BCryptCloseAlgorithmProvider(alg,0);}
 void add(const unsigned char* p,size_t n){require(BCryptHashData(hash,const_cast<PUCHAR>(p),static_cast<ULONG>(n),0)>=0,"Hash update failed");}
 std::string finish(){DWORD n=0,got=0;require(BCryptGetProperty(alg,BCRYPT_HASH_LENGTH,reinterpret_cast<PUCHAR>(&n),sizeof(n),&got,0)>=0,"Hash length failed");std::vector<unsigned char>b(n);require(BCryptFinishHash(hash,b.data(),n,0)>=0,"Hash finish failed");return hex(b.data(),b.size());}
};
struct Reader {
 Handle file; uint64_t size=0; std::atomic_bool& cancel; ULONGLONG start=GetTickCount64(); DWORD deadline=120000;
 std::array<unsigned char,65536> cache{}; uint64_t cacheAt=UINT64_MAX; size_t cacheN=0;
 Reader(const fs::path& path,std::atomic_bool& stop) : file(CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT|FILE_FLAG_SEQUENTIAL_SCAN,nullptr)),cancel(stop) {
  require(file.value!=INVALID_HANDLE_VALUE,"Cannot open file for read (Win32 "+std::to_string(GetLastError())+")");
  FILE_ATTRIBUTE_TAG_INFO info{}; require(GetFileInformationByHandleEx(file,FileAttributeTagInfo,&info,sizeof(info))!=0,"Cannot inspect file attributes");
  require(!(info.FileAttributes&(FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_DIRECTORY)),"Directories and reparse points are not analyzed as files");
  require(GetFileType(file)==FILE_TYPE_DISK,"Only regular disk files are accepted");
  LARGE_INTEGER n{};require(GetFileSizeEx(file,&n)!=0,"Cannot read file size");size=static_cast<uint64_t>(n.QuadPart);
 }
 void check(){require(!cancel,"Cancelled"); require(GetTickCount64()-start<deadline,"Native analysis time limit exceeded; results incomplete");}
 void read(uint64_t off,unsigned char* out,size_t n) {
  check(); require(off<=size && n<=size-off,"Truncated structure or out-of-bounds read");
  LARGE_INTEGER pos{};pos.QuadPart=static_cast<LONGLONG>(off);require(SetFilePointerEx(file,pos,nullptr,FILE_BEGIN)!=0,"Seek failed");
  DWORD got=0;require(n<=MAXDWORD && ReadFile(file,out,static_cast<DWORD>(n),&got,nullptr) && got==n,"File read failed");
 }
 std::vector<unsigned char> read(uint64_t off,size_t n){std::vector<unsigned char> b(n);if(n)read(off,b.data(),n);return b;}
 unsigned char byte(uint64_t off){if(cacheAt==UINT64_MAX || off<cacheAt || off-cacheAt>=cacheN){cacheAt=off;cacheN=static_cast<size_t>(std::min<uint64_t>(cache.size(),size-off));read(off,cache.data(),cacheN);}require(cacheN!=0,"Unexpected end of file");return cache[static_cast<size_t>(off-cacheAt)];}
 std::string hash(uint64_t off,uint64_t length){Hash h(BCRYPT_SHA256_ALGORITHM);std::array<unsigned char,65536>b;for(uint64_t p=0;p<length;){size_t n=static_cast<size_t>(std::min<uint64_t>(b.size(),length-p));read(off+p,b.data(),n);h.add(b.data(),n);p+=n;}return h.finish();}
};
uint32_t be32(const unsigned char* b){return uint32_t(b[0])<<24|uint32_t(b[1])<<16|uint32_t(b[2])<<8|b[3];}
std::string timeUtc(FILETIME f) { SYSTEMTIME t{}; if(!FileTimeToSystemTime(&f,&t))return "Unavailable";char b[64];sprintf_s(b,"%04u-%02u-%02uT%02u:%02u:%02u.%03uZ",t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond,t.wMilliseconds);return b; }
void field(Result& r,std::string category,std::string name,Json value,std::string source="Native",Json raw=nullptr,std::optional<uint64_t> off={},std::optional<uint64_t> size={},std::string warning={}){
 r.fields.push_back({std::move(category),std::move(name),std::move(source),std::move(warning),std::move(value),std::move(raw),off,size});
}
void block(Result& r,Reader& f,std::string type,uint64_t off,uint64_t size,std::string note={}){
 require(r.blocks.size()<20000,"Structure block limit (20000) reached");
 r.blocks.push_back({std::move(type),std::move(note),f.hash(off,size),off,size});
}
std::string latin(const unsigned char* b,size_t n) { std::string s;for(size_t i=0;i<n;++i){auto c=b[i];if(c<128)s+=char(c);else{s+=char(0xc0|(c>>6));s+=char(0x80|(c&63));}}return s; }
uint32_t crcStep(uint32_t crc,const unsigned char* b,size_t n){
 static const auto table=[] {std::array<uint32_t,256> t{};for(uint32_t i=0;i<256;++i){uint32_t c=i;for(int j=0;j<8;++j)c=(c>>1)^((c&1)?0xedb88320u:0);t[i]=c;}return t;}();
 for(size_t i=0;i<n;++i)crc=table[(crc^b[i])&255]^(crc>>8);return crc;
}
std::string detect(Reader& f) {
 auto b=f.read(0,static_cast<size_t>(std::min<uint64_t>(f.size,4096)));
 auto at=[&](size_t off,std::string_view s){return off<=b.size() && s.size()<=b.size()-off && memcmp(s.data(),b.data()+off,s.size())==0;};
 if(at(0,std::string_view("\x89PNG\r\n\x1a\n",8)))return "PNG";
 if(b.size()>=3&&b[0]==255&&b[1]==216&&b[2]==255)return "JPEG";
 if(at(0,"RIFF")&&at(8,"WEBP"))return "WebP";
 if(at(0,"RIFF")&&at(8,"WAVE"))return "WAV";
 if(at(0,std::string_view("II\x2a\0",4))||at(0,std::string_view("MM\0\x2a",4))||at(0,std::string_view("II\x2b\0",4))||at(0,std::string_view("MM\0\x2b",4)))return "TIFF";
 if(at(0,"GIF87a")||at(0,"GIF89a"))return "GIF";
 if(at(0,"BM"))return "BMP";
 if(at(0,"%PDF-"))return "PDF";
 if(at(0,"fLaC"))return "FLAC";
 if(at(0,"ID3"))return "ID3 (audio type unconfirmed)";
 if(b.size()>3&&b[0]==255&&(b[1]&0xe0)==0xe0&&(b[1]&6)!=0&&(b[2]&0xf0)!=0xf0)return "MPEG audio candidate";
 if(at(0,std::string_view("PK\x03\x04",4))||at(0,std::string_view("PK\x05\x06",4))||at(0,std::string_view("PK\x07\x08",4)))return "ZIP container";
 if(at(0,std::string_view("\x1a\x45\xdf\xa3",4)))return "EBML container";
 if(at(4,"ftyp")&&b.size()>=16) {std::string brand(reinterpret_cast<char*>(b.data()+8),4);if(brand=="heic"||brand=="heix"||brand=="mif1"||brand=="hevc")return "HEIF container";if(brand=="qt  ")return "QuickTime container";return "ISO BMFF container";}
 if(at(4,"moov")||at(4,"mdat")||at(4,"wide"))return "QuickTime container candidate";
 if(at(0,"MZ")&&b.size()>=64){uint32_t p=uint32_t(b[60])|uint32_t(b[61])<<8|uint32_t(b[62])<<16|uint32_t(b[63])<<24;if(p<=f.size&&f.size-p>=4){auto pe=f.read(p,4);if(pe==std::vector<unsigned char>{'P','E',0,0})return "PE";}return "DOS executable";}
 return "Unknown";
}
void png(Result& r,Reader& f) {
 uint64_t off=8,retained=0;bool ihdr=false,idat=false,end=false,afterIdat=false;std::map<std::string,int> seen;
 while(off<f.size) {
  require(f.size-off>=12,"Truncated PNG chunk header/trailer");auto header=f.read(off,8);uint32_t n=be32(header.data());require(n<=0x7fffffff,"Invalid PNG chunk length");require(uint64_t(n)+12<=f.size-off,"PNG chunk length exceeds file");
  std::string type(reinterpret_cast<char*>(header.data()+4),4);bool validType=true;for(auto c:type)if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')))validType=false;
  require(validType,"Invalid PNG chunk type bytes");
  auto definition=pngChunk(type);std::string note=definition?std::string(definition->description):"Unknown chunk; raw bytes available";
  if(!definition&&(type[0]&32)==0)r.errors.push_back("Unknown critical PNG chunk: "+type);
  if(type[2]&32)r.errors.push_back("PNG reserved chunk-name bit is invalid: "+type);
  if(off==8)require(type=="IHDR"&&n==13,"PNG must begin with a 13-byte IHDR");
  if(++seen[type]>1 && (type=="IHDR"||type=="IEND"||type=="PLTE"))r.errors.push_back("Duplicate PNG singleton chunk: "+type);
  if(type=="IDAT"){if(afterIdat)r.errors.push_back("Non-consecutive PNG IDAT chunks");idat=true;}else if(idat)afterIdat=true;
  uint32_t crc=crcStep(0xffffffff,header.data()+4,4);std::array<unsigned char,65536>b;
  for(uint64_t p=0;p<n;){size_t k=static_cast<size_t>(std::min<uint64_t>(b.size(),n-p));f.read(off+8+p,b.data(),k);crc=crcStep(crc,b.data(),k);p+=k;}
  auto stored=f.read(off+8+n,4);bool good=(crc^0xffffffff)==be32(stored.data());
  note+=(note.empty()?"":"; ")+std::string(good?"CRC valid":"CRC MISMATCH");if(!good)r.errors.push_back("PNG CRC mismatch at "+std::to_string(off)+" ("+type+")");
  block(r,f,type,off,uint64_t(n)+12,note);
  if(type=="IHDR") {require(n==13,"Invalid IHDR size");auto d=f.read(off+8,13);ihdr=true;uint32_t w=be32(d.data()),h=be32(d.data()+4);require(w&&h&&w<=0x7fffffff&&h<=0x7fffffff,"Invalid PNG dimensions");field(r,"Image","PNG width",w,"PNG",nullptr,off+8,4);field(r,"Image","PNG height",h,"PNG",nullptr,off+12,4);const std::map<unsigned,std::set<unsigned>> depths{{0,{1,2,4,8,16}},{2,{8,16}},{3,{1,2,4,8}},{4,{8,16}},{6,{8,16}}};auto it=depths.find(d[9]);if(it==depths.end()||!it->second.contains(d[8])||d[10]!=0||d[11]!=0||d[12]>1)r.errors.push_back("Invalid PNG IHDR encoding fields");}
  if(type=="tEXt"||type=="iTXt") {
   if(n>MiB||retained+n>4*MiB)r.errors.push_back("PNG text retention limit exceeded; use raw block extraction");
   else {auto d=f.read(off+8,n);auto zero=std::find(d.begin(),d.end(),0);size_t key=static_cast<size_t>(zero-d.begin());
    if(key==0||key>79||zero==d.end())r.errors.push_back("Malformed PNG text keyword");
    else {size_t start=key+1;bool plain=true;
     if(type=="iTXt") {if(n-start<2){plain=false;r.errors.push_back("Truncated iTXt header");}else{unsigned compression=d[start],method=d[start+1];start+=2;for(int z=0;z<2;++z){while(start<n&&d[start])++start;if(start==n){plain=false;r.errors.push_back("Truncated iTXt language/translation");break;}++start;}if(compression>1||method!=0){plain=false;r.errors.push_back("Invalid iTXt compression flags");}if(compression==1)plain=false;}}
     if(plain&&start<=n) {std::string value=type=="tEXt"?latin(d.data()+start,n-start):std::string(reinterpret_cast<char*>(d.data()+start),n-start);field(r,"PNG text",latin(d.data(),key),value,"PNG",Json{{"encoding",type=="tEXt"?"Latin-1":"UTF-8"},{"hex",hex(d.data()+start,n-start)}},off+8+start,n-start);retained+=n;}
    }
   }
  }
  off+=uint64_t(n)+12;
  if(type=="IEND"){require(n==0,"IEND must have zero data bytes");end=true;break;}
 }
 require(ihdr&&idat&&end,"PNG missing IHDR, IDAT or IEND");
 if(off<f.size){block(r,f,"Trailing data",off,f.size-off,"Bytes after IEND; purpose unknown");r.errors.push_back("Extra bytes after PNG IEND");}
 field(r,"Limitations","PNG validation","Chunk framing, CRC, core ordering and IHDR checks only; image data and APNG semantics are not decoded.");
}
void jpeg(Result& r,Reader& f) {
 block(r,f,"SOI",0,2);uint64_t off=2;bool ended=false,sawScan=false;
 while(off<f.size) {
  require(f.byte(off)==255,"Expected JPEG marker");uint64_t start=off++;while(off<f.size&&f.byte(off)==255)++off;require(off<f.size,"Truncated JPEG marker");unsigned marker=f.byte(off++);
  if(marker==0xd9){block(r,f,"EOI",start,off-start);ended=true;break;}
  require(marker!=0&&marker!=0xd8,"Invalid JPEG marker sequence");
  if(marker==1||(marker>=0xd0&&marker<=0xd7)){block(r,f,"Standalone marker",start,off-start);continue;}
  require(f.size-off>=2,"Missing JPEG segment length");auto len=f.read(off,2);uint64_t n=uint64_t(len[0])*256+len[1];require(n>=2&&n<=f.size-off,"Invalid JPEG segment length");
  std::string type=marker>=0xe0&&marker<=0xef?"APP"+std::to_string(marker-0xe0):marker==0xfe?"COM":marker==0xda?"SOS":"Marker 0x"+hex(reinterpret_cast<unsigned char*>(&marker),1);
  block(r,f,type,start,off+n-start,(marker>=0xe0&&marker<=0xef)?"Application metadata/resource segment; extract complete segment bytes":"");
  if(marker==0xfe){auto d=f.read(off+2,static_cast<size_t>(n-2));field(r,"JPEG comment","Comment",latin(d.data(),d.size()),"JPEG",Json{{"hex",hex(d.data(),d.size())}},off+2,n-2);}
  off+=n;
  if(marker==0xda) {sawScan=true;uint64_t begin=off;while(off<f.size){if(f.byte(off)!=255){++off;continue;}uint64_t m=off++;while(off<f.size&&f.byte(off)==255)++off;require(off<f.size,"JPEG entropy stream has truncated marker");unsigned c=f.byte(off);if(c==0||(c>=0xd0&&c<=0xd7)){++off;continue;}off=m;break;}if(off>begin)block(r,f,"Entropy data",begin,off-begin,"Encoded scan data; not decoded");}
 }
 require(ended,"JPEG EOI marker missing");if(!sawScan)r.errors.push_back("JPEG contains no image scan");
 if(off<f.size){block(r,f,"Trailing data",off,f.size-off,"Bytes after first JPEG EOI; may be a valid multi-image extension");r.errors.push_back("Bytes after first JPEG EOI; interpretation requires container context");}
 field(r,"Limitations","JPEG validation","Segment bounds and entropy marker framing only; compressed image validity is not verified.");
}
bool extensionMatches(const Result& r) {
 const std::map<std::string,std::set<std::string>> exts{{"PNG",{".png"}},{"JPEG",{".jpg",".jpeg",".jpe",".jfif"}},{"TIFF",{".tif",".tiff",".dng",".nef",".cr2"}},{"WebP",{".webp"}},{"GIF",{".gif"}},{"BMP",{".bmp",".dib"}},{"PDF",{".pdf"}},{"WAV",{".wav",".wave"}},{"FLAC",{".flac"}},{"ZIP container",{".zip",".docx",".xlsx",".pptx",".odt",".jar",".epub"}},{"PE",{".exe",".dll",".sys",".scr",".ocx"}},{"HEIF container",{".heic",".heif",".hif"}},{"ISO BMFF container",{".mp4",".m4v",".m4a",".mov",".avif",".heic",".heif",".3gp"}},{"QuickTime container",{".mov",".qt"}},{"EBML container",{".mkv",".mka",".webm"}},{"ID3 (audio type unconfirmed)",{".mp3",".aiff",".aif"}},{"MPEG audio candidate",{".mp3",".mp2",".mp1"}}};
 auto i=exts.find(r.format);return i==exts.end()||i->second.contains(lower(utf8(r.path.extension().wstring())));
}
}
std::string sha256File(const fs::path& path,std::atomic_bool& cancel){Reader file(path,cancel);return file.hash(0,file.size);}
Json Result::json() const {
 Json j={{"path",utf8(path.wstring())},{"format_signature",format},{"status",status},{"size",size},{"fields",Json::array()},{"blocks",Json::array()},{"structure_summary",structureGroups(*this)},{"errors",errors},{"generation",generation},{"photo",photo},{"provenance",provenance},{"c2patool_report",c2paReport},{"exiftool_interpreted",parser},{"exiftool_numeric",parserNumeric}};
 for(auto& f:fields)j["fields"].push_back({{"category",f.category},{"name",f.name},{"value",f.value},{"raw",f.raw},{"source",f.source},{"offset",f.offset?Json(*f.offset):Json(nullptr)},{"size",f.size?Json(*f.size):Json(nullptr)},{"warning",f.warning}});
 for(auto& b:blocks)j["blocks"].push_back({{"type",b.type},{"offset",b.offset},{"size",b.size},{"sha256",b.sha256},{"note",b.note}});return j;
}
Result analyze(const fs::path& path,const Options& options,std::atomic_bool& cancel,const std::function<void(unsigned)>& progress) {
 Result r;r.path=fs::absolute(path).lexically_normal();
 try {
  Reader f(r.path,cancel);r.size=f.size;BY_HANDLE_FILE_INFORMATION info{};require(GetFileInformationByHandle(f.file,&info)!=0,"Filesystem information unavailable");
  field(r,"Filesystem","Size (bytes)",r.size);field(r,"Filesystem","Attributes (bitmask)",info.dwFileAttributes);
  std::string attrs;for(auto [flag,name]:std::vector<std::pair<DWORD,std::string>>{{FILE_ATTRIBUTE_READONLY,"Read-only"},{FILE_ATTRIBUTE_HIDDEN,"Hidden"},{FILE_ATTRIBUTE_SYSTEM,"System"},{FILE_ATTRIBUTE_ARCHIVE,"Archive"},{FILE_ATTRIBUTE_COMPRESSED,"Compressed"},{FILE_ATTRIBUTE_ENCRYPTED,"Encrypted"},{FILE_ATTRIBUTE_SPARSE_FILE,"Sparse"},{FILE_ATTRIBUTE_OFFLINE,"Offline"}})if(info.dwFileAttributes&flag)attrs+=name+"; ";field(r,"Filesystem","Attributes",attrs.empty()?"Normal":attrs);
  field(r,"Filesystem","Filesystem creation time (UTC)",timeUtc(info.ftCreationTime));field(r,"Filesystem","Filesystem modification time (UTC)",timeUtc(info.ftLastWriteTime));field(r,"Filesystem","Filesystem access time (UTC)",timeUtc(info.ftLastAccessTime),"Win32",nullptr,{}, {},"May be delayed or disabled by filesystem policy; reading may affect access time.");
  r.format=detect(f);field(r,"File","Signature identification",r.format);if(!extensionMatches(r))r.errors.push_back("Extension/signature mismatch: "+utf8(r.path.extension().wstring())+" versus "+r.format);
  Hash sha256(BCRYPT_SHA256_ALGORITHM),sha1(BCRYPT_SHA1_ALGORITHM);std::vector<unsigned char>b(MiB);
  for(uint64_t p=0;p<f.size;){size_t n=static_cast<size_t>(std::min<uint64_t>(b.size(),f.size-p));f.read(p,b.data(),n);sha256.add(b.data(),n);sha1.add(b.data(),n);p+=n;if(progress)progress(static_cast<unsigned>(static_cast<long double>(p)*70/std::max<uint64_t>(1,f.size)));}
  field(r,"Hashes","SHA-256",sha256.finish(),"Windows CNG");field(r,"Hashes","SHA-1",sha1.finish(),"Windows CNG",nullptr,{}, {},"Legacy comparison hash; use SHA-256 for integrity.");
  f.start=GetTickCount64();f.deadline=30000;
  try{if(r.format=="PNG")png(r,f);else if(r.format=="JPEG")jpeg(r,f);else field(r,"Limitations","Native structure inspection","Unsupported for this format; signature identification and hashes remain available.");}catch(const std::exception& e){r.errors.push_back(e.what());}
  if(cancel)throw std::runtime_error("Cancelled");
  field(r,"Summary","Native structure inspected",std::to_string(r.blocks.size())+" blocks; individual offsets, hashes and validation findings retained. Consecutive IDAT chunks can be grouped for display.");
  if(options.useParser) {
   if(!fs::is_regular_file(options.exiftool))r.errors.push_back("ExifTool executable missing: metadata extraction unsupported in this installation");
   else {
    for(bool numeric:{false,true}) {try {auto proc=runParser(options.exiftool,r.path,numeric,cancel,options.parserTimeoutMs);if(!proc.err.empty())r.errors.push_back(std::string(numeric?"ExifTool numeric stderr: ":"ExifTool stderr: ")+proc.err);if(proc.exitCode)r.errors.push_back("ExifTool exit code "+std::to_string(proc.exitCode));
     // Refuse excessive JSON nesting before parsing attacker-controlled workflow metadata.
     auto parsed=boundedJson(proc.out);
     require(parsed.is_array()&&parsed.size()==1&&parsed[0].is_object(),"Unexpected ExifTool JSON response");if(numeric)r.parserNumeric=std::move(parsed[0]);else r.parser=std::move(parsed[0]);
    }catch(const std::exception& e){r.errors.push_back(e.what());if(cancel)throw;}}
    if(r.parser.is_object())for(auto it=r.parser.begin();it!=r.parser.end();++it){if(r.fields.size()>=16000){r.errors.push_back("Metadata field display limit reached; full parser output retained in JSON");break;}if(it.key()=="SourceFile")continue;auto name=it.key();auto l=lower(name);std::string cat=metadataCategory(name);std::string warning=cat=="Analysis diagnostics"?"Scanner output; not an embedded image metadata field.":cat=="Derived values"?"Computed by ExifTool; not a source metadata field.":"";
     if(l.find("warning")!=std::string::npos||l.find("error")!=std::string::npos){warning=display(it.value());r.errors.push_back("ExifTool "+name+": "+warning);}field(r,cat,name,it.value(),"ExifTool",r.parserNumeric.contains(name)?r.parserNumeric[name]:Json(nullptr),{}, {},warning);
    }
   }
  }else field(r,"Limitations","External parser","Disabled for this analysis; only native metadata is included.");
  if(progress)progress(95);
  analyzeProvenance(r,options,cancel);
  parseGeneration(r);
  summarizePhoto(r);
  std::map<std::string,std::vector<std::string>> texts;for(auto& t:r.fields)if(t.category=="PNG text")texts[t.name].push_back(display(t.value));for(auto& [key,values]:texts)if(values.size()>1){std::set<std::string> distinct(values.begin(),values.end());r.errors.push_back("Duplicate PNG text keyword "+key+" ("+std::to_string(values.size())+" entries)"+(distinct.size()>1?"; values conflict":""));}
  field(r,"Limitations","Attribution limits","Embedded authors, software names and public identifiers are claims; they do not establish identity. C2PA validation does not establish the truth of recorded actions. PDF signature validation and Authenticode verification are not implemented. Filesystem timestamps are not generation timestamps; copying or downloading can make local creation later than modification.");
  field(r,"Limitations","Extraction scope","All tags returned by the configured ExifTool invocation are retained. Binary tag payloads, recursive embedded documents/media and unavailable proprietary tags are not exhaustively extracted. Numeric output is not original source bytes; offsets are null unless known.");
  BY_HANDLE_FILE_INFORMATION after{};require(GetFileInformationByHandle(f.file,&after)!=0,"Final file-state check failed");if(CompareFileTime(&info.ftLastWriteTime,&after.ftLastWriteTime)||after.nFileSizeHigh!=info.nFileSizeHigh||after.nFileSizeLow!=info.nFileSizeLow)r.errors.push_back("File changed during analysis; results may be inconsistent");
 }catch(const std::exception& e){r.errors.push_back(e.what());r.status=cancel?"cancelled":"error";}
 if(r.status=="complete"&&!r.errors.empty())r.status="complete with findings/limitations";
 if(progress)progress(100);return r;
}
void parseGeneration(Result& r) {
 std::set<std::pair<std::string,std::string>> seen;
 auto add=[&](const std::string& source,const std::string& name,Json value){r.generation.push_back({{"source_field",source},{"name",name},{"value",value},{"interpretation","Unverified embedded generation metadata"}});};
 // Recognize only two verified conventions. Preserve all other data as ordinary metadata.
 for(auto& f:r.fields) {
  if(!f.value.is_string())continue;auto text=f.value.get<std::string>();if(text.size()>MiB)continue;
  auto n=lower(f.name);auto pos=n.find_last_of(':');if(pos!=std::string::npos)n=n.substr(pos+1);
  if(!seen.insert({n,text}).second)continue;
  if(n=="prompt"||n=="workflow") {
   try{size_t depth=0;auto j=Json::parse(text,[&](int,Json::parse_event_t e,Json&){if(e==Json::parse_event_t::array_start||e==Json::parse_event_t::object_start)require(++depth<=64,"Workflow nesting limit");if(e==Json::parse_event_t::array_end||e==Json::parse_event_t::object_end)--depth;return true;});bool recognized=n=="workflow"?j.is_object()&&j.contains("nodes")&&j["nodes"].is_array():false;
    if(n=="prompt"&&j.is_object())for(auto& node:j)if(node.is_object()&&node.contains("class_type")&&node.contains("inputs"))recognized=true;
    if(recognized){add(f.name,n=="workflow"?"ComfyUI workflow JSON":"ComfyUI API prompt JSON",j);if(n=="prompt")for(auto it=j.begin();it!=j.end();++it)if(it->is_object()&&it->contains("inputs"))add(f.name,"Node "+it.key()+" inputs ("+it->value("class_type",std::string("unknown"))+")",(*it)["inputs"]);}
   }catch(const std::exception& e){r.errors.push_back("Generation JSON in "+f.name+": "+e.what());}
  }
  if(n=="parameters"||n=="usercomment") {
   size_t settings=text.rfind("\nSteps: ");size_t start=0;
   if(settings==std::string::npos){if(text.starts_with("Steps: ")){settings=0;start=0;}else continue;}else start=settings+1;
   auto last=text.substr(start);if(last.find("Sampler:")==std::string::npos||last.find("Seed:")==std::string::npos)continue;
   add(f.name,"AUTOMATIC1111-compatible parameters (generator not proven)",text);
   auto prompt=text.substr(0,settings);auto neg=prompt.find("\nNegative prompt: ");add(f.name,"Prompt",prompt.substr(0,neg));if(neg!=std::string::npos)add(f.name,"Negative prompt",prompt.substr(neg+18));
   // Split commas outside quoted strings and brackets; do not interpret unknown keys.
   size_t begin=0;bool quoted=false,escaped=false;int brackets=0;
   for(size_t i=0;i<=last.size();++i){char c=i<last.size()?last[i]:',';if(c=='"'&&!escaped)quoted=!quoted;if(!quoted){if(c=='['||c=='{'||c=='(')++brackets;if(c==']'||c=='}'||c==')')--brackets;}if(c==','&&!quoted&&brackets==0){auto part=last.substr(begin,i-begin);auto colon=part.find(": ");if(colon!=std::string::npos){auto key=part.substr(0,colon);key.erase(0,key.find_first_not_of(" \r\n"));add(f.name,key,part.substr(colon+2));}begin=i+1;}escaped=c=='\\'&&!escaped;}
  }
 }
 field(r,"Generation parameters","Model parameters result",r.generation.empty()?"No model, prompt or seed parameters recovered by the implemented parameter recognizers. AI provenance is reported separately; absent parameters do not determine whether an image was AI-generated.":"Recovered embedded generation parameters; claims can be edited or forged.");
 field(r,"Limitations","Generation recognizers","ComfyUI workflow/API prompt JSON and AUTOMATIC1111-compatible infotext only. Forge may use the same convention; generator identity is not inferred. InvokeAI, Fooocus and NovelAI-specific decoding is not implemented.");
}
Json compare(const Result& a,const Result& b,std::atomic_bool* cancel) {
 if(cancel&&*cancel)throw std::runtime_error("Comparison cancelled");
 Json changes=Json::array();auto left=a.json(),right=b.json();
 auto diff=[&](const std::string& section,const Json& x,const Json& y){if(cancel&&*cancel)throw std::runtime_error("Comparison cancelled");if(x!=y)changes.push_back({{"section",section},{"change",x.is_null()?"added":y.is_null()?"removed":"modified"},{"left",x},{"right",y}});};
 std::map<std::string,std::vector<Json>> x,y;for(auto& f:left["fields"])if(f["category"]!="Analysis diagnostics")x[f["category"].get<std::string>()+":"+f["name"].get<std::string>()].push_back(f);for(auto& f:right["fields"])if(f["category"]!="Analysis diagnostics")y[f["category"].get<std::string>()+":"+f["name"].get<std::string>()].push_back(f);
 std::set<std::string> keys;for(auto& [k,v]:x)keys.insert(k);for(auto& [k,v]:y)keys.insert(k);for(auto& k:keys)diff(k,x.contains(k)?Json(x[k]):Json(nullptr),y.contains(k)?Json(y[k]):Json(nullptr));
 diff("Signature",a.format,b.format);diff("Structure and raw block hashes",left["blocks"],right["blocks"]);diff("Generation and workflows",a.generation,b.generation);diff("Analysis errors",a.errors,b.errors);
 diff("C2PA provenance",a.provenance,b.provenance);
 return {{"left",utf8(a.path.wstring())},{"right",utf8(b.path.wstring())},{"changes",changes},{"limitation","Observed differences only; no editing history or identity conclusion. Raw block comparison covers native PNG/JPEG blocks only."}};
}
std::vector<fs::path> enumerate(const fs::path& path,bool recursive,std::atomic_bool& stop,std::vector<std::string>& errors) {
 std::vector<fs::path> result;std::vector<std::pair<fs::path,int>> pending{{fs::absolute(path),0}};
 while(!pending.empty()&&!stop){auto [dir,depth]=pending.back();pending.pop_back();DWORD attrs=GetFileAttributesW(dir.c_str());if(attrs==INVALID_FILE_ATTRIBUTES){errors.push_back("Cannot access directory: "+utf8(dir.wstring()));continue;}if(attrs&FILE_ATTRIBUTE_REPARSE_POINT){errors.push_back("Skipped reparse directory: "+utf8(dir.wstring()));continue;}
  std::error_code ec;fs::directory_iterator it(dir,fs::directory_options::none,ec),end;if(ec){errors.push_back("Directory enumeration: "+utf8(dir.wstring())+": "+ec.message());continue;}
  while(it!=end&&!stop){auto p=it->path();DWORD a=GetFileAttributesW(p.c_str());if(a==INVALID_FILE_ATTRIBUTES)errors.push_back("Cannot stat: "+utf8(p.wstring()));else if(a&FILE_ATTRIBUTE_REPARSE_POINT)errors.push_back("Skipped reparse point: "+utf8(p.wstring()));else if(a&FILE_ATTRIBUTE_DIRECTORY){if(recursive){if(depth<32)pending.push_back({p,depth+1});else errors.push_back("Directory depth limit: "+utf8(p.wstring()));}}else{if(result.size()>=256){errors.push_back("Scan limited to 256 files; remaining paths not analyzed");return result;}result.push_back(p);}it.increment(ec);if(ec){errors.push_back("Directory iteration failed: "+ec.message());break;}if(pending.size()>4096){errors.push_back("Directory count limit reached");pending.clear();break;}}
  if(errors.size()>4096){errors.push_back("Enumeration error limit reached");break;}
 }if(stop)errors.push_back("Cancelled during directory enumeration");std::sort(result.begin(),result.end());return result;
}
namespace {
Handle outputFile(const fs::path& destination,const std::vector<fs::path>& originals){
 auto dest=fs::absolute(destination).lexically_normal();for(auto& p:originals){std::error_code ec;require(!fs::equivalent(p,dest,ec),"Export destination is an inspected original");require(_wcsicmp(fs::absolute(p).lexically_normal().c_str(),dest.c_str())!=0,"Export destination is an inspected original");}
 // CREATE_NEW deliberately prevents clobbering aliases, hard links and existing evidence.
 HANDLE h=CreateFileW(dest.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);require(h!=INVALID_HANDLE_VALUE,"Destination must be a new file (Win32 "+std::to_string(GetLastError())+")");return Handle(h);
}
void write(HANDLE h,std::string_view s,std::atomic_bool* cancel=nullptr){while(!s.empty()){require(!cancel||!*cancel,"Export cancelled; destination may be incomplete");DWORD n=static_cast<DWORD>(std::min<size_t>(s.size(),MiB)),done=0;require(WriteFile(h,s.data(),n,&done,nullptr)&&done==n,"Export write failed; destination may be incomplete");s.remove_prefix(done);}}
std::string csv(std::string s){std::string out="\""; // Prevent spreadsheet formulas; JSON retains exact original values.
 if(!s.empty()&&(s[0]=='='||s[0]=='+'||s[0]=='-'||s[0]=='@'||s[0]=='\t'||s[0]=='\r'))out+="'";
 for(char c:s){if(c=='"')out+='"';out+=c;}return out+'"';}
}
void exportReport(const fs::path& dest,const Json& report,const std::string& format,const std::vector<fs::path>& originals,std::atomic_bool* cancel){
 require(!cancel||!*cancel,"Export cancelled");
 auto h=outputFile(dest,originals);
 auto emit=[&](std::string_view s){write(h,s,cancel);};
 if(format=="json")emit(report.dump(2,' ',false,Json::error_handler_t::replace));
 else if(format=="txt")emit(report.dump(2,' ',false,Json::error_handler_t::replace)+"\r\n");
 else if(format=="csv") {
  emit("path,category,field,value,source,raw,offset,size,warning\r\n");
  auto row=[&](const std::vector<std::string>& cells){std::string line;for(auto& c:cells){if(!line.empty())line+=',';line+=csv(c);}emit(line+"\r\n");};
  for(auto& r:report.at("files")){auto p=display(r["path"]);row({p,"Analysis","Status",display(r["status"]),"Native","","","",""});for(auto& f:r["fields"])row({p,display(f["category"]),display(f["name"]),display(f["value"]),display(f["source"]),display(f["raw"]),display(f["offset"]),display(f["size"]),display(f["warning"])});for(auto& e:r["errors"])row({p,"Error","Analysis error",display(e),"Analysis","","","",display(e)});for(auto& b:r["blocks"])row({p,"Structure",display(b["type"]),display(b),"Native","",display(b["offset"]),display(b["size"]),display(b["note"])});for(auto& g:r["generation"])row({p,"Generation",display(g["name"]),display(g["value"]),display(g["source_field"]),"","","","Unverified embedded metadata"});}
  for(auto& r:report.at("files")){auto p=display(r["path"]);if(r.contains("provenance"))row({p,"Provenance","C2PA history and validation",display(r["provenance"]),"Analysis","","","","Claims and independent validation are separate"});if(r.contains("c2patool_report"))row({p,"C2PA validation","Full validator report",display(r["c2patool_report"]),"c2patool","","","",""});}
  for(auto& e:report.value("batch_errors",Json::array()))row({"","Batch error","Error",display(e),"Scan","","","",display(e)});
  if(report.contains("comparison"))row({"","Comparison","Observed changes",display(report["comparison"]),"Analysis","","","",""});
  row({"","Export","CSV safety","Formula-leading cells are prefixed with an apostrophe. JSON preserves exact values.","Export","","","",""});
 }else throw std::runtime_error("Unknown export format");require(FlushFileBuffers(h)!=0,"Cannot flush export");
}
std::string hexPage(const fs::path& path,uint64_t offset,size_t count){std::atomic_bool cancel=false;Reader f(path,cancel);require(offset<=f.size,"Hex offset is beyond EOF");auto b=f.read(offset,static_cast<size_t>(std::min<uint64_t>(std::min<size_t>(count,4096),f.size-offset)));std::ostringstream out;for(size_t i=0;i<b.size();i+=16){out<<std::hex<<std::setw(16)<<std::setfill('0')<<offset+i<<"  ";for(size_t j=0;j<16;++j)if(i+j<b.size())out<<std::setw(2)<<unsigned(b[i+j])<<' ';else out<<"   ";out<<" |";for(size_t j=0;j<16&&i+j<b.size();++j)out<<char(b[i+j]>=32&&b[i+j]<127?b[i+j]:'.');out<<"|\r\n";}return out.str();}
void extractBlock(const fs::path& source,const Block& b,const fs::path& dest,std::atomic_bool* cancel){std::atomic_bool stop=false;Reader f(source,cancel?*cancel:stop);require(b.offset<=f.size&&b.size<=f.size-b.offset,"Block no longer fits file");require(f.hash(b.offset,b.size)==b.sha256,"Source block changed since analysis; rescan before extraction");auto out=outputFile(dest,{source});std::array<unsigned char,65536>buf;for(uint64_t p=0;p<b.size;){size_t n=static_cast<size_t>(std::min<uint64_t>(buf.size(),b.size-p));f.read(b.offset+p,buf.data(),n);write(out,std::string_view(reinterpret_cast<char*>(buf.data()),n),cancel);p+=n;}require(FlushFileBuffers(out)!=0,"Cannot flush extracted block");}
}
