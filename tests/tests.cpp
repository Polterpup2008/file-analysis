#include "core.hpp"
#include <fstream>
#include <iostream>
#include <thread>
using namespace fa;
int checks=0;
void check(bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);}
bool contains(const std::vector<std::string>& v,std::string_view s){for(auto& x:v)if(x.find(s)!=std::string::npos)return true;return false;}
std::string hash(const Result& r){for(auto& f:r.fields)if(f.name=="SHA-256")return display(f.value);return {};}
int wmain(int argc,wchar_t** argv){try{
 check(argc==3,"Expected fixture and ExifTool paths");fs::path root=argv[1];Options opt;opt.exiftool=argv[2];std::atomic_bool cancel=false;
 auto before=fs::file_size(root/L"metadata.png");auto r=analyze(root/L"metadata.png",opt,cancel);
 check(r.format=="PNG","PNG signature");check(r.size==before,"File size");check(r.parser.is_object(),"Real ExifTool process returned metadata");
 check(r.parserNumeric.is_object(),"ExifTool numeric values retained");check(!contains(r.errors,"Cannot start"),"Parser launched");check(!contains(r.errors,"stderr"),"Parser clean environment");
 check(contains(r.errors,"Duplicate PNG text"),"Duplicate PNG tags detected");check(contains(r.errors,"values conflict"),"Conflicting duplicate values identified");
 check(r.generation.size()>=10,"Generation parameters and workflow decoded");check(r.blocks.back().type=="Trailing data","PNG trailing data");
 int authors=0,parserAuthors=0;for(auto& f:r.fields){if(f.name=="Author")++authors;if(f.source=="ExifTool"&&f.name.find(":Author")!=std::string::npos)++parserAuthors;}
 check(authors==2,"Duplicate native text fields preserved");check(parserAuthors==2,"Duplicate ExifTool fields preserved with instance grouping");
 auto j=r.json();check(j["fields"][0]["offset"].is_null(),"Unknown byte locations are null");
 auto clean=analyze(root/L"clean.png",opt,cancel);check(clean.errors.empty(),"Clean PNG has no errors");check(clean.generation.empty(),"No fabricated generation metadata");
 auto cmp=compare(clean,r);check(!cmp["changes"].empty(),"Comparison finds differences");auto self=compare(r,r);check(self["changes"].empty(),"Identical comparison");
 auto wrong=analyze(root/L"wrong.jpg",opt,cancel);check(wrong.format=="PNG"&&contains(wrong.errors,"mismatch"),"Extension mismatch");
 auto compressed=analyze(root/L"compressed.png",opt,cancel);bool ztxt=false;for(auto& f:compressed.fields)if(display(f.value).find("Compressed metadata recovered")!=std::string::npos)ztxt=true;check(ztxt,"Compressed PNG metadata extracted by established parser");
 auto uni=analyze(root/L"unicode-\u753b\u50cf-\u00e9.png",opt,cancel);check(uni.errors.empty()&&uni.parser.is_object(),"Unicode file paths and iTXt");
 auto pdf=analyze(root/L"fixture.pdf",opt,cancel);check(pdf.format=="PDF"&&pdf.parser.dump().find("Fixture author")!=std::string::npos,"PDF actual metadata");
 auto doc=analyze(root/L"fixture.docx",opt,cancel);check(doc.format=="ZIP container"&&doc.parser.dump().find("Fixture author")!=std::string::npos,"DOCX document properties");
 auto zip=analyze(root/L"fixture.zip",opt,cancel);check(zip.format=="ZIP container"&&zip.parser.is_object(),"ZIP parser");
 auto wav=analyze(root/L"fixture.wav",opt,cancel);check(wav.format=="WAV"&&wav.parser.is_object(),"WAV parser");
 Options native=opt;native.useParser=false;
 for(auto name:{L"bad-crc.png",L"truncated.png",L"malformed.jpg",L"no-scan.jpg",L"large-text.png",L"malformed-generation.png"}){auto b=analyze(root/name,native,cancel);check(!b.errors.empty(),"Malformed or oversized input reported");}
 auto zero=analyze(root/L"zero.bin",native,cancel);check(zero.size==0&&hash(zero)=="e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855","Zero-byte hash");
 auto unknown=analyze(root/L"unknown.bin",native,cancel);check(unknown.format=="Unknown"&&hash(unknown)=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad","Known SHA-256 vector abc");
 {Handle lock(CreateFileW((root/L"clean.png").c_str(),GENERIC_READ,0,nullptr,OPEN_EXISTING,0,nullptr));auto locked=analyze(root/L"clean.png",native,cancel);check(locked.status=="error"&&contains(locked.errors,"Cannot open"),"Inaccessible/sharing-denied path reported");}
 auto missing=analyze(root/L"missing.png",native,cancel);check(missing.status=="error","Missing file error");
 auto large=analyze(root/L"large-sparse.bin",native,cancel,[&](unsigned){cancel=true;});check(large.size>UINT32_MAX&&large.status=="cancelled","Large file streamed and cancelled without narrowing");cancel=false;
 Options timeout=opt;timeout.parserTimeoutMs=1;auto timed=analyze(root/L"clean.png",timeout,cancel);check(contains(timed.errors,"timeout"),"Parser timeout enforced");
 Options absent=opt;absent.exiftool=root/L"absent.exe";auto noParser=analyze(root/L"clean.png",absent,cancel);check(contains(noParser.errors,"missing"),"Missing parser explicitly reported");
 SetEnvironmentVariableW(L"PERL5OPT",L"-e die('must never execute')");auto safe=analyze(root/L"clean.png",opt,cancel);SetEnvironmentVariableW(L"PERL5OPT",nullptr);check(safe.parser.is_object()&&!contains(safe.errors,"stderr"),"Perl startup environment hooks removed");
 for(int i=0;i<48;++i){auto fuzz=analyze(root/(L"fuzz-"+std::to_wstring(i)+L".bin"),native,cancel);check(!fuzz.fields.empty(),"Malformed input bounded without crash");}
 std::vector<std::string> errs;auto paths=enumerate(root,false,cancel,errs);check(paths.size()>50&&errs.empty(),"Batch enumeration");cancel=true;auto interrupted=enumerate(root,true,cancel,errs);check(interrupted.empty()&&contains(errs,"Cancelled"),"Interrupted enumeration reported");cancel=false;
 auto output=root.parent_path()/L"output";fs::create_directories(output);Json report={{"files",Json::array({r.json(),clean.json(),unknown.json()})},{"batch_errors",Json::array({"Example enumeration failure"})},{"comparison",cmp}};
 for(auto ext:{"json","csv","txt"}){auto dest=output/(std::string("test.")+ext);fs::remove(dest);exportReport(dest,report,ext,{r.path,clean.path});check(fs::file_size(dest)>100,"Real export nonempty");}
 std::ifstream in(output/L"test.json");auto exported=Json::parse(in);check(exported==report,"JSON lossless round trip including errors and provenance");
 bool protectedOriginal=false;try{exportReport(r.path,report,"json",{r.path});}catch(...){protectedOriginal=true;}check(protectedOriginal,"Export cannot overwrite inspected evidence");
 auto alias=output/L"alias.png";fs::remove(alias);fs::create_hard_link(r.path,alias);bool protectedAlias=false;try{exportReport(alias,report,"json",{r.path});}catch(...){protectedAlias=true;}check(protectedAlias,"Hard-linked evidence protected");fs::remove(alias);
 check(hexPage(r.path,0).find("89 50 4e 47")!=std::string::npos,"Real hex viewer bytes");
 auto blockPath=output/L"raw.bin";fs::remove(blockPath);extractBlock(r.path,r.blocks.back(),blockPath);std::ifstream raw(blockPath,std::ios::binary);std::string bytes((std::istreambuf_iterator<char>(raw)),{});check(bytes=="TRAILING-EVIDENCE","Exact raw resource extraction");
 auto after=analyze(r.path,native,cancel);check(hash(after)==hash(r)&&after.size==before,"Original bytes unchanged after analysis and exports");
 auto exe=analyze(argv[0],opt,cancel);check(exe.format=="PE"&&exe.parser.is_object(),"PE binary identification and metadata; never executed as input");
 std::cout<<"PASS: "<<checks<<" assertions\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<" assertions: "<<e.what()<<"\n";return 1;}}
