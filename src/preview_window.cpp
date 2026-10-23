#include "preview_window.hpp"
#include <algorithm>
#include <memory>
#include <stdexcept>
namespace fa {
namespace {
struct ImageWindow {RasterPixels pixels;std::wstring caption;};
HWND currentPreview=nullptr;
LRESULT CALLBACK previewProc(HWND h,UINT message,WPARAM wp,LPARAM lp){
 auto state=reinterpret_cast<ImageWindow*>(GetWindowLongPtrW(h,GWLP_USERDATA));
 if(message==WM_NCCREATE){auto create=reinterpret_cast<CREATESTRUCTW*>(lp);auto pending=reinterpret_cast<std::unique_ptr<ImageWindow>*>(create->lpCreateParams);SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(pending->release()));return DefWindowProcW(h,message,wp,lp);}
 if(message==WM_NCDESTROY){if(currentPreview==h)currentPreview=nullptr;SetWindowLongPtrW(h,GWLP_USERDATA,0);delete state;return DefWindowProcW(h,message,wp,lp);}
 if(message==WM_SIZE){InvalidateRect(h,nullptr,TRUE);return 0;}
 if(message==WM_GETMINMAXINFO){reinterpret_cast<MINMAXINFO*>(lp)->ptMinTrackSize={400,300};return 0;}
 if(message==WM_KEYDOWN&&wp==VK_ESCAPE){DestroyWindow(h);return 0;}
 if(message==WM_PAINT){PAINTSTRUCT paint{};auto dc=BeginPaint(h,&paint);RECT client{};GetClientRect(h,&client);FillRect(dc,&client,reinterpret_cast<HBRUSH>(COLOR_WINDOW+1));
  if(state){RECT text{8,8,client.right-8,88};DrawTextW(dc,state->caption.c_str(),-1,&text,DT_LEFT|DT_WORDBREAK|DT_NOPREFIX);int aw=std::max(1L,client.right-16),ah=std::max(1L,client.bottom-104);double scale=std::min({1.0,double(aw)/state->pixels.width,double(ah)/state->pixels.height});int width=std::max(1,static_cast<int>(state->pixels.width*scale)),height=std::max(1,static_cast<int>(state->pixels.height*scale));
   BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=static_cast<LONG>(state->pixels.width);info.bmiHeader.biHeight=-static_cast<LONG>(state->pixels.height);info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;SetStretchBltMode(dc,HALFTONE);SetBrushOrgEx(dc,0,0,nullptr);StretchDIBits(dc,8+(aw-width)/2,96+(ah-height)/2,width,height,0,0,state->pixels.width,state->pixels.height,state->pixels.bgra.data(),&info,DIB_RGB_COLORS,SRCCOPY);
  }EndPaint(h,&paint);return 0;
 }
 return DefWindowProcW(h,message,wp,lp);
}
}
void showPreviewWindow(HWND owner,HINSTANCE instance,RasterPixels pixels,std::wstring title){
 if(!pixels.width||!pixels.height||pixels.bgra.size()!=uint64_t(pixels.width)*pixels.height*4)throw std::runtime_error("Invalid decoded preview buffer");
 if(currentPreview&&IsWindow(currentPreview))DestroyWindow(currentPreview);
 WNDCLASSEXW cls{sizeof(cls)};cls.lpfnWndProc=previewProc;cls.hInstance=instance;cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);cls.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);cls.lpszClassName=L"FileAnalysisPreview";if(!RegisterClassExW(&cls)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)throw std::runtime_error("Cannot register preview window");
 auto state=std::make_unique<ImageWindow>();state->caption=title+L"\r\nFirst frame; aspect preserved; transparency shown on white. Recovered at scan time.";state->pixels=std::move(pixels);
 auto h=CreateWindowExW(0,cls.lpszClassName,L"Embedded preview — File Analysis",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,820,680,owner,nullptr,instance,&state);if(!h)throw std::runtime_error("Cannot create preview window");currentPreview=h;ShowWindow(h,SW_SHOW);UpdateWindow(h);
}
}
