#include "core.hpp"
#include "presentation.hpp"
#include <commctrl.h>
#include <commdlg.h>
#include <shlobj.h>
#include <shellapi.h>
#include <algorithm>
#include <array>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <thread>
#pragma comment(lib,"comctl32.lib")
using namespace fa;
namespace {
constexpr UINT Done=WM_APP+1,Progress=WM_APP+2;
enum {Open=100,Folder,Scan,Cancel,Export,Copy,Compare,Workflow,Extract,Files,Mode,Category,Search,Offset,Go,Prev,Next,Table,Detail,Info,Status,Bar,Recursive,Privacy,GroupIdat,Level,ViewLabel,SectionLabel};
enum {SummaryView,MetadataView,StructureView,HexView,ComparisonView,ProvenanceView};
struct Packet {std::vector<Result> files;std::vector<std::string> errors;Json comparison;std::wstring message;bool analysis=false;};
struct Row {int kind;size_t index;}; // 0 field, 1 block, 2 generation, 3 error, 4 comparison; 9 simple finding
HWND window=nullptr;std::map<int,HWND> controls;HFONT font=nullptr,mono=nullptr;HINSTANCE instance;
std::thread worker;std::atomic_bool cancelled=false;bool busy=false,closing=false,analysisCancelled=false;fs::path folder;std::vector<fs::path> inputPaths;
std::vector<Result> results;std::vector<std::string> batchErrors;Json comparison,groups;std::vector<Row> rows;std::vector<SimpleFinding> findings;uint64_t hexOffset=0;int selectedFile=-1;bool updating=false;
bool simple(){return SendMessageW(controls[Level],CB_GETCURSEL,0,0)==0;}
std::wstring text(HWND h){int n=GetWindowTextLengthW(h);std::wstring s(static_cast<size_t>(n)+1,0);GetWindowTextW(h,s.data(),n+1);s.resize(n);return s;}
std::wstring trim(std::wstring s,size_t limit=4096){if(s.size()>limit){s.resize(limit);s+=L" [display truncated; select row for full value]";}for(auto& c:s)if(c==0)c=0x2400;return s;}
void status(const std::wstring& s){SetWindowTextW(controls[Status],s.c_str());}
void error(const std::exception& e){MessageBoxW(window,wide(e.what()).c_str(),L"File Analysis",MB_OK|MB_ICONERROR);}
HWND make(int id,LPCWSTR cls,LPCWSTR caption,DWORD style,DWORD ex=0){auto h=CreateWindowExW(ex,cls,caption,WS_CHILD|WS_VISIBLE|style,0,0,1,1,window,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),instance,nullptr);controls[id]=h;SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);return h;}
void enable(){for(int id:{Open,Folder,Scan,Export,Compare,Workflow,Extract})EnableWindow(controls[id],!busy);EnableWindow(controls[Cancel],busy);EnableWindow(controls[Scan],!busy&&(!folder.empty()||!inputPaths.empty()));EnableWindow(controls[Export],!busy&&(!results.empty()||!batchErrors.empty()));EnableWindow(controls[Workflow],!busy&&selectedFile>=0&&!results[selectedFile].generation.empty());EnableWindow(controls[Compare],!busy&&results.size()>=2);int idx=ListView_GetNextItem(controls[Table],-1,LVNI_SELECTED);EnableWindow(controls[Copy],idx>=0||GetWindowTextLengthW(controls[Detail])>0);EnableWindow(controls[Extract],!busy&&selectedFile>=0&&idx>=0&&idx<static_cast<int>(rows.size())&&rows[idx].kind==1);}
void layout(){RECT r{};GetClientRect(window,&r);int dpi=static_cast<int>(GetDpiForWindow(window));int w=MulDiv(r.right,96,dpi),h=MulDiv(r.bottom,96,dpi);auto place=[dpi](HWND control,int x,int y,int width,int height,BOOL repaint){MoveWindow(control,MulDiv(x,dpi,96),MulDiv(y,dpi,96),MulDiv(width,dpi,96),MulDiv(height,dpi,96),repaint);};bool concise=simple();int x=8;for(auto [id,width]:std::vector<std::pair<int,int>>{{Open,90},{Folder,90},{Scan,55},{Cancel,60},{Export,110},{Copy,70},{Compare,90},{Workflow,125},{Extract,115}}){bool visible=!concise||(id!=Compare&&id!=Workflow&&id!=Extract);ShowWindow(controls[id],visible?SW_SHOW:SW_HIDE);if(visible){place(controls[id],x,8,width,27,TRUE);x+=width+5;}}
 place(controls[Recursive],8,40,220,23,TRUE);place(controls[Privacy],235,40,250,23,TRUE);place(controls[GroupIdat],495,40,230,23,TRUE);place(controls[Info],235,68,w-243,40,TRUE);
 place(controls[Files],8,84,220,h-124,TRUE);
 place(controls[ViewLabel],235,120,36,23,TRUE);place(controls[Level],275,116,100,180,TRUE);
 ShowWindow(controls[SectionLabel],concise?SW_HIDE:SW_SHOW);ShowWindow(controls[Mode],concise?SW_HIDE:SW_SHOW);
 place(controls[SectionLabel],390,120,50,23,TRUE);place(controls[Mode],444,116,135,200,TRUE);int filterX=concise?385:587;place(controls[Category],filterX,116,170,350,TRUE);place(controls[Search],filterX+178,116,w-filterX-186,25,TRUE);
 bool isHex=!concise&&SendMessageW(controls[Mode],CB_GETCURSEL,0,0)==HexView;for(int id:{Offset,Go,Prev,Next})ShowWindow(controls[id],isHex?SW_SHOW:SW_HIDE);
 ShowWindow(controls[GroupIdat],!concise&&SendMessageW(controls[Mode],CB_GETCURSEL,0,0)==StructureView?SW_SHOW:SW_HIDE);
 place(controls[Offset],235,148,210,24,TRUE);place(controls[Go],452,148,65,24,TRUE);place(controls[Prev],525,148,75,24,TRUE);place(controls[Next],608,148,75,24,TRUE);
 int y=isHex?180:150;int detailH=std::clamp(h/3,140,270);place(controls[Table],235,y,w-243,h-y-detailH-50,TRUE);place(controls[Detail],235,h-detailH-44,w-243,detailH,TRUE);
 if(concise){ListView_SetColumnWidth(controls[Table],0,MulDiv(120,dpi,96));ListView_SetColumnWidth(controls[Table],1,MulDiv(195,dpi,96));ListView_SetColumnWidth(controls[Table],2,MulDiv(std::max(200,w-243-315-23),dpi,96));}
 place(controls[Status],8,h-31,w-220,24,TRUE);place(controls[Bar],w-205,h-29,197,20,TRUE);
}
void configurePresentation(){
 ListView_SetItemCount(controls[Table],0);while(ListView_DeleteColumn(controls[Table],0)){}
 std::vector<std::pair<LPCWSTR,int>> columns=simple()?std::vector<std::pair<LPCWSTR,int>>{{L"Topic",120},{L"Finding",195},{L"Result",600}}:std::vector<std::pair<LPCWSTR,int>>{{L"Category",100},{L"Field / Block",235},{L"Value",300},{L"Source",125},{L"Raw / numeric / SHA-256",200},{L"Offset / size",120},{L"Warning / limitation",300}};
 int col=0;for(auto [title,width]:columns){LVCOLUMNW c{LVCF_TEXT|LVCF_WIDTH};c.pszText=const_cast<LPWSTR>(title);c.cx=MulDiv(width,static_cast<int>(GetDpiForWindow(window)),96);ListView_InsertColumn(controls[Table],col++,&c);}
 // Native edit controls set wrapping at creation; recreate only on a view change.
 if(controls[Detail])DestroyWindow(controls[Detail]);
 make(Detail,L"EDIT",L"",ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL|WS_VSCROLL|WS_TABSTOP|(simple()?0:ES_AUTOHSCROLL|WS_HSCROLL),WS_EX_CLIENTEDGE);
 SendMessageW(controls[Detail],EM_SETLIMITTEXT,32*1024*1024,0);SendMessageW(controls[Detail],WM_SETFONT,reinterpret_cast<WPARAM>(simple()?font:mono),TRUE);
 SendMessageW(controls[Search],EM_SETCUEBANNER,0,reinterpret_cast<LPARAM>(simple()?L"Search findings":L"Search fields, values, sources, warnings"));
}
Json report(){Json j={{"application","File Analysis"},{"version",FA_VERSION},{"schema_version",3},{"files",Json::array()},{"batch_errors",batchErrors},{"cancelled",analysisCancelled}};for(auto& r:results)j["files"].push_back(r.json());if(!comparison.is_null())j["comparison"]=comparison;return j;}
std::array<std::string,7> rowText(const Row& row){
 if(row.kind==9){auto& f=findings.at(row.index);return {f.topic,f.title,f.value,"","","",""};}
 if(row.kind==10)return {"Attention","Batch finding",batchErrors.at(row.index),"","","",""};
 if(row.kind==4){auto& c=comparison["changes"][row.index];return {"Comparison",display(c["section"]),display(c["change"]),"Native","", "",display(c)};}
 auto& r=results.at(selectedFile);
 if(row.kind==5){auto& g=groups[row.index];return {"Structure",display(g["type"])+" x "+display(g["count"]),display(g["total_bytes"])+" bytes; CRC valid "+display(g["crc_valid_count"])+", mismatch "+display(g["crc_mismatch_count"]),"Native PNG","Individual hashes retained",display(g["offset"]),display(g["note"])};}
 if(row.kind==6){auto& m=r.provenance["manifests"][row.index];std::string actions;for(auto& a:m["actions"]){if(!actions.empty())actions+=" -> ";actions+=a.is_object()&&a.contains("action")&&a["action"].is_string()?a["action"].get<std::string>():"Unsupported action value: "+display(a);if(a.is_object()&&a.contains("description")&&a["description"].is_string())actions+=": "+a["description"].get<std::string>();}bool active=r.provenance["active_manifest"]==m["id"];return {"Provenance",display(m["id"])+(active?" [active]":""),actions,display(m["claim_generator_info"]),"Embedded claims","Unavailable",display(m["independent_validation"])};}
 if(row.kind==7){auto& edge=r.provenance["relationships"][row.index];return {"Provenance","Ingredient "+display(edge["relationship"]),display(edge["ingredient_manifest"])+" -> "+display(edge["manifest"]),"Embedded C2PA link","","Unavailable",edge["resolved"]==true?"Relationship resolved within manifest store":"Ingredient manifest not present in this store"};}
 if(row.kind==8)return {"C2PA validation","Independent validation details",display(r.provenance["verification"]),"c2patool","","Unavailable","Historical embedded validation claims are not used as current validation results"};
 if(row.kind==0){auto& f=r.fields[row.index];return {f.category,f.name,display(f.value),f.source,display(f.raw),f.offset?std::to_string(*f.offset)+" + "+std::to_string(*f.size):"Unavailable",f.warning};}
 if(row.kind==1){auto& b=r.blocks[row.index];return {"Structure",b.type,std::to_string(b.size)+" bytes", "Native PNG/JPEG",b.sha256,std::to_string(b.offset),b.note};}
 if(row.kind==2){auto& g=r.generation[row.index];return {"Generation",display(g["name"]),display(g["value"]),display(g["source_field"]),"", "Unavailable", "Unverified embedded claim"};}
 return {"Error","Analysis finding/error",r.errors[row.index],"Analysis","","",r.errors[row.index]};
}
std::string rowDetail(const Row& row){
 if(row.kind==9){auto& f=findings.at(row.index);return f.title+"\r\n"+f.value+"\r\n\r\n"+f.detail;}
 if(row.kind==10)return batchErrors.at(row.index);
 if(row.kind==4)return comparison["changes"][row.index].dump(2,' ',false,Json::error_handler_t::replace);
 auto& r=results.at(selectedFile);
 if(row.kind==5){auto j=groups[row.index];j["individual_blocks"]=Json::array();size_t first=j["first_block_index"],count=j["count"];for(size_t i=first;i<first+count;++i){auto& b=r.blocks[i];j["individual_blocks"].push_back({{"index",i},{"offset",b.offset},{"size",b.size},{"sha256",b.sha256},{"note",b.note}});}j["inspection"]="Uncheck Group repeated IDAT to inspect or extract individual chunks.";return j.dump(2);}
 if(row.kind==6){auto m=r.provenance["manifests"][row.index];if(r.provenance["active_manifest"]==m["id"])m["independent_verification"]=r.provenance["verification"];return m.dump(2,' ',false,Json::error_handler_t::replace);}
 if(row.kind==7)return r.provenance["relationships"][row.index].dump(2);
 if(row.kind==8)return r.provenance["verification"].dump(2,' ',false,Json::error_handler_t::replace);
 if(row.kind==0){auto& f=r.fields[row.index];return Json{{"category",f.category},{"name",f.name},{"value",f.value},{"raw_numeric_or_representation",f.raw},{"source",f.source},{"offset",f.offset?Json(*f.offset):Json(nullptr)},{"size",f.size?Json(*f.size):Json(nullptr)},{"warning",f.warning}}.dump(2,' ',false,Json::error_handler_t::replace);}
 if(row.kind==1){auto& b=r.blocks[row.index];return "Type: "+b.type+"\r\nOffset: "+std::to_string(b.offset)+"\r\nSize: "+std::to_string(b.size)+"\r\nSHA-256: "+b.sha256+"\r\n"+b.note+"\r\n\r\n"+hexPage(r.path,b.offset,512);}
 if(row.kind==2)return r.generation[row.index].dump(2,' ',false,Json::error_handler_t::replace);
 return r.errors[row.index];
}
void refresh(bool categories=false){
 if(updating)return;updating=true;rows.clear();ListView_SetItemCount(controls[Table],0);SetWindowTextW(controls[Detail],L"");
 int mode=static_cast<int>(SendMessageW(controls[Mode],CB_GETCURSEL,0,0));
 if(categories){SendMessageW(controls[Category],CB_RESETCONTENT,0,0);SendMessageW(controls[Category],CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"All categories"));std::set<std::string> cats;if(simple()){for(auto& f:findings)cats.insert(f.topic);}else {cats={"Generation","Error","Structure","Comparison","Provenance","C2PA validation"};if(selectedFile>=0)for(auto& f:results[selectedFile].fields)cats.insert(f.category);}for(auto& c:cats){auto s=wide(c);SendMessageW(controls[Category],CB_ADDSTRING,0,reinterpret_cast<LPARAM>(s.c_str()));}SendMessageW(controls[Category],CB_SETCURSEL,0,0);}
 auto query=text(controls[Search]),cat=text(controls[Category]);std::transform(query.begin(),query.end(),query.begin(),towlower);
 auto include=[&](Row row){auto cells=rowText(row);if(cat!=L"All categories"&&cat!=wide(cells[0]))return;std::wstring hay;for(auto& s:cells){hay+=wide(s);hay+=L' ';}if(simple()&&!query.empty())hay+=wide(rowDetail(row));std::transform(hay.begin(),hay.end(),hay.begin(),towlower);if(query.empty()||hay.find(query)!=std::wstring::npos)rows.push_back(row);};
 try {
  if(simple()){for(size_t i=0;i<batchErrors.size();++i)include({10,i});if(selectedFile>=0)for(size_t i=0;i<findings.size();++i)include({9,i});}
  else if(mode==ComparisonView){if(!comparison.is_null())for(size_t i=0;i<comparison["changes"].size();++i)include({4,i});}
  else if(selectedFile>=0) {auto& r=results[selectedFile];
   if(mode==MetadataView||mode==SummaryView){
    for(size_t i=0;i<r.fields.size();++i){auto& f=r.fields[i];bool concise=f.category=="Summary"||f.category=="Photo / device"||f.category=="AI provenance"||f.category=="C2PA validation"||f.category=="Image"||f.category=="Hashes"||f.name=="Signature identification"||f.name=="Model parameters result"||f.name=="Size (bytes)";if(mode==MetadataView||concise)include({0,i});}
    if(mode==MetadataView)for(size_t i=0;i<r.generation.size();++i)include({2,i});
    if(mode==SummaryView&&r.provenance.is_object()){
     auto& p=r.provenance;if(p["history_order"].is_array())for(auto& id:p["history_order"])for(size_t i=0;i<p["manifests"].size();++i)if(p["manifests"][i]["id"]==id)include({6,i});
     for(size_t i=0;i<p["relationships"].size();++i)include({7,i});
    }
    for(size_t i=0;i<r.errors.size();++i)include({3,i});
   }
   else if(mode==StructureView){groups=structureGroups(r,SendMessageW(controls[GroupIdat],BM_GETCHECK,0,0)==BST_CHECKED);for(size_t i=0;i<groups.size();++i)if(groups[i]["count"]==1)include({1,groups[i]["first_block_index"].get<size_t>()});else include({5,i});}
   else if(mode==ProvenanceView&&r.provenance.is_object()){auto& p=r.provenance;std::set<size_t> added;if(p["history_order"].is_array())for(auto& id:p["history_order"])for(size_t i=0;i<p["manifests"].size();++i)if(p["manifests"][i]["id"]==id){include({6,i});added.insert(i);}for(size_t i=0;i<p["manifests"].size();++i)if(!added.contains(i))include({6,i});for(size_t i=0;i<p["relationships"].size();++i)include({7,i});include({8,0});}
   else if(mode==HexView){auto dump=hexPage(r.path,hexOffset,512);SetWindowTextW(controls[Detail],wide(dump).c_str());}
  }
  ListView_SetItemCountEx(controls[Table],static_cast<int>(rows.size()),LVSICF_NOSCROLL);
 }catch(const std::exception& e){status(wide(e.what()));}
 updating=false;
 if(simple()){if(!rows.empty()){ListView_SetItemState(controls[Table],0,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);SetWindowTextW(controls[Detail],wide(rowDetail(rows.front())).c_str());}else SetWindowTextW(controls[Detail],selectedFile>=0?L"No matching findings. Clear the search or choose All categories.":L"Open a file to see provenance, generation settings, embedded claims and unusual findings.\r\n\r\nChoose Detailed for all metadata and binary inspection tools. Export Results always includes the full analysis.");}
 layout();enable();InvalidateRect(controls[Table],nullptr,TRUE);
}
void copyText(const std::wstring& s){if(!OpenClipboard(window))throw std::runtime_error("Clipboard unavailable");struct Clip{~Clip(){CloseClipboard();}}guard;size_t n=(s.size()+1)*sizeof(wchar_t);HGLOBAL data=GlobalAlloc(GMEM_MOVEABLE,n);if(!data)throw std::runtime_error("Clipboard allocation failed");void* p=GlobalLock(data);if(!p){GlobalFree(data);throw std::runtime_error("Clipboard lock failed");}memcpy(p,s.c_str(),n);GlobalUnlock(data);EmptyClipboard();if(!SetClipboardData(CF_UNICODETEXT,data)){GlobalFree(data);throw std::runtime_error("Clipboard write failed");}}
std::optional<fs::path> save(LPCWSTR filter,LPCWSTR extension,LPCWSTR name){std::array<wchar_t,32768>b{};wcscpy_s(b.data(),b.size(),name);OPENFILENAMEW d{sizeof(d)};d.hwndOwner=window;d.lpstrFilter=filter;d.lpstrFile=b.data();d.nMaxFile=static_cast<DWORD>(b.size());d.lpstrDefExt=extension;d.lpstrTitle=L"Save to a NEW file (existing files are protected)";d.Flags=OFN_EXPLORER|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;if(!GetSaveFileNameW(&d))return {};return fs::path(b.data());}
template<class F>void launch(F work){if(busy)return;busy=true;cancelled=false;enable();SendMessageW(controls[Bar],PBM_SETPOS,0,0);worker=std::thread([work=std::move(work)]()mutable{auto packet=std::make_unique<Packet>();try{work(*packet);}catch(const std::exception& e){packet->errors.push_back(e.what());packet->message=wide(e.what());}if(!PostMessageW(window,Done,0,reinterpret_cast<LPARAM>(packet.get())))return;packet.release();});}
void scan(){
 auto paths=inputPaths;auto dir=folder;bool recurse=SendMessageW(controls[Recursive],BM_GETCHECK,0,0)==BST_CHECKED;
 selectedFile=-1;results.clear();findings.clear();batchErrors.clear();comparison=nullptr;SendMessageW(controls[Files],LB_RESETCONTENT,0,0);refresh(true);status(L"Analyzing files... Cancel is available.");
 launch([paths=std::move(paths),dir,recurse](Packet& p)mutable{p.analysis=true;if(!dir.empty())paths=enumerate(dir,recurse,cancelled,p.errors);if(paths.size()>256){paths.resize(256);p.errors.push_back("Batch limited to 256 files");}wchar_t exe[32768];GetModuleFileNameW(nullptr,exe,32768);Options opt;opt.exiftool=fs::path(exe).parent_path()/L"exiftool"/L"exiftool.exe";uint64_t retained=0;
  for(size_t i=0;i<paths.size();++i){if(cancelled){p.errors.push_back("Cancelled; "+std::to_string(paths.size()-i)+" queued files not analyzed");break;}auto r=analyze(paths[i],opt,cancelled,[i,count=paths.size()](unsigned n){PostMessageW(window,Progress,static_cast<WPARAM>((i*100+n)/std::max<size_t>(1,count)),static_cast<LPARAM>(i+1));});retained+=r.json().dump(-1,' ',false,Json::error_handler_t::replace).size();p.files.push_back(std::move(r));if(retained>64*1024*1024){p.errors.push_back("Batch retention budget (64 MiB serialized) reached; remaining files not analyzed");break;}}
  p.message=L"Analysis finished: "+std::to_wstring(p.files.size())+L" file(s).";if(cancelled)p.message+=L" Cancelled.";
 });
}
void openFiles(){std::vector<wchar_t>b(65536);OPENFILENAMEW d{sizeof(d)};d.hwndOwner=window;d.lpstrFilter=L"All files\0*.*\0\0";d.lpstrFile=b.data();d.nMaxFile=static_cast<DWORD>(b.size());d.Flags=OFN_EXPLORER|OFN_ALLOWMULTISELECT|OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;if(!GetOpenFileNameW(&d))return;inputPaths.clear();folder.clear();auto p=b.data();fs::path first=p;p+=wcslen(p)+1;if(!*p)inputPaths.push_back(first);else while(*p){inputPaths.push_back(first/p);p+=wcslen(p)+1;}scan();}
void openFolder(){IFileOpenDialog* d=nullptr;if(FAILED(CoCreateInstance(CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&d))))throw std::runtime_error("Folder dialog unavailable");DWORD flags=0;d->GetOptions(&flags);d->SetOptions(flags|FOS_PICKFOLDERS|FOS_FORCEFILESYSTEM);if(SUCCEEDED(d->Show(window))){IShellItem* item=nullptr;if(SUCCEEDED(d->GetResult(&item))){PWSTR p=nullptr;if(SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH,&p))){folder=p;inputPaths.clear();SetWindowTextW(controls[Info],(L"Folder: "+folder.wstring()+L" — press Scan").c_str());CoTaskMemFree(p);}item->Release();}}d->Release();enable();}
void updateInfo(){if(selectedFile<0)return;auto& r=results[selectedFile];std::wstring info=r.path.wstring()+L"\r\n"+wide(r.format)+L" | "+wide(r.status);if(!simple())info+=L" | "+std::to_wstring(r.size)+L" bytes | "+std::to_wstring(r.errors.size())+L" findings/errors";SetWindowTextW(controls[Info],info.c_str());}
void selectFile(){int idx=static_cast<int>(SendMessageW(controls[Files],LB_GETCARETINDEX,0,0));if(idx<0||idx>=static_cast<int>(results.size()))return;selectedFile=idx;findings=simpleFindings(results[idx]);hexOffset=0;SetWindowTextW(controls[Offset],L"0x0");updateInfo();refresh(true);}
void command(int id,int notification){
 switch(id){
 case Open:if(!busy)openFiles();break;case Folder:if(!busy)openFolder();break;case Scan:if(!busy)scan();break;case Cancel:cancelled=true;status(L"Cancelling; completed and partial results will be retained.");break;
 case Files:if(notification==LBN_SELCHANGE)selectFile();break;
 case Level:if(notification==CBN_SELCHANGE){configurePresentation();SendMessageW(controls[Search],WM_SETTEXT,0,reinterpret_cast<LPARAM>(L""));updateInfo();refresh(true);}break;
 case Search:if(notification==EN_CHANGE)refresh();break;case Category:if(notification==CBN_SELCHANGE)refresh();break;case Mode:if(notification==CBN_SELCHANGE)refresh(true);break;case GroupIdat:refresh();break;
 case Copy:{std::wstring s;int count=ListView_GetSelectedCount(controls[Table]);int i=-1;while((i=ListView_GetNextItem(controls[Table],i,LVNI_SELECTED))>=0){if(count==1)s=wide(rowDetail(rows.at(i)));else{auto c=rowText(rows.at(i));s+=wide(c[1]+"\t"+c[2]+"\r\n");}}if(s.empty())s=text(controls[Detail]);if(s.empty())throw std::runtime_error("Select a row or a nonempty hex page to copy");copyText(s);status(L"Copied selection to clipboard.");break;}
 case Export:if(!busy){auto dest=save(L"JSON\0*.json\0CSV\0*.csv\0Plain text\0*.txt\0\0",L"json",L"analysis.json");if(dest){auto ext=dest->extension().wstring();std::transform(ext.begin(),ext.end(),ext.begin(),towlower);std::string format=ext==L".csv"?"csv":ext==L".txt"?"txt":"json";bool privacy=SendMessageW(controls[Privacy],BM_GETCHECK,0,0)==BST_CHECKED;launch([dest=*dest,format,privacy](Packet& p){std::vector<fs::path> originals;for(auto& r:results)originals.push_back(r.path);auto j=report();if(privacy)j=redactLocalPaths(j,originals);exportReport(dest,j,format,originals,&cancelled);p.message=L"Exported "+dest.wstring()+(privacy?L" (local paths redacted; other personal metadata may remain)":L"");});status(L"Exporting report...");}}break;
 case Compare:if(!busy&&results.size()>=2){int items[2]{};int n=static_cast<int>(SendMessageW(controls[Files],LB_GETSELCOUNT,0,0));if(n!=2)throw std::runtime_error("Select exactly two analyzed files in the file list using Ctrl+click.");SendMessageW(controls[Files],LB_GETSELITEMS,2,reinterpret_cast<LPARAM>(items));launch([a=items[0],b=items[1]](Packet& p){p.comparison=fa::compare(results[a],results[b],&cancelled);p.message=L"Comparison complete; observed differences only.";});status(L"Comparing analyzed files...");}break;
 case Workflow:if(!busy&&selectedFile>=0){auto dest=save(L"JSON\0*.json\0\0",L"json",L"generation.json");if(dest){int idx=selectedFile;bool privacy=SendMessageW(controls[Privacy],BM_GETCHECK,0,0)==BST_CHECKED;launch([dest=*dest,idx,privacy](Packet& p){Json j={{"source_file",utf8(results[idx].path.wstring())},{"generation",results[idx].generation}};if(privacy)j=redactLocalPaths(j,{results[idx].path});exportReport(dest,j,"json",{results[idx].path},&cancelled);p.message=L"Exported recovered generation metadata. ComfyUI workflow JSON is under generation[].value.";});}}break;
 case Extract:if(!busy&&selectedFile>=0){int idx=ListView_GetNextItem(controls[Table],-1,LVNI_SELECTED);if(idx>=0&&rows.at(idx).kind==1){auto dest=save(L"Binary data\0*.bin\0\0",L"bin",L"block.bin");if(dest){auto block=results[selectedFile].blocks[rows[idx].index];auto source=results[selectedFile].path;launch([source,block,dest=*dest](Packet& p){extractBlock(source,block,dest,&cancelled);p.message=L"Extracted complete raw block to "+dest.wstring();});status(L"Verifying and extracting raw block...");}}}break;
 case Go:{auto s=text(controls[Offset]);size_t used=0;auto n=std::stoull(s,&used,0);if(used!=s.size())throw std::runtime_error("Enter a decimal offset or 0x-prefixed hexadecimal offset");hexOffset=n;refresh();break;}
 case Prev:hexOffset=hexOffset>512?hexOffset-512:0;SetWindowTextW(controls[Offset],std::to_wstring(hexOffset).c_str());refresh();break;
 case Next:if(selectedFile>=0){hexOffset=std::min<uint64_t>(hexOffset+512,results[selectedFile].size);SetWindowTextW(controls[Offset],std::to_wstring(hexOffset).c_str());refresh();}break;
 }
}
LRESULT CALLBACK wndproc(HWND h,UINT msg,WPARAM wp,LPARAM lp){try{switch(msg){
 case WM_CREATE:{window=h;NONCLIENTMETRICSW metrics{sizeof(metrics)};SystemParametersInfoW(SPI_GETNONCLIENTMETRICS,sizeof(metrics),&metrics,0);font=CreateFontIndirectW(&metrics.lfMessageFont);mono=CreateFontW(-MulDiv(14,static_cast<int>(GetDpiForWindow(window)),96),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,FIXED_PITCH,L"Consolas");
  for(auto [id,title]:std::vector<std::pair<int,LPCWSTR>>{{Open,L"Open File..."},{Folder,L"Open Folder..."},{Scan,L"Scan"},{Cancel,L"Cancel"},{Export,L"Export Results..."},{Copy,L"Copy"},{Compare,L"Compare two"},{Workflow,L"Export generation..."},{Extract,L"Extract block..."}})make(id,L"BUTTON",title,BS_PUSHBUTTON|WS_TABSTOP);
  make(Recursive,L"BUTTON",L"Scan subfolders (skip links)",BS_AUTOCHECKBOX|WS_TABSTOP);
  make(Privacy,L"BUTTON",L"Redact local paths in exports",BS_AUTOCHECKBOX|WS_TABSTOP);SendMessageW(controls[Privacy],BM_SETCHECK,BST_CHECKED,0);
  make(GroupIdat,L"BUTTON",L"Group repeated IDAT chunks",BS_AUTOCHECKBOX|WS_TABSTOP);SendMessageW(controls[GroupIdat],BM_SETCHECK,BST_CHECKED,0);
  make(Info,L"EDIT",L"Open files to analyze. Offline. Originals opened for read only.",ES_READONLY|ES_MULTILINE|ES_AUTOHSCROLL);
  make(Files,L"LISTBOX",L"",LBS_EXTENDEDSEL|LBS_NOTIFY|LBS_NOINTEGRALHEIGHT|WS_VSCROLL|WS_HSCROLL|WS_TABSTOP,WS_EX_CLIENTEDGE);SendMessageW(controls[Files],LB_SETHORIZONTALEXTENT,1500,0);
  make(ViewLabel,L"STATIC",L"View:",SS_LEFT);make(Level,L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP);for(auto s:{L"Simple",L"Detailed"})SendMessageW(controls[Level],CB_ADDSTRING,0,reinterpret_cast<LPARAM>(s));SendMessageW(controls[Level],CB_SETCURSEL,0,0);
  make(SectionLabel,L"STATIC",L"Section:",SS_LEFT);make(Mode,L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP);for(auto s:{L"Overview",L"Metadata",L"Structure",L"Hex",L"Comparison",L"Provenance"})SendMessageW(controls[Mode],CB_ADDSTRING,0,reinterpret_cast<LPARAM>(s));SendMessageW(controls[Mode],CB_SETCURSEL,MetadataView,0);
  make(Category,L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL|WS_TABSTOP);make(Search,L"EDIT",L"",ES_AUTOHSCROLL|WS_TABSTOP,WS_EX_CLIENTEDGE);SendMessageW(controls[Search],EM_SETCUEBANNER,0,reinterpret_cast<LPARAM>(L"Search fields, values, sources, warnings"));
  make(Offset,L"EDIT",L"0x0",ES_AUTOHSCROLL|WS_TABSTOP,WS_EX_CLIENTEDGE);make(Go,L"BUTTON",L"Go",WS_TABSTOP);make(Prev,L"BUTTON",L"Previous",WS_TABSTOP);make(Next,L"BUTTON",L"Next",WS_TABSTOP);
  make(Table,WC_LISTVIEWW,L"",LVS_REPORT|LVS_OWNERDATA|LVS_SHOWSELALWAYS|WS_TABSTOP,WS_EX_CLIENTEDGE);ListView_SetExtendedListViewStyle(controls[Table],LVS_EX_FULLROWSELECT|LVS_EX_GRIDLINES|LVS_EX_DOUBLEBUFFER);
  configurePresentation();
  make(Status,L"STATIC",L"Ready. Select rows for complete values; Ctrl+C copies selected rows.",SS_LEFT);make(Bar,PROGRESS_CLASSW,L"",0);SendMessageW(controls[Bar],PBM_SETRANGE32,0,100);refresh(true);enable();return 0;}
 case WM_SIZE:layout();return 0;
 case WM_GETMINMAXINFO:reinterpret_cast<MINMAXINFO*>(lp)->ptMinTrackSize={MulDiv(1000,static_cast<int>(GetDpiForWindow(h)),96),MulDiv(680,static_cast<int>(GetDpiForWindow(h)),96)};return 0;
 case WM_COMMAND:command(LOWORD(wp),HIWORD(wp));return 0;
 case WM_NOTIFY:{auto hdr=reinterpret_cast<NMHDR*>(lp);if(hdr->idFrom==Table){if(hdr->code==LVN_GETDISPINFOW){auto di=reinterpret_cast<NMLVDISPINFOW*>(lp);if((di->item.mask&LVIF_TEXT)&&di->item.iItem>=0&&di->item.iItem<static_cast<int>(rows.size())){static std::wstring cell;auto c=rowText(rows[di->item.iItem]);cell=trim(wide(c.at(di->item.iSubItem)));di->item.pszText=cell.data();}}else if(hdr->code==LVN_ITEMCHANGED&&!updating){auto n=reinterpret_cast<NMLISTVIEW*>(lp);if(n->iItem>=0&&n->iItem<static_cast<int>(rows.size())&&(n->uNewState&LVIS_SELECTED)){SetWindowTextW(controls[Detail],trim(wide(rowDetail(rows[n->iItem])),16*1024*1024).c_str());}enable();}else if(hdr->code==NM_DBLCLK){int idx=ListView_GetNextItem(controls[Table],-1,LVNI_SELECTED);if(idx>=0&&rows[idx].kind==1){hexOffset=results[selectedFile].blocks[rows[idx].index].offset;SendMessageW(controls[Mode],CB_SETCURSEL,HexView,0);SetWindowTextW(controls[Offset],std::to_wstring(hexOffset).c_str());refresh();}}else if(hdr->code==LVN_KEYDOWN){auto key=reinterpret_cast<NMLVKEYDOWN*>(lp);if(key->wVKey=='C'&&(GetKeyState(VK_CONTROL)&0x8000))command(Copy,0);}}return 0;}
 case Progress:SendMessageW(controls[Bar],PBM_SETPOS,wp,0);status(L"Analyzing file "+std::to_wstring(lp)+L" — "+std::to_wstring(wp)+L"% of batch. Cancel available.");return 0;
 case Done:{std::unique_ptr<Packet> p(reinterpret_cast<Packet*>(lp));if(worker.joinable())worker.join();busy=false;if(p->analysis){analysisCancelled=cancelled.load();results=std::move(p->files);batchErrors=std::move(p->errors);for(auto& r:results){auto label=r.path.filename().wstring()+L" ["+wide(r.status)+L"]";SendMessageW(controls[Files],LB_ADDSTRING,0,reinterpret_cast<LPARAM>(label.c_str()));}if(!results.empty()){SendMessageW(controls[Files],LB_SETSEL,TRUE,0);SendMessageW(controls[Files],LB_SETCARETINDEX,0,FALSE);selectFile();}}else{if(!p->comparison.is_null()){comparison=std::move(p->comparison);SendMessageW(controls[Mode],CB_SETCURSEL,ComparisonView,0);SendMessageW(controls[Category],CB_SETCURSEL,0,0);SetWindowTextW(controls[Search],L"");refresh();}if(!p->errors.empty())MessageBoxW(window,wide(p->errors.front()).c_str(),L"Operation failed",MB_OK|MB_ICONERROR);}if(!batchErrors.empty()){p->message+=L" Batch errors: "+std::to_wstring(batchErrors.size())+L" (included in export).";std::string detail;for(auto& e:batchErrors)detail+=e+"\r\n";SetWindowTextW(controls[Detail],wide(detail).c_str());}status(p->message);SendMessageW(controls[Bar],PBM_SETPOS,100,0);enable();if(closing)DestroyWindow(window);return 0;}
 case WM_CLOSE:if(busy){closing=true;cancelled=true;status(L"Cancelling active analysis before closing...");}else DestroyWindow(h);return 0;
 case WM_DESTROY:if(font)DeleteObject(font);if(mono)DeleteObject(mono);PostQuitMessage(0);return 0;
 }}catch(const std::exception& e){error(e);}return DefWindowProcW(h,msg,wp,lp);}
}
int WINAPI wWinMain(HINSTANCE h,HINSTANCE,LPWSTR,int show){instance=h;SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32|LOAD_LIBRARY_SEARCH_APPLICATION_DIR);CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);INITCOMMONCONTROLSEX init{sizeof(init),ICC_LISTVIEW_CLASSES|ICC_PROGRESS_CLASS};InitCommonControlsEx(&init);WNDCLASSEXW cls{sizeof(cls)};cls.lpfnWndProc=wndproc;cls.hInstance=h;cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);cls.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_BTNFACE+1);cls.lpszClassName=L"FileAnalysisWindow";RegisterClassExW(&cls);auto hwnd=CreateWindowExW(0,cls.lpszClassName,wide(std::string("File Analysis ")+FA_VERSION).c_str(),WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,MulDiv(1240,static_cast<int>(GetDpiForSystem()),96),MulDiv(800,static_cast<int>(GetDpiForSystem()),96),nullptr,nullptr,h,nullptr);if(!hwnd)return 1;ShowWindow(hwnd,show);UpdateWindow(hwnd);
 int argc=0;auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);for(int i=1;i<argc;++i)inputPaths.emplace_back(argv[i]);LocalFree(argv);if(!inputPaths.empty())scan();MSG msg{};while(GetMessageW(&msg,nullptr,0,0)>0){if(!IsDialogMessageW(hwnd,&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}}if(worker.joinable())worker.join();CoUninitialize();return 0;}
