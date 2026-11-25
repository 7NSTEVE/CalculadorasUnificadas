#pragma once
#include <windows.h>
#include <vector>

// Pixel coordinates calibrated against all three original bitmap resources.
// A D-pad is a single physical piece: retain it in full for each direction.
// Parts remove neighbouring buttons where a wide legend overlaps their column.
// Every retained pixel is copied unchanged from the original bitmap.
struct CalculatorKeyCapture {
    RECT bounds{};
    RECT body{};
    RECT legend{};
    int mask = 0; // 0 rectangle, 1 body + legend, 2 neutral D-pad ellipse
};
inline CalculatorKeyCapture CalculatorKeyCaptureSpec(int model, int id, int cx, int cy,
    int keyWidth, int keyHeight, int bitmapWidth, int bitmapHeight)
{
    CalculatorKeyCapture s{};
    if (model == 2) {
        if (id <= 5) s.bounds = {cx-44,503,cx+44,590};
        else if (id <= 9) s.bounds = {cx-43,600,cx+43,668};
        else if (id <= 13) s.bounds = {430,593,595,753};
        else if (id <= 17) s.bounds = {cx-43,670,cx+43,737};
        else if (id <= 23) s.bounds = {cx-42,750,cx+42,821};
        else if (id <= 29) s.bounds = {cx-42,821,cx+42,893};
        else if (id <= 34) s.bounds = {cx-50,907,cx+50,996};
        else if (id <= 39) s.bounds = {cx-50,999,cx+50,1082};
        else if (id <= 44) s.bounds = {cx-50,1085,cx+50,1169};
        else s.bounds = {cx-50,1172,cx+50,1256};
    } else if (model == 0) {
        if (id == 0) s.bounds = {90,450,157,537};
        else if (id == 1) s.bounds = {158,450,226,537};
        else if (id >= 2 && id <= 5) {s.bounds={221,463,424,607};s.mask=2;}
        else if (id == 6) {s.bounds={405,449,498,537};s.body={418,473,483,537};s.legend={405,449,498,473};s.mask=1;}
        else if (id == 7) {s.bounds={484,449,556,537};s.body={484,473,556,537};s.legend={505,449,541,473};s.mask=1;}
        else if (id == 8) s.bounds={89,550,165,618};
        else if (id == 9) {s.bounds={167,550,243,618};s.body={169,574,242,618};s.legend={169,550,241,574};s.mask=1;}
        else if (id == 10) {s.bounds={400,545,478,618};s.body={404,573,477,618};s.legend={417,545,475,574};s.mask=1;}
        else if (id == 11) s.bounds={479,545,555,618};
        else if (id <= 17) s.bounds={cx-38,619,cx+38,684};
        else if (id <= 23) s.bounds={cx-38,684,cx+38,747};
        else if (id <= 29) s.bounds={cx-38,748,cx+38,815};
        else if (id <= 34) s.bounds={cx-45,819,cx+45,901};
        else if (id <= 39) s.bounds={cx-45,903,cx+45,981};
        else if (id <= 44) s.bounds={cx-45,983,cx+45,1062};
        else s.bounds={cx-45,1063,cx+45,1142};
    } else if (model == 3) {
        if (id == 0) s.bounds={72,714,192,850};
        else if (id == 1) s.bounds={218,714,338,852};
        else if (id == 2 || id == 6 || id == 7 || id == 8 || id == 10) s.bounds={366,738,776,1118};
        else if (id == 3 || id == 9) s.bounds={800,740,924,998};
        else if (id == 4) s.bounds={64,856,200,996};
        else if (id == 5) s.bounds={218,882,338,996};
        else if (id <= 15) s.bounds={cx-71,1002,cx+71,1142};
        else if (id <= 21) s.bounds={cx-74,1143,cx+68,1272};
        else if (id <= 27) s.bounds={(id==22 ? 50 : cx-74),1274,cx+66,1400};
        else if (id <= 32) s.bounds={cx-84,1404,cx+78,1558};
        else if (id <= 37) s.bounds={cx-84,1559,cx+78,1707};
        else if (id <= 42) s.bounds={cx-84,1708,cx+78,1857};
        else s.bounds={cx-84,1858,cx+78,2008};
    } else {
        if (id == 0) {s.bounds={36,274,96,338};s.body={39,292,94,338};s.legend={47,274,83,292};s.mask=1;}
        else if (id == 1) {s.bounds={310,274,371,338};s.body={311,291,370,338};s.legend={330,274,353,291};s.mask=1;}
        else if (id == 2 || id == 5 || id == 6 || id == 7) s.bounds={148,289,256,387};
        else if (id == 3) {s.bounds={90,282,149,347};s.body={93,300,148,347};s.legend={92,282,146,300};s.mask=1;}
        else if (id == 4) {s.bounds={251,283,320,346};s.body={258,300,319,346};s.legend={251,283,317,300};s.mask=1;}
        else if (id <= 11) s.bounds={cx-27,347,cx+27,398};
        else if (id <= 17) s.bounds={cx-27,398,cx+27,444};
        else if (id <= 23) s.bounds={cx-27,444,cx+27,487};
        else if (id <= 29) s.bounds={cx-27,487,cx+27,536};
        else if (id <= 34) s.bounds={cx-32,536,cx+32,594};
        else if (id <= 39) s.bounds={cx-32,594,cx+32,650};
        else if (id <= 44) s.bounds={cx-32,650,cx+32,707};
        else s.bounds={cx-32,707,cx+32,767};
    }
    if (s.bounds.left<0) s.bounds.left=0;
    if (s.bounds.top<0) s.bounds.top=0;
    if (s.bounds.right>bitmapWidth) s.bounds.right=bitmapWidth;
    if (s.bounds.bottom>bitmapHeight) s.bounds.bottom=bitmapHeight;
    (void)cy; (void)keyWidth; (void)keyHeight;
    return s;
}
inline RECT CalculatorKeyCaptureRect(int model,int id,int cx,int cy,int w,int h,int bw,int bh)
{ return CalculatorKeyCaptureSpec(model,id,cx,cy,w,h,bw,bh).bounds; }
inline HBITMAP CreateCalculatorKeySnapshot(HDC reference,HBITMAP skin,int model,
    const CalculatorKeyCapture& spec)
{
    const RECT r=spec.bounds;
    const int width=r.right-r.left,height=r.bottom-r.top;
    if(width<=0||height<=0||!skin)return nullptr;
    BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth=width;info.bmiHeader.biHeight=-height;
    info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    void* raw=nullptr;HBITMAP bitmap=CreateDIBSection(reference,&info,DIB_RGB_COLORS,&raw,nullptr,0);
    if(!bitmap)return nullptr;
    HDC src=CreateCompatibleDC(reference),dst=CreateCompatibleDC(reference);
    HGDIOBJ oldSrc=SelectObject(src,skin),oldDst=SelectObject(dst,bitmap);
    const COLORREF bg=GetPixel(src,r.left,r.top);
    const unsigned background=0xFF000000u|((bg&255u)<<16)|(bg&0xFF00u)|((bg>>16)&255u);
    BitBlt(dst,0,0,width,height,src,r.left,r.top,SRCCOPY);
    GdiFlush();
    auto* pixels=static_cast<unsigned*>(raw);
    std::vector<unsigned char> bodyMask;
    if(model==1&&spec.mask==1){
        bodyMask.resize(width*height);
        std::vector<int> frontier;
        const int seedX=(spec.body.left+spec.body.right)/2-r.left;
        const int seedY=(spec.body.top+spec.body.bottom)/2-r.top;
        if(seedX>=0&&seedX<width&&seedY>=0&&seedY<height){
            frontier.push_back(seedY*width+seedX);bodyMask[frontier[0]]=1;
            for(size_t next=0;next<frontier.size();++next){
                const int index=frontier[next],px=index%width,py=index/width;
                for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx){
                    const int xx=px+dx,yy=py+dy,gx=r.left+xx,gy=r.top+yy;
                    if(xx<0||xx>=width||yy<0||yy>=height||gx<spec.body.left||gx>=spec.body.right||gy<spec.body.top||gy>=spec.body.bottom)continue;
                    const int ni=yy*width+xx;const unsigned color=pixels[ni];
                    if(!bodyMask[ni]&&(((color>>16)&255)<185||((color>>8)&255)<185||(color&255)<185)){
                        bodyMask[ni]=1;frontier.push_back(ni);
                    }
                }
            }
            auto expanded=bodyMask;
            for(int index:frontier)for(int dy=-2;dy<=2;++dy)for(int dx=-2;dx<=2;++dx){
                int xx=index%width+dx,yy=index/width+dy,gx=r.left+xx,gy=r.top+yy;
                if(xx>=0&&xx<width&&yy>=0&&yy<height&&gx>=spec.body.left&&gx<spec.body.right&&gy>=spec.body.top&&gy<spec.body.bottom)expanded[yy*width+xx]=1;
            }
            bodyMask.swap(expanded);
        }
    }
    for(int y=0;y<height;++y)for(int x=0;x<width;++x){
        const int gx=r.left+x,gy=r.top+y;unsigned& p=pixels[y*width+x];p|=0xFF000000u;
        bool keep=true;
        if(spec.mask==1){
            const bool body=bodyMask.empty()?(gx>=spec.body.left&&gx<spec.body.right&&gy>=spec.body.top&&gy<spec.body.bottom):bodyMask[y*width+x]!=0;
            bool legend=gx>=spec.legend.left&&gx<spec.legend.right&&gy>=spec.legend.top&&gy<spec.legend.bottom;
            // White-model legends are bright on a uniform dark skin; exclude the neighbouring D-pad.
            if(model==0&&legend)legend=((p>>16)&255u)>((background>>16)&255u)+2u||((p>>8)&255u)>((background>>8)&255u)+2u||(p&255u)>(background&255u)+2u;
            keep=body||legend;
        }else if(spec.mask==2){
            double dx=(gx-322.0)/100.5,dy=(gy-535.0)/72.0;
            const unsigned red=(p>>16)&255u,green=(p>>8)&255u,blue=p&255u;
            const unsigned hi=red>green?(red>blue?red:blue):(green>blue?green:blue);
            const unsigned lo=red<green?(red<blue?red:blue):(green<blue?green:blue);
            keep=dx*dx+dy*dy<=1.0&&hi-lo<12;
        }
        if(!keep)p=background;
    }
    SelectObject(src,oldSrc);SelectObject(dst,oldDst);DeleteDC(src);DeleteDC(dst);
    return bitmap;
}
