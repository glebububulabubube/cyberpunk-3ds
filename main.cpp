#include <3ds.h>
#include <citro2d.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <time.h>

#define SCR_W 400
#define SCR_H 240
#define BOT_W 320
#define BOT_H 240

#define MAP_W 24
#define MAP_H 24

static u32 C(float r,float g,float b){ return C2D_Color32((int)(r*255),(int)(g*255),(int)(b*255),255); }

enum GS{ST_MENU,ST_PLAY,ST_OVER};
static GS state=ST_MENU;
static int menuSel=0;
static float tclock;
static bool wantExit=false;

static C3D_RenderTarget *top,*bottom;
static C2D_Font fnt;
static C2D_TextBuf tbuf;
static u32 black;

static float px=3.5f, py=3.5f, yaw=0.0f, pitch=0.0f;
static float hp=100.0f;
static int credits=1500;
static int curDistrict=0;

static int city[MAP_W][MAP_H];
static int districtMap[MAP_W][MAP_H];
static const char* districtNames[6]={"WATSON","CITY CENTER","HEYWOOD","SANTO DOMINGO","PACIFICA","BADLANDS"};

static void text(float x,float y,float s,const char* t,float r,float g,float b){
    C2D_Text t1; C2D_TextFontParse(&t1,fnt,tbuf,t); C2D_TextOptimize(&t1);
    C2D_DrawText(&t1,C2D_AlignLeft,x,y,0.3f,s,s,C(r,g,b));
}

static void buildCity(void){
    for(int y=0;y<MAP_H;y++)
        for(int x=0;x<MAP_W;x++){
            if(x%4==0 || y%4==0){ city[x][y]=0; }
            else { city[x][y]=1+(rand()%4); }
            districtMap[x][y]=0;
            if(y<8)districtMap[x][y]=0;
            else if(y>=8&&y<12)districtMap[x][y]=1;
            else if(y>=12&&y<16)districtMap[x][y]=2;
            else if(y>=16&&y<20)districtMap[x][y]=3;
            else if(y>=20)districtMap[x][y]=4;
            if(x>=20)districtMap[x][y]=5;
        }
}

static int wallHit(float wx,float wy){
    int ix=(int)wx, iy=(int)wy;
    if(ix<0||ix>=MAP_W||iy<0||iy>=MAP_H)return 1;
    return city[ix][iy]>0;
}

static void drawFPS(void){
    C2D_TargetClear(top,black);
    C2D_SceneBegin(top);
    C2D_DrawRectSolid(0,0,0,SCR_W,SCR_H,C(0.05f,0.03f,0.08f));
    for(int i=0;i<40;i++){ C2D_DrawCircleSolid((i*53)%SCR_W,(i*37)%SCR_H,0.1f,1,C(0.3f,0.3f,0.6f)); }
    C2D_DrawRectSolid(0,SCR_H*0.55f,0.1f,SCR_W,2,C(0.9f,0.2f,0.4f));

    int rays=80;
    float rayStep=1.0f/rays;
    float rayW=(float)SCR_W/rays;
    for(int i=0;i<rays;i++){
        float off=-0.5f+rayStep*i;
        float a=yaw+off;
        float dx=cosf(a), dy=sinf(a);
        float wx=px, wy=py;
        float dist=0;
        float step=0.05f;
        int found=0;
        float mx=0,my=0;
        for(int t=0;t<400;t++){
            wx+=dx*step; wy+=dy*step;
            dist+=step;
            mx=(int)wx; my=(int)wy;
            if(wallHit(mx,my)){ found=1; break; }
        }
        if(!found)continue;
        float corr=cosf(off);
        float hh=(SCR_H*0.45f)/(dist*corr)*15.0f;
        if(hh>SCR_H)hh=SCR_H;
        int bheight=city[(int)mx][(int)my];
        if(bheight==0)bheight=1;
        float hh2=hh*(0.5f+bheight*0.25f);
        float x0=i*rayW;
        int d=(mx>=0&&mx<MAP_W&&my>=0&&my<MAP_H)?districtMap[(int)mx][(int)my]:0;
        float nr=(d==0)?0.2f:(d==1)?0.0f:(d==2)?0.0f:(d==3)?0.9f:(d==4)?0.3f:0.0f;
        float ng=(d==0)?0.6f:(d==1)?1.0f:(d==2)?0.8f:(d==3)?0.4f:(d==4)?0.2f:0.2f;
        float nb=(d==0)?0.9f:(d==1)?1.0f:(d==2)?0.0f:(d==3)?0.1f:(d==4)?0.6f:0.1f;
        float dark=(dist>12)?0.5f:1.0f;
        u32 bc=C2D_Color32((int)(nr*255*dark),(int)(ng*255*dark),(int)(nb*255*dark),255);
        float topY=SCR_H*0.55f-hh2;
        C2D_DrawRectSolid(x0,topY,0.5f,rayW+1,hh2,bc);
        C2D_DrawRectSolid(x0,topY,0.51f,rayW+1,2,C(0.9f,0.9f,0.5f));
    }
    C2D_DrawCircleSolid(SCR_W/2,SCR_H*0.55f-10,0.6f,3,C(1,0,0));
    text(SCR_W/2-30,SCR_H-16,0.5f,"PISTOL",0.9f,0.9f,0.3f);
}

