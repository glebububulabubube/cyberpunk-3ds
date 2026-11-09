#include <3ds.h>
#include <citro3d.h>
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

static u32 black;
static u32 C(float r,float g,float b){ return C2D_Color32((int)(r*255),(int)(g*255),(int)(b*255),255); }

enum GS{ST_MENU,ST_PLAY,ST_OVER};
static GS state=ST_MENU;
static int menuSel=0;
static bool wantExit=false;

static C3D_RenderTarget *top,*bottom;
static C3D_AttrInfo attrInfo;
static C2D_Font fnt;
static C2D_TextBuf tbuf;

typedef struct { float x,y,z; u32 color; } Vtx;
static Vtx __attribute__((aligned(16))) verts[6*6*2048];
static int vcount=0;

static float camX=3.5f, camZ=3.5f, camY=1.6f;
static float yaw=0.0f, pitch=0.0f;
static float hp=100.0f;
static int credits=1500;
static int curDistrict=0;

static int city[MAP_W][MAP_H];
static int districtMap[MAP_W][MAP_H];
static const char* districtNames[6]={"WATSON","CITY CENTER","HEYWOOD","SANTO DOMINGO","PACIFICA","BADLANDS"};

static void text2d(float x,float y,float s,const char* t,float r,float g,float b){
    C2D_Text t1; C2D_TextFontParse(&t1,fnt,tbuf,t); C2D_TextOptimize(&t1);
    C2D_DrawText(&t1,C2D_AlignLeft,x,y,0.3f,s,s,C(r,g,b));
}

static void pushCube(float cx,float cz,float w,float h,float d,unsigned col){
    int b=vcount;
    float x0=cx-w/2, x1=cx+w/2, z0=cz-d/2, z1=cz+d/2, y0=0, y1=h;
    float sh[6]={0.75f,0.6f,1.0f,0.5f,0.7f,0.55f};
    float fs[6][4][3]={
        {{x0,y0,z1},{x1,y0,z1},{x1,y1,z1},{x0,y1,z1}},
        {{x1,y0,z0},{x0,y0,z0},{x0,y1,z0},{x1,y1,z0}},
        {{x0,y1,z0},{x1,y1,z0},{x1,y1,z1},{x0,y1,z1}},
        {{x0,y0,z1},{x1,y0,z1},{x1,y0,z0},{x0,y0,z0}},
        {{x1,y0,z0},{x1,y0,z1},{x1,y1,z1},{x1,y1,z0}},
        {{x0,y0,z1},{x0,y0,z0},{x0,y1,z0},{x0,y1,z1}},
    };
    for(int f=0;f<6;f++){
        unsigned r=(col>>24)&0xff,g2=(col>>16)&0xff,b=(col>>8)&0xff;
        unsigned cc=0xFF000000|((unsigned)(r*sh[f])<<16)|((unsigned)(g2*sh[f])<<8)|(unsigned)(b*sh[f]);
        verts[b].x=fs[f][0][0]; verts[b].y=fs[f][0][1]; verts[b].z=fs[f][0][2]; verts[b].color=cc; b++;
        verts[b].x=fs[f][1][0]; verts[b].y=fs[f][1][1]; verts[b].z=fs[f][1][2]; verts[b].color=cc; b++;
        verts[b].x=fs[f][2][0]; verts[b].y=fs[f][2][1]; verts[b].z=fs[f][2][2]; verts[b].color=cc; b++;
        verts[b].x=fs[f][0][0]; verts[b].y=fs[f][0][1]; verts[b].z=fs[f][0][2]; verts[b].color=cc; b++;
        verts[b].x=fs[f][2][0]; verts[b].y=fs[f][2][1]; verts[b].z=fs[f][2][2]; verts[b].color=cc; b++;
        verts[b].x=fs[f][3][0]; verts[b].y=fs[f][3][1]; verts[b].z=fs[f][3][2]; verts[b].color=cc; b++;
    }
    vcount=b;
}

