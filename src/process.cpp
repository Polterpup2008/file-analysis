#include "core.hpp"
#include <algorithm>
#include <chrono>
#include <stdexcept>
namespace fa {
std::string utf8(std::wstring_view s) {
 if(s.empty()) return {};
 int n=WideCharToMultiByte(CP_UTF8,0,s.data(),static_cast<int>(s.size()),nullptr,0,nullptr,nullptr);
 std::string r(n,0); WideCharToMultiByte(CP_UTF8,0,s.data(),static_cast<int>(s.size()),r.data(),n,nullptr,nullptr); return r;
}
std::wstring wide(std::string_view s) {
 if(s.empty()) return {};
 int n=MultiByteToWideChar(CP_UTF8,0,s.data(),static_cast<int>(s.size()),nullptr,0);
 std::wstring r(n,0); MultiByteToWideChar(CP_UTF8,0,s.data(),static_cast<int>(s.size()),r.data(),n); return r;
}
std::string display(const Json& j) { return j.is_string()? j.get<std::string>() : j.dump(-1,' ',false,Json::error_handler_t::replace); }
namespace {
std::wstring quoteArgument(std::wstring_view arg) {
 std::wstring out=L"\"";size_t slashes=0;
 for(auto c:arg){if(c==L'\\'){++slashes;continue;}out.append(slashes*(c==L'"'?2:1),L'\\');slashes=0;if(c==L'"')out+=L'\\';out+=c;}
 out.append(slashes*2,L'\\');return out+L'"';
}
ProcessResult runExternal(const fs::path& executable,const std::vector<std::wstring>& args,const std::string& input,std::atomic_bool& cancel,DWORD timeout,const std::string& label) {
 auto exe=fs::absolute(executable);
 SECURITY_ATTRIBUTES sa{sizeof(sa),nullptr,TRUE};
 HANDLE inR,inW,outR,outW,errR,errW;
 if(!CreatePipe(&inR,&inW,&sa,65536)) throw std::runtime_error("Parser stdin pipe failed");
 Handle ir(inR),iw(inW);
 if(!CreatePipe(&outR,&outW,&sa,65536)) throw std::runtime_error("Parser stdout pipe failed");
 Handle rr(outR),rw(outW);
 if(!CreatePipe(&errR,&errW,&sa,65536)) throw std::runtime_error("Parser stderr pipe failed");
 Handle er(errR),ew(errW);
 SetHandleInformation(inW,HANDLE_FLAG_INHERIT,0); SetHandleInformation(outR,HANDLE_FLAG_INHERIT,0); SetHandleInformation(errR,HANDLE_FLAG_INHERIT,0);
 SIZE_T bytes=0; InitializeProcThreadAttributeList(nullptr,1,0,&bytes);
 std::vector<unsigned char> attrs(bytes);
 auto list=reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attrs.data());
 if(!InitializeProcThreadAttributeList(list,1,0,&bytes)) throw std::runtime_error("Parser attribute list failed");
 struct AttrGuard { LPPROC_THREAD_ATTRIBUTE_LIST p; ~AttrGuard(){DeleteProcThreadAttributeList(p);} } guard{list};
 HANDLE allowed[]{inR,outW,errW};
 if(!UpdateProcThreadAttribute(list,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,allowed,sizeof(allowed),nullptr,nullptr)) throw std::runtime_error("Parser handle allowlist failed");
 STARTUPINFOEXW si{}; si.StartupInfo.cb=sizeof(si); si.StartupInfo.dwFlags=STARTF_USESTDHANDLES;
 si.StartupInfo.hStdInput=inR; si.StartupInfo.hStdOutput=outW; si.StartupInfo.hStdError=errW; si.lpAttributeList=list;
 // Perl environment hooks can execute code before ExifTool even starts. Remove them.
 std::vector<wchar_t> env;
 auto original=GetEnvironmentStringsW();
 if(!original) throw std::runtime_error("Cannot read child environment");
 for(auto p=original;*p;p+=wcslen(p)+1) {
  std::wstring s=p, upper=s; std::transform(upper.begin(),upper.end(),upper.begin(),towupper);
  if(upper.starts_with(L"PERL") || upper.starts_with(L"EXIFTOOL") || upper.starts_with(L"PAR_") || upper.starts_with(L"LC_") || upper.starts_with(L"LANG=") || upper.starts_with(L"C2PA") || upper.starts_with(L"RUST_LOG=") || upper.starts_with(L"RUST_BACKTRACE=")) continue;
  env.insert(env.end(),s.begin(),s.end()); env.push_back(0);
 }
 FreeEnvironmentStringsW(original); for(auto c:std::wstring(L"LC_ALL=C"))env.push_back(c);env.push_back(0);env.push_back(0);
 Handle job(CreateJobObjectW(nullptr,nullptr));
 if(!job.value) throw std::runtime_error("Cannot create parser job");
 JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
 limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE|JOB_OBJECT_LIMIT_PROCESS_MEMORY|JOB_OBJECT_LIMIT_ACTIVE_PROCESS;
 limits.ProcessMemoryLimit=512ull*1024*1024; limits.BasicLimitInformation.ActiveProcessLimit=4;
 if(!SetInformationJobObject(job,JobObjectExtendedLimitInformation,&limits,sizeof(limits))) throw std::runtime_error("Cannot bound parser job");
 std::wstring command=quoteArgument(exe.wstring());for(auto& arg:args)command+=L" "+quoteArgument(arg);
 PROCESS_INFORMATION pi{};
 if(!CreateProcessW(exe.c_str(),command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW|CREATE_SUSPENDED|CREATE_UNICODE_ENVIRONMENT|EXTENDED_STARTUPINFO_PRESENT,env.data(),exe.parent_path().c_str(),&si.StartupInfo,&pi))
  throw std::runtime_error("Cannot start "+label+" (Win32 "+std::to_string(GetLastError())+")");
 Handle process(pi.hProcess),thread(pi.hThread);
 if(!AssignProcessToJobObject(job,process)) { TerminateProcess(process,1); throw std::runtime_error("Cannot contain ExifTool in job"); }
 ResumeThread(thread);
 CloseHandle(ir.value); ir.value=INVALID_HANDLE_VALUE; CloseHandle(rw.value); rw.value=INVALID_HANDLE_VALUE; CloseHandle(ew.value); ew.value=INVALID_HANDLE_VALUE;
 DWORD wrote=0;
 if(!WriteFile(iw,input.data(),static_cast<DWORD>(input.size()),&wrote,nullptr) || wrote!=input.size()) throw std::runtime_error("Cannot feed ExifTool argument file");
 CloseHandle(iw.value); iw.value=INVALID_HANDLE_VALUE;
 ProcessResult result;
 auto start=GetTickCount64();
 auto drain=[](HANDLE pipe,std::string& s) {
  DWORD available=0;
  while(PeekNamedPipe(pipe,nullptr,0,nullptr,&available,nullptr) && available) {
   char b[8192]; DWORD got=0;
   if(!ReadFile(pipe,b,std::min<DWORD>(available,sizeof(b)),&got,nullptr)||!got) break;
   if(s.size()+got>16*1024*1024) throw std::runtime_error("Parser output exceeded 16 MiB; extraction incomplete");
   s.append(b,got);
  }
 };
 for(;;) {
  if(cancel) throw std::runtime_error("Cancelled during "+label);
  if(GetTickCount64()-start>timeout) throw std::runtime_error(label+" timeout; extraction incomplete");
  drain(rr,result.out); drain(er,result.err);
  if(WaitForSingleObject(process,15)==WAIT_OBJECT_0) { drain(rr,result.out); drain(er,result.err); break; }
 }
 GetExitCodeProcess(process,&result.exitCode); return result;
}
}
ProcessResult runParser(const fs::path& exe,const fs::path& file,bool numeric,std::atomic_bool& cancel,DWORD timeout) {
 auto path=utf8(fs::absolute(file).wstring());
 if(path.find_first_of("\r\n")!=std::string::npos || path.size()>50000)throw std::runtime_error("ExifTool path cannot be represented safely in an argument file");
 std::string input="-j\n-G0:1:3:4:5\n-a\n-u\n-U\n-struct\n-api\nRequestAll=3\n-all\n-validate\n-warning\n-error\n";
 if(numeric)input+="-n\n";input+="--\n"+path+"\n";
 return runExternal(exe,{L"-config",L"",L"-charset",L"filename=utf8",L"-@",L"-"},input,cancel,timeout,"ExifTool");
}
ProcessResult runC2pa(const fs::path& exe,const fs::path& file,std::atomic_bool& cancel,DWORD timeout) {
 auto settings=fs::absolute(exe).parent_path()/L"offline.toml";
 // Missing settings must fail closed: c2patool itself silently falls back to online defaults.
 if(!fs::is_regular_file(settings))throw std::runtime_error("C2PA offline settings missing; validation refused");
 if(sha256File(settings,cancel)!="9f6073062bb379e2ed03e07f16a5de0f73636262dc2ae3e7dbe815a87d3580f6")throw std::runtime_error("C2PA offline settings changed; validation refused to preserve offline policy");
 if(sha256File(exe,cancel)!="b970c119c95539896c9a2a05b1bb160dce7da6874bafad08815c2bd6538f01e1")throw std::runtime_error("Bundled c2patool checksum mismatch; validation refused");
 auto trust=fs::absolute(exe).parent_path()/L"c2pa-trust-list.pem";
 if(fs::is_regular_file(trust)&&sha256File(trust,cancel)!="b3949271a07b007e92ef1d18050b55388f5e0d2686a16fd864176c48421af8ae")throw std::runtime_error("Bundled C2PA trust-list checksum mismatch; validation refused");
 return runExternal(exe,{L"--settings",settings.wstring(),L"--",fs::absolute(file).wstring()},"",cancel,timeout,"c2patool");
}
Json boundedJson(std::string_view text,size_t maxDepth,size_t maxNodes) {
 size_t depth=0,nodes=0;
 return Json::parse(text,[&](int,Json::parse_event_t event,Json&){
  if(++nodes>maxNodes)throw std::runtime_error("JSON node budget exceeded");
  if(event==Json::parse_event_t::object_start||event==Json::parse_event_t::array_start)if(++depth>maxDepth)throw std::runtime_error("JSON nesting limit exceeded");
  if(event==Json::parse_event_t::object_end||event==Json::parse_event_t::array_end)--depth;return true;
 });
}
}