static void drawHUD(void){
    C2D_TargetClear(bottom,black);
    C2D_SceneBegin(bottom);
    C2D_DrawRectSolid(0,0,0,BOT_W,BOT_H,C(0.03f,0.03f,0.08f));
    text(10,8,0.5f,"HP",1,0.3f,0.3f);
    C2D_DrawRectSolid(50,10,0.3f,200,14,C(0.2f,0.2f,0.2f));
    float hpf=hp/100.0f; if(hpf<0)hpf=0;
    C2D_DrawRectSolid(50,10,0.31f,200*hpf,14,hpf>0.3?C(0.2f,0.9f,0.2f):C(0.9f,0.2f,0.2f));
    char cr[32];snprintf(cr,32,"CREDITS: $%d",credits);text(10,32,0.5f,cr,0.9f,0.9f,0.2f);
    char ds[40];snprintf(ds,40,"DISTRICT: %s",districtNames[curDistrict]);text(10,54,0.5f,ds,0.2f,0.9f,0.9f);
    text(10,120,0.4f,"STICK: look",0.6f,0.7f,0.9f);
    text(10,136,0.4f,"DPAD: walk",0.6f,0.7f,0.9f);
    text(10,152,0.4f,"A: use  B: map",0.6f,0.7f,0.9f);
}

static void startGame(void){
    px=3.5f;py=3.5f;yaw=0;pitch=0;hp=100;credits=1500;
    buildCity();
    state=ST_PLAY;
}

static void update(float dt){
    tclock+=dt;
    u32 k=hidKeysHeld();
    static u32 lastK=0;
    u32 down=hidKeysDown();
    u32 fresh=down&~lastK;lastK=down;
    if(state==ST_PLAY){
        circlePosition c;
        hidCircleRead(&c);
        yaw += c.dx*0.0012f;
        pitch += c.dy*0.0012f;
        if(pitch>0.8f)pitch=0.8f; if(pitch<-0.8f)pitch=-0.8f;
        float step=2.2f*dt;
        float npx=px, npy=py;
        if(k&KEY_DUP){ npx+=cosf(yaw)*step; npy+=sinf(yaw)*step; }
        if(k&KEY_DDOWN){ npx-=cosf(yaw)*step; npy-=sinf(yaw)*step; }
        if(k&KEY_DLEFT){ npx+=cosf(yaw-1.57f)*step; npy+=sinf(yaw-1.57f)*step; }
        if(k&KEY_DRIGHT){ npx-=cosf(yaw-1.57f)*step; npy-=sinf(yaw-1.57f)*step; }
        if(npx>=0.5f&&npx<=MAP_W-0.5f&&npy>=0.5f&&npy<=MAP_H-0.5f && !wallHit(npx,npy)){ px=npx; py=npy; }
        int ix=(int)px, iy=(int)py;
        curDistrict=districtMap[ix][iy];
        if(fresh&KEY_START){ wantExit=true; }
    } else if(state==ST_MENU){
        if(fresh&(KEY_DUP|KEY_CPAD_UP))menuSel=(menuSel+1)%2;
        if(fresh&(KEY_DDOWN|KEY_CPAD_DOWN))menuSel=(menuSel+1)%2;
        if(fresh&KEY_A){ if(menuSel==0)startGame(); else wantExit=true; }
    } else if(state==ST_OVER){
        if(fresh&KEY_A)startGame();
        if(fresh&KEY_B)state=ST_MENU;
    }
}