static void buildCity(void){
    for(int y=0;y<MAP_H;y++)for(int x=0;x<MAP_W;x++){
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

static int wallAt(int ix,int iy){
    if(ix<0||ix>=MAP_W||iy<0||iy>=MAP_H)return 1;
    return city[ix][iy]>0;
}

static void rebuildScene(void){
    vcount=0;
    for(int y=0;y<MAP_H;y++)for(int x=0;x<MAP_W;x++){
        if(city[x][y]>0){
            int d=districtMap[x][y];
            float nr=(d==0)?0.2f:(d==1)?0.0f:(d==2)?0.0f:(d==3)?0.9f:(d==4)?0.3f:0.1f;
            float ng=(d==0)?0.6f:(d==1)?1.0f:(d==2)?0.8f:(d==3)?0.4f:(d==4)?0.2f:0.3f;
            float nb=(d==0)?0.9f:(d==1)?1.0f:(d==2)?0.0f:(d==3)?0.1f:(d==4)?0.6f:0.1f;
            u32 col=C2D_Color32((int)(nr*255),(int)(ng*255),(int)(nb*255),255);
            float h=1.0f+city[x][y]*1.2f;
            pushCube(x+0.5f,y+0.5f,0.9f,h,0.9f,col);
        }
    }
    for(int gx=0;gx<MAP_W;gx++)for(int gz=0;gz<MAP_H;gz++){
        u32 gc=((gx+gz)%2==0)?C(0.15f,0.12f,0.2f):C(0.05f,0.05f,0.1f);
        pushCube(gx+0.5f,gz+0.5f,1.0f,0.1f,1.0f,gc);
    }
}

static void render3d(void){
    C2D_SceneBegin(top);
    C2D_TargetClear(top,C(0.02f,0.02f,0.05f));

    C3D_Mtx proj, view, mvp;
    Mtx_Persp(&proj, 60.0f*M_PI/180.0f, (float)SCR_W/(float)SCR_H, 0.1f, 100.0f);
    Mtx_PerspTilt(&proj);

    C3D_FVec eye={camX,camY,camZ};
    C3D_FVec tgt={camX+cosf(yaw)*cosf(pitch), camY+sinf(pitch), camZ+sinf(yaw)*cosf(pitch)};
    C3D_FVec up={0,1,0};
    Mtx_LookAt(&view,&eye,&tgt,&up);

    Mtx_Multiply(&mvp,&proj,&view);
    C3D_FVPMatrix(&mvp);

    C3D_SetAttrInfo(&attrInfo);
    C3D_SetTexEnv(0, C3D_TexEnv(GPU_REPLACE, GPU_REPLACE, GPU_REPLACE, GPU_REPLACE, 0xFFFFFFFF));
    C3D_SetTexture(0, NULL);
    C3D_SendVertices(GPU_TRIANGLES, vcount, verts);
}

static void drawHUD(void){
    C2D_SceneBegin(bottom);
    C2D_TargetClear(bottom,black);
    C2D_DrawRectSolid(0,0,0,BOT_W,BOT_H,C(0.03f,0.03f,0.08f));
    text2d(10,8,0.5f,"HP",1,0.3f,0.3f);
    C2D_DrawRectSolid(50,10,0.3f,200,14,C(0.2f,0.2f,0.2f));
    float hpf=hp/100.0f; if(hpf<0)hpf=0;
    C2D_DrawRectSolid(50,10,0.31f,200*hpf,14,hpf>0.3?C(0.2f,0.9f,0.2f):C(0.9f,0.2f,0.2f));
    char cr[32];snprintf(cr,32,"CREDITS: $%d",credits);text2d(10,32,0.5f,cr,0.9f,0.9f,0.2f);
    char ds[40];snprintf(ds,40,"DISTRICT: %s",districtNames[curDistrict]);text2d(10,54,0.5f,ds,0.2f,0.9f,0.9f);
    text2d(10,120,0.4f,"TOUCH: look",0.6f,0.7f,0.9f);
    text2d(10,136,0.4f,"X/B/Y/A: walk",0.6f,0.7f,0.9f);
}

static void startGame(void){
    camX=3.5f; camZ=3.5f; yaw=0; pitch=0; hp=100; credits=1500;
    buildCity(); rebuildScene();
    state=ST_PLAY;
}

static void update(float dt){
    u32 k=hidKeysHeld();
    u32 down=hidKeysDown();
    static int lastTx=-1, lastTy=-1;
    if(state==ST_PLAY){
        touchPosition t; hidTouchRead(&t);
        if(t.px!=-1 && t.py!=-1){
            if(lastTx!=-1 && lastTy!=-1){
                int dx=t.px-lastTx, dy=t.py-lastTy;
                yaw   -= dx*0.004f;
                pitch += dy*0.004f;
                if(pitch>0.8f)pitch=0.8f;
                if(pitch<-0.8f)pitch=-0.8f;
            }
            lastTx=t.px; lastTy=t.py;
        } else { lastTx=-1; lastTy=-1; }
        float step=2.2f*dt;
        float npx=camX, npz=camZ;
        if(k&KEY_X){ npx+=cosf(yaw)*step; npz+=sinf(yaw)*step; }
        if(k&KEY_B){ npx-=cosf(yaw)*step; npz-=sinf(yaw)*step; }
        if(k&KEY_Y){ npx+=cosf(yaw-1.57f)*step; npz+=sinf(yaw-1.57f)*step; }
        if(k&KEY_A){ npx-=cosf(yaw-1.57f)*step; npz-=sinf(yaw-1.57f)*step; }
        if(npx>=0.3f&&npx<=MAP_W-0.3f&&npz>=0.3f&&npz<=MAP_H-0.3f&&!wallAt((int)npx,(int)npz)){ camX=npx; camZ=npz; }
        curDistrict=districtMap[(int)camX][(int)camZ];
        if(down&KEY_START){ wantExit=true; }
    } else if(state==ST_MENU){
        if(down&(KEY_DUP|KEY_CPAD_UP))menuSel=(menuSel+1)%2;
        if(down&(KEY_DDOWN|KEY_CPAD_DOWN))menuSel=(menuSel+1)%2;
        if(down&KEY_A){ if(menuSel==0)startGame(); else wantExit=true; }
    } else if(state==ST_OVER){
        if(down&KEY_A)startGame();
        if(down&KEY_B)state=ST_MENU;
    }
}

static void drawMenu(void){
    C2D_SceneBegin(top); C2D_TargetClear(top,C(0.05f,0.03f,0.08f));
    C2D_DrawRectSolid(0,0,0,SCR_W,SCR_H,C(0.05f,0.03f,0.08f));
    text2d(SCR_W/2-110,SCR_H/2-40,0.9f,"CYBERPUNK 3DS",0.2f,0.95f,1.0f);
    text2d(SCR_W/2-80,SCR_H/2-10,0.5f,"night city demo",0.9f,0.2f,0.6f);
    C2D_SceneBegin(bottom); C2D_TargetClear(bottom,C(0.05f,0.03f,0.08f));
    C2D_DrawRectSolid(0,0,0,BOT_W,BOT_H,C(0.05f,0.03f,0.08f));
    const char* it[2]={"JUMP IN","EXIT"};
    for(int i=0;i<2;i++){int y=70+i*70;u32 cc=(i==menuSel)?C(0.1f,0.4f,0.4f):C(0.1f,0.1f,0.2f);
        C2D_DrawRectSolid(60,y,0.3f,200,50,cc);text2d(120,y+14,0.6f,it[i],0.9f,0.9f,0.9f);}
}

static void drawOver(void){
    C2D_SceneBegin(top); C2D_TargetClear(top,C(0.4f,0.05f,0.05f));
    C2D_DrawRectSolid(0,0,0,SCR_W,SCR_H,C(0.4f,0.05f,0.05f));
    text2d(SCR_W/2-90,SCR_H/2-20,0.7f,"FLATLINED",1,0.2f,0.2f);
    C2D_SceneBegin(bottom); C2D_TargetClear(bottom,black);
    C2D_DrawRectSolid(0,0,0,BOT_W,BOT_H,black);
    text2d(70,80,0.6f,"A: retry",1,1,1);
    text2d(70,140,0.6f,"B: menu",1,1,1);
}

int main(void){
    srand((unsigned)time(NULL));
    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();
    top=C2D_CreateScreenTarget(GFX_TOP,GFX_LEFT);
    bottom=C2D_CreateScreenTarget(GFX_BOTTOM,GFX_LEFT);
    fnt=C2D_FontLoadSystem(CFG_REGION_USA);
    tbuf=C2D_TextBufNew(4096);
    black=C(0,0,0);

    AttrInfo_Init(&attrInfo);
    AttrInfo_AddLoader(&attrInfo,0,GPU_FLOAT,3);
    AttrInfo_AddLoader(&attrInfo,1,GPU_UNSIGNED_BYTE,4);

    buildCity();
    while(aptMainLoop()){
        hidScanInput();
        if(hidKeysDown()&KEY_START)break;
        if(wantExit)break;
        update(1.0f/60.0f);
        C2D_TextBufClear(tbuf);
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        if(state==ST_PLAY){ render3d(); drawHUD(); }
        else if(state==ST_OVER){ drawOver(); }
        else { drawMenu(); }
        C3D_FrameEnd(0);
    }
    C2D_TextBufDelete(tbuf); C2D_FontFree(fnt);
    C2D_Fini(); C3D_Fini(); gfxExit();
    return 0;
}
