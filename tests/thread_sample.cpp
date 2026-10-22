// Developer-only, offline stack sampler for the application's own performance QA.
#include <windows.h>
#include <dbghelp.h>
#include <iostream>
#include <iomanip>
#pragma comment(lib,"dbghelp.lib")
int wmain(int argc,wchar_t** argv){
 if(argc!=3)return 1;DWORD pid=wcstoul(argv[1],nullptr,10),tid=wcstoul(argv[2],nullptr,10);
 HANDLE process=OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ,FALSE,pid);
 HANDLE thread=OpenThread(THREAD_GET_CONTEXT|THREAD_SUSPEND_RESUME|THREAD_QUERY_INFORMATION,FALSE,tid);
 if(!process||!thread){std::cerr<<"Open failed "<<GetLastError()<<"\n";return 2;}
 SymSetOptions(SYMOPT_UNDNAME|SYMOPT_DEFERRED_LOADS);
 if(!SymInitializeW(process,L".",TRUE)){std::cerr<<"Symbols failed\n";return 3;}
 if(SuspendThread(thread)==DWORD(-1))return 4;
 CONTEXT ctx{};ctx.ContextFlags=CONTEXT_FULL;bool got=GetThreadContext(thread,&ctx)!=0;
 STACKFRAME64 frame{};frame.AddrPC={ctx.Rip,0,AddrModeFlat};frame.AddrFrame={ctx.Rbp,0,AddrModeFlat};frame.AddrStack={ctx.Rsp,0,AddrModeFlat};
 DWORD64 addresses[40]{};size_t count=0;
 if(got)while(count<40&&StackWalk64(IMAGE_FILE_MACHINE_AMD64,process,thread,&frame,&ctx,nullptr,SymFunctionTableAccess64,SymGetModuleBase64,nullptr)&&frame.AddrPC.Offset)addresses[count++]=frame.AddrPC.Offset;
 ResumeThread(thread);
 for(size_t i=0;i<count;++i){auto address=addresses[i];IMAGEHLP_MODULE64 module{};module.SizeOfStruct=sizeof(module);SymGetModuleInfo64(process,address,&module);char data[sizeof(SYMBOL_INFO)+1024]{};auto symbol=reinterpret_cast<SYMBOL_INFO*>(data);symbol->SizeOfStruct=sizeof(SYMBOL_INFO);symbol->MaxNameLen=1023;DWORD64 displacement=0;bool named=SymFromAddr(process,address,&displacement,symbol)!=0;
  std::cout<<module.ModuleName<<" + 0x"<<std::hex<<address-module.BaseOfImage<<" "<<(named?symbol->Name:"")<<" + 0x"<<displacement<<"\n";
 }
 SymCleanup(process);CloseHandle(thread);CloseHandle(process);return got?0:5;
}