static void drawMenu(void){
    C2D_TargetClear(top,C(0.05f,0.03f,0.08f));
    C2D_SceneBegin(top);
    C2D_DrawRectSolid(0,0,0,SCR_W,SCR_H,C(0.05f,0.03f,0.08f));
    text(SCR_W/2-110,SCR_H/2-40,0.9f,"CYBERPUNK 3DS",0.2f,0.95f,1.0f);
    text(SCR_W/2-80,SCR_H/2-10,0.5f,"night city demo",0.9f,0.2f,0.6f);
    C2D_TargetClear(bottom,C(0.05f,0.03f,0.08f));
    C2D_SceneBegin(bottom);
    C2D_DrawRectSolid(0,0,0,BOT_W,BOT_H,C(0.05f,0.03f,0.08f));
    const char* it[2]={"JUMP IN","EXIT"};
    for(int i=0;i<2;i++){int y=70+i*70;u32 c=(i==menuSel)?C(0.1f,0.4f,0.4f):C(0.1f,0.1f,0.2f);C2D_DrawRectSolid(60,y,0.3f,200,50,c);text(120,y+14,0.6f,it[i],0.9f,0.9f,0.9f);}
}

static void drawOver(void){
    C2D_TargetClear(top,C(0.4f,0.05f,0.05f));
    C2D_SceneBegin(top);C2D_DrawRectSolid(0,0,0,SCR_W,SCR_H,C(0.4f,0.05f,0.05f));
    text(SCR_W/2-90,SCR_H/2-20,0.7f,"FLATLINED",1,0.2f,0.2f);
    C2D_TargetClear(bottom,black);C2D_SceneBegin(bottom);C2D_DrawRectSolid(0,0,0,BOT_W,BOT_H,black);
    text(70,80,0.6f,"A: retry",1,1,1);text(70,140,0.6f,"B: menu",1,1,1);
}

int main(void){
    srand((unsigned)time(NULL));gfxInitDefault();C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);C2D_Init(C2D_DEFAULT_MAX_OBJECTS);C2D_Prepare();
    top=C2D_CreateScreenTarget(GFX_TOP,GFX_LEFT);bottom=C2D_CreateScreenTarget(GFX_BOTTOM,GFX_LEFT);
    fnt=C2D_FontLoadSystem(CFG_REGION_USA);tbuf=C2D_TextBufNew(4096);
    black=C(0,0,0);
    buildCity();
    while(aptMainLoop()){
        hidScanInput();if(hidKeysDown()&KEY_START)break;if(wantExit)break;
        update(1.0f/60.0f);C2D_TextBufClear(tbuf);C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        if(state==ST_PLAY){drawFPS();drawHUD();}
        else if(state==ST_OVER){drawOver();}
        else {drawMenu();}
        C3D_FrameEnd(0);
    }
    C2D_TextBufDelete(tbuf);C2D_FontFree(fnt);C2D_Fini();C3D_Fini();gfxExit();return 0;
}
