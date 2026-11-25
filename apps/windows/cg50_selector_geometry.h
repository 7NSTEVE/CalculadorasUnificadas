#pragma once
#include <windows.h>
#include <vector>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include "cg50_keys_exact.h"
#include "key_capture_rects.h"

// Derive the physical button silhouette from the actual faceplate pixels.
// Captions outside the button are disconnected components and are excluded.
// Drawing and hit testing use these same source-coordinate regions.
namespace cg50_selector {
inline HRGN regions[50]{};
inline bool attempted=false;

inline void Release()
{
    for(auto& region:regions){if(region)DeleteObject(region);region=nullptr;}
    attempted=false;
}

inline int Direction(int x,int y)
{
    const int dx=x-512,dy=y-674;
    if(dx*dx+dy*dy<28*28)return -1;
    if(std::abs(dx)>std::abs(dy))return dx<0?11:12;
    return dy<0?10:13;
}

inline bool Ensure(HBITMAP skin)
{
    if(attempted)return regions[49]!=nullptr;
    attempted=true;
    BITMAP bm{};
    if(!skin||!GetObjectW(skin,sizeof bm,&bm)||bm.bmWidth<=0||bm.bmHeight<=0)return false;
    const int width=bm.bmWidth,height=bm.bmHeight;
    BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth=width;info.bmiHeader.biHeight=-height;
    info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    std::vector<std::uint32_t> pixels(static_cast<size_t>(width)*height);
    HDC dc=GetDC(nullptr);
    const int rows=GetDIBits(dc,skin,0,height,pixels.data(),&info,DIB_RGB_COLORS);
    ReleaseDC(nullptr,dc);
    if(rows!=height)return false;
    for(int id=0;id<50;++id){
        const auto& key=g_cg50ExactKeys[id];
        auto spec=CalculatorKeyCaptureSpec(2,id,key.cx,key.cy,key.w,key.h,width,height);
        const RECT original=spec.bounds;
        const auto background=pixels[original.top*width+original.left];
        RECT search=original;
        search.left=search.left>12?search.left-12:0;
        search.top=search.top>12?search.top-12:0;
        search.right=search.right+12<width?search.right+12:width;
        search.bottom=search.bottom+12<height?search.bottom+12:height;
        // These two boxed legends touch the cap through their printed shadow.
        // They belong to the legend row, not to the physical push button.
        if((id==14||id==17)&&search.top<692)search.top=692;
        const int w=search.right-search.left,h=search.bottom-search.top;
        auto foreground=[&](int x,int y){
            const auto value=pixels[y*width+x];
            for(int shift=0;shift<=16;shift+=8)
                if(std::abs(int((value>>shift)&255)-int((background>>shift)&255))>8)return true;
            return false;
        };
        int seedX=key.cx,seedY=key.cy;
        if(!foreground(seedX,seedY)){
            bool found=false;
            for(int radius=1;radius<=10&&!found;++radius)
                for(int y=key.cy-radius;y<=key.cy+radius&&!found;++y)
                    for(int x=key.cx-radius;x<=key.cx+radius;++x)
                        if(x>=search.left&&x<search.right&&y>=search.top&&y<search.bottom&&foreground(x,y)){
                            seedX=x;seedY=y;found=true;break;
                        }
            if(!found){Release();attempted=true;return false;}
        }
        std::vector<unsigned char> seen(static_cast<size_t>(w)*h);
        std::vector<int> queue;
        const int seed=(seedY-search.top)*w+seedX-search.left;
        seen[seed]=1;queue.push_back(seed);
        for(size_t next=0;next<queue.size();++next){
            const int at=queue[next],x=at%w,y=at/w;
            for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx){
                const int nx=x+dx,ny=y+dy;
                if(nx<0||nx>=w||ny<0||ny>=h)continue;
                const int n=ny*w+nx;
                if(!seen[n]&&foreground(search.left+nx,search.top+ny)){
                    seen[n]=1;queue.push_back(n);
                }
            }
        }
        std::vector<RECT> runs;
        // Fill enclosed text/anti-alias holes; physical key bodies are row-convex.
        for(int y=0;y<h;++y){
            int first=w,last=-1;
            for(int x=0;x<w;++x)if(seen[y*w+x]){if(x<first)first=x;if(x>last)last=x;}
            int start=-1;
            for(int x=first;x<=last+1;++x){
                const bool keep=x<=last&&(id<10||id>13||Direction(search.left+x,search.top+y)==id);
                if(keep&&start<0)start=x;
                if(!keep&&start>=0){runs.push_back({search.left+start,search.top+y,search.left+x,search.top+y+1});start=-1;}
            }
        }
        if(runs.empty()){Release();attempted=true;return false;}
        const size_t bytes=sizeof(RGNDATAHEADER)+runs.size()*sizeof(RECT);
        std::vector<unsigned char> data(bytes);
        auto* rd=reinterpret_cast<RGNDATA*>(data.data());
        rd->rdh.dwSize=sizeof(RGNDATAHEADER);rd->rdh.iType=RDH_RECTANGLES;
        rd->rdh.nCount=static_cast<DWORD>(runs.size());rd->rdh.nRgnSize=static_cast<DWORD>(runs.size()*sizeof(RECT));
        rd->rdh.rcBound=search;
        std::memcpy(rd->Buffer,runs.data(),runs.size()*sizeof(RECT));
        regions[id]=ExtCreateRegion(nullptr,static_cast<DWORD>(bytes),rd);
        if(!regions[id]){Release();attempted=true;return false;}
    }
    return true;
}

inline int KeyAt(int x,int y)
{
    for(int id=0;id<50;++id)if(regions[id]&&PtInRegion(regions[id],x,y))return id;
    return -1;
}

inline HRGN AtScale(int id,const RECT& viewport,int sourceWidth,int sourceHeight)
{
    if(id<0||id>=50||!regions[id])return nullptr;
    const DWORD count=GetRegionData(regions[id],0,nullptr);
    if(!count)return nullptr;
    std::vector<unsigned char> bytes(count);
    auto* data=reinterpret_cast<RGNDATA*>(bytes.data());
    if(!GetRegionData(regions[id],count,data))return nullptr;
    XFORM transform{};
    transform.eM11=float(viewport.right-viewport.left)/sourceWidth;
    transform.eM22=float(viewport.bottom-viewport.top)/sourceHeight;
    transform.eDx=float(viewport.left);transform.eDy=float(viewport.top);
    return ExtCreateRegion(&transform,count,data);
}
}
