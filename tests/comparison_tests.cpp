#include "comparison_view.hpp"
#include <iostream>
using namespace fa;
namespace {int checks=0;void check(bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);}}
int main(){try{
 Result a,b;a.path=L"before.png";b.path=L"after.png";a.format=b.format="PNG";
 a.fields.push_back({"XMP","Creator","ExifTool","","Alice","Alice",{},{}});b.fields.push_back({"XMP","Creator","ExifTool","","Bob","Bob",{},{}});
 a.fields.push_back({"XMP","Removed note","ExifTool","","old",nullptr,{},{}});b.fields.push_back({"XMP","Added note","ExifTool","","new",nullptr,{},{}});
 a.fields.push_back({"PNG text","Author","Native","","First",nullptr,{},{}});a.fields.push_back({"PNG text","Author","Native","","Second",nullptr,{},{}});b.fields.push_back(a.fields[2]);b.fields.push_back({"PNG text","Author","Native","","Third",nullptr,{},{}});
 a.blocks.push_back({"IHDR","","oldhash",8,25});b.blocks.push_back({"IHDR","","newhash",8,25});b.blocks.push_back({"IEND","","endhash",33,12});
 auto report=compare(a,b);auto original=report;auto view=comparisonView(report);
 check(view.added==2&&view.removed==1&&view.changed==3,"Added/removed/changed metadata groups and raw blocks counted");
 bool creator=false,duplicates=false,block=false,added=false;
 for(const auto& entry:view.entries){auto detail=comparisonDetail(report,entry);
  if(entry.section=="XMP:Creator"){creator=entry.left=="Alice"&&entry.right=="Bob"&&detail.find("Source: ExifTool")!=detail.npos&&detail.find("Raw / numeric representation")!=detail.npos;}
  if(entry.section=="PNG text:Author")duplicates=entry.left.find("2 occurrences")!=entry.left.npos&&detail.find("Second")!=detail.npos&&detail.find("Third")!=detail.npos;
  if(entry.blockIndex==0)block=entry.left.find("oldhash")!=entry.left.npos&&entry.right.find("newhash")!=entry.right.npos&&detail.find("not inferred moves")!=detail.npos;
  if(entry.section=="XMP:Added note")added=entry.change=="Added"&&entry.left=="Not present"&&entry.right=="new";
 }
 check(creator,"Readable before/after fields retain parser and raw values");check(duplicates,"Duplicate occurrences retained and counted together");check(block,"Block SHA-256 differences have explicit positional scope");check(added,"New values clearly distinguished from absent values");check(report==original,"Presentation never mutates comparison export");
 auto same=comparisonView(compare(a,a));check(same.entries.empty()&&!same.truncated,"Identical evidence has no fabricated difference rows");
 a.fields.push_back({"Analysis diagnostics","Now","ExifTool","","yesterday",nullptr,{},{}});b=a;b.fields.back().value="today";check(comparisonView(compare(a,b)).entries.empty(),"Volatile scanner values remain excluded from comparison");
 std::atomic_bool cancel=true;bool refused=false;try{comparisonView(report,&cancel);}catch(...){refused=true;}check(refused,"Presentation respects worker cancellation");
 Json many={{"changes",Json::array()}};for(size_t n=0;n<5001;++n)many["changes"].push_back({{"section","Test"},{"left",0},{"right",1}});auto bounded=comparisonView(many);check(bounded.truncated&&bounded.entries.size()==5000,"Large comparison display has explicit cap; report remains complete");
 std::cout<<"PASS: "<<checks<<" comparison presentation assertions\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<" comparison assertions: "<<e.what()<<"\n";return 1;}}
