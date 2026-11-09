#include <3ds.h>
#include <citro3d.h>
#include <citro2d.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <time.h>

#define SW 400
#define SH 240
#define BW 320
#define BH 240
#define MW 24
#define MH 24
#define NC 10
#define ND 4
#define NSH 3
#define MAXV (36*(MW*MH+256))
#define PF 3.14159265f

static u32 black;
static u32 C(float r,float g,float b){return C2D_Color32((int)(r*255),(int)(g*255),(int)(b*255),255);}
static u32 CA(float r,float g,float b,float a){return C2D_Color32((int)(r*255),(int)(g*255),(int)(b*255),(int)(a*255));}

enum GS{ST_MENU,ST_PLAY,ST_OVER};
static GS state=ST_MENU;
static int menuSel=0;
static bool wantExit=false;

static C3D_RenderTarget *top,*bottom;
static C3D_AttrInfo ai;
static C2D_Font fnt;
static C2D_TextBuf tbuf;

typedef struct{float x,y,z;u32 c;}Vtx;
static Vtx __attribute__((aligned(16))) verts[MAXV];
static int vcount=0;

static const u8 dcol[6][3]={{88,214,253},{10,235,255},{0,255,200},{255,140,80},{255,60,255},{110,90,40}};

static int city[MW][MH], dm[MW][MH];
static float camX=0.5f,camZ=10.5f,camY=1.7f;
static float yaw=0,pitch=0,tclock=0,fps=60;
static int credits=1500,colShards=0;
static int curDistrict=0;

typedef struct{float x,z,sp;int dir;u8 r,g,b;}Car;
static Car car[NC];
static float dp[ND];
static float shX[NSH],shZ[NSH];
static int shAlive[NSH];
static float rx[90],ry[90];

static const char* dNames[6]={"WATSON","CITY CENTER","HEYWOOD","SANTO DOMINGO","PACIFICA","BADLANDS"};

static void text2d(float x,float y,float s,const char* t,float r,float g,float b){
    C2D_Text t1; C2D_TextFontParse(&t1,fnt,tbuf,t); C2D_TextOptimize(&t1);
    C2D_DrawText(&t1,C2D_AlignLeft,x,y,0.3f,s,s,C(r,g,b));
}

static void pushCube(float cx,float cy,float cz,float w,float h,float d,float r,float g,float b,float sh){
    int v=vcount;
    float x0=cx-w/2.0f,x1=cx+w/2.0f,z0=cz-d/2.0f,z1=cz+d/2.0f,y0=cy,y1=cy+h;
    float fm[5]={0.5f,0.65f,1.0f,0.42f,0.58f};
    float fs[5][4][3]={
        {{x0,y0,z1},{x1,y0,z1},{x1,y1,z1},{x0,y1,z1}},
        {{x1,y0,z0},{x0,y0,z0},{x0,y1,z0},{x1,y1,z0}},
        {{x0,y1,z0},{x1,y1,z0},{x1,y1,z1},{x0,y1,z1}},
        {{x0,y0,z1},{x0,y0,z0},{x0,y1,z0},{x0,y1,z1}},
        {{x1,y0,z0},{x1,y0,z1},{x1,y1,z1},{x1,y1,z0}},
    };
    for(int f=0;f<5;f++){
        unsigned cc=C2D_Color32((int)(r*255.0f*fm[f]*sh),(int)(g*255.0f*fm[f]*sh),(int)(b*255.0f*fm[f]*sh),255);
        int pi[6]={0,1,2,0,2,3};
        for(int q=0;q<6;q++){
            verts[v].x=fs[f][pi[q]][0]; verts[v].y=fs[f][pi[q]][1]; verts[v].z=fs[f][pi[q]][2]; verts[v].c=cc; v++;
        }
    }
    vcount=v;
}

static void buildCity(void){
    for(int y=0;y<MH;y++)for(int x=0;x<MW;x++){
        if(x%4==0||y%4==0){ city[x][y]=0; }
        else { city[x][y]=1+(rand()%4); }
        dm[x][y]=0;
        if(y<8)dm[x][y]=0;
        else if(y>=8&&y<12)dm[x][y]=1;
        else if(y>=12&&y<16)dm[x][y]=2;
        else if(y>=16&&y<20)dm[x][y]=3;
        else if(y>=20)dm[x][y]=4;
        if(x>=20)dm[x][y]=5;
    }
}

static int wallAt(int ix,int iy){
    if(ix<0||ix>=MW||iy<0||iy>=MH)return 1;
    return city[ix][iy]>0;
}

static void spawnCar(int i){
    int line=rand()%6;
    if(rand()%2){
        car[i].x=(float)(rand()%MW)+0.5f;
        car[i].z=line*4+0.5f;
        car[i].dir=rand()%2;
    } else {
        car[i].x=line*4+0.5f;
        car[i].z=(float)(rand()%MH)+0.5f;
        car[i].dir=2+(rand()%2);
    }
    car[i].sp=1.6f+((rand()%20)/10.0f);
    car[i].r=50+(rand()%206);
    car[i].g=50+(rand()%206);
    car[i].b=50+(rand()%206);
}

static void updateCars(float dt){
    for(int i=0;i<NC;i++){
        float sp=car[i].sp*dt;
        if(car[i].dir==0)car[i].x+=sp;
        else if(car[i].dir==1)car[i].x-=sp;
        else if(car[i].dir==2)car[i].z+=sp;
        else car[i].z-=sp;
        int ix=(int)car[i].x, iz=(int)car[i].z;
        if(ix<1||ix>=MW-1||iz<1||iz>=MH-1){ spawnCar(i); continue; }
        if(ix%4==0&&iz%4==0&&(rand()%90)==0){
            int nd; do{ nd=rand()%4; }while((nd&1)==(car[i].dir&1));
            car[i].x=ix+0.5f; car[i].z=iz+0.5f;
            car[i].dir=nd;
        }
    }
}

static void spawnShard(int i){
    for(int t=0;t<20;t++){
        float x,z;
        if(rand()%2){ x=(rand()%6)*4+0.5f; z=(float)(rand()%MH)+0.5f; }
        else { x=(float)(rand()%MW)+0.5f; z=(rand()%6)*4+0.5f; }
        float dx=x-camX,dz=z-camZ;
        if(dx*dx+dz*dz>9.0f){ shX[i]=x; shZ[i]=z; shAlive[i]=1; return; }
    }
    shX[i]=8.5f; shZ[i]=8.5f; shAlive[i]=1;
}

static void render3d(void){
    C2D_SceneBegin(top);
    C2D_TargetClear(top,C(0.02f,0.02f,0.07f));

    C3D_Mtx proj,view,mvp;
    Mtx_Persp(&proj,60.0f*PF/180.0f,(float)SW/(float)SH,0.1f,100.0f);
    C3D_FVec eye={camX,camY,camZ};
    C3D_FVec tgt={camX+cosf(yaw)*cosf(pitch),camY+sinf(pitch),camZ+sinf(yaw)*cosf(pitch)};
    C3D_FVec up={0,1,0};
    Mtx_LookAt(&view,&eye,&tgt,&up);
    Mtx_Multiply(&mvp,&proj,&view);
    C3D_FVPMatrix(&mvp);
    C3D_SetAttrInfo(&ai);
    C3D_SetTexEnv(0,C3D_TexEnv(GPU_REPLACE,GPU_REPLACE,GPU_REPLACE,GPU_REPLACE,0xFFFFFFFF));
    C3D_SetTexture(0,NULL);

    vcount=0;
    for(int x=0;x<MW;x++)for(int z=0;z<MH;z++){
        float b=((x+z)%2==0)?0.16f:0.07f;
        float bc=b*1.4f; if(bc>1.0f)bc=1.0f;
        pushCube(x+0.5f,0.0f,z+0.5f,1.0f,0.02f,1.0f,b,b*0.7f,bc,1.0f);
    }

    int fx=cosf(yaw)>=0, fz=sinf(yaw)>=0;
    int xs=fx?MW-1:0, zs=fz?MH-1:0, xd=fx?-1:1, zd=fz?-1:1;
    for(int ii=0;ii<MW;ii++){
        int x=xs+ii*xd;
        for(int jj=0;jj<MH;jj++){
            int z=zs+jj*zd;
            float dx=camX-(x+0.5f),dz=camZ-(z+0.5f);
            float dist=sqrtf(dx*dx+dz*dz)+0.01f;
            float sh=(dist<3)?1.0f:(dist>26)?0.12f:1.0f-(dist-3.0f)*0.88f/23.0f;
            int h=city[x][z];
            if(h>0){
                int d=dm[x][z];
                float bh=0.7f+h*1.15f;
                pushCube(x+0.5f,0.0f,z+0.5f,0.9f,bh,0.9f,dcol[d][0]/255.0f,dcol[d][1]/255.0f,dcol[d][2]/255.0f,sh);
            }
            for(int ci=0;ci<NC;ci++){
                if((int)car[ci].x==x&&(int)car[ci].z==z){
                    pushCube(car[ci].x,0.0f,car[ci].z,0.9f,0.45f,0.55f,car[ci].r/255.0f,car[ci].g/255.0f,car[ci].b/255.0f,sh);
                    float fx2=car[ci].dir==0?0.6f:car[ci].dir==1?-0.6f:0.0f;
                    float fz2=car[ci].dir==2?0.6f:car[ci].dir==3?-0.6f:0.0f;
                    pushCube(car[ci].x+fx2,0.3f,car[ci].z+fz2,0.16f,0.16f,0.16f,1.0f,0.95f,0.5f,sh);
                }
            }
            for(int si=0;si<NSH;si++){
                if(shAlive[si]&&(int)shX[si]==x&&(int)shZ[si]==z){
                    float bob=0.9f+sinf(tclock*2.5f+si)*0.15f;
                    float pulse=0.7f+0.3f*sinf(tclock*6.0f+si);
                    pushCube(shX[si],bob,shZ[si],0.3f,0.3f,0.3f,0.1f,1.0f,1.0f,sh*pulse);
                }
            }
        }
    }
    for(int i=0;i<ND;i++){
        float r=4.0f+i*2.0f;
        float x=12+cosf(dp[i])*r, z=12+sinf(dp[i])*r, y=7+sinf(dp[i]*2.5f)*1.3f;
        if(((int)(tclock*8.0f)+i)%2==0)
            pushCube(x,y,z,0.5f,0.5f,0.5f,1.0f,0.25f,0.2f,0.9f);
    }

    C3D_SendVertices(GPU_TRIANGLES,vcount,verts);

    for(int i=0;i<90;i++)
        C2D_DrawRectSolid(rx[i],ry[i],0.5f,1,6,CA(0.5f,0.7f,1.0f,0.30f));
}

static void drawHUD(void){
    C2D_SceneBegin(bottom);
    C2D_TargetClear(bottom,black);
    C2D_DrawRectSolid(0,0,0,BW,BH,C(0.03f,0.03f,0.08f));
    text2d(180,6,0.4f,"NIGHT CITY 2077",0.2f,0.9f,1.0f);
    char cr[32];snprintf(cr,32,"$%d",credits);text2d(10,8,0.5f,cr,1.0f,0.9f,0.2f);
    char sd[32];snprintf(sd,32,"SHARDS %d/3",colShards);text2d(10,28,0.4f,sd,0.1f,1.0f,1.0f);
    char ds[40];snprintf(ds,40,"%s",dNames[curDistrict]);text2d(10,46,0.5f,ds,0.3f,0.6f,1.0f);
    char ff[16];snprintf(ff,16,"%d FPS",(int)fps);text2d(BW-70,6,0.4f,ff,0.4f,1.0f,0.3f);

    int mx0=8,my0=76,sc=4;
    for(int y=0;y<MH;y++)for(int x=0;x<MW;x++){
        u32 cc;
        if(city[x][y]==0)cc=C(0.08f,0.08f,0.12f);
        else{ int d=dm[x][y]; cc=C(dcol[d][0]/255.0f*0.4f,dcol[d][1]/255.0f*0.4f,dcol[d][2]/255.0f*0.4f); }
        C2D_DrawRectSolid(mx0+x*sc,my0+y*sc,0.3f,sc,sc,cc);
    }
    for(int i=0;i<NSH;i++)
        if(shAlive[i])C2D_DrawRectSolid(mx0+(int)shX[i]*sc,my0+(int)shZ[i]*sc,0.4f,sc,sc,C(0,1,1));
    for(int i=0;i<NC;i++)
        C2D_DrawRectSolid(mx0+(int)car[i].x*sc,my0+(int)car[i].z*sc,0.35f,2,2,C(car[i].r/255.0f,car[i].g/255.0f,car[i].b/255.0f));
    float pxc=mx0+camX*sc,pzc=my0+camZ*sc;
    C2D_DrawRectSolid(pxc-1,pzc-1,0.5f,3,3,C(0.2f,1.0f,0.3f));
    C2D_DrawLine(pxc,pzc,pxc+cosf(yaw)*14,pzc+sinf(yaw)*14,1,C(1,1,1));

    text2d(10,BH-16,0.35f,"TOUCH look | X/B/Y/A move | START exit",0.5f,0.6f,0.8f);
    text2d(120,36,0.3f,"collect cyan shards",0.4f,0.8f,0.8f);
}

static void startGame(void){
    camX=0.5f; camZ=10.5f; yaw=0; pitch=0;
    credits=1500; colShards=0;
    for(int i=0;i<NC;i++)spawnCar(i);
    for(int i=0;i<NSH;i++)spawnShard(i);
    state=ST_PLAY;
}

static void update(float dt){
    tclock+=dt;
    u32 k=hidKeysHeld();
    u32 down=hidKeysDown();
    static int lastTx=-1,lastTy=-1;
    if(state==ST_PLAY){
        if(k&KEY_TOUCH){
            touchPosition t; hidTouchRead(&t);
            if(lastTx!=-1){
                yaw-=(t.px-lastTx)*0.004f;
                pitch+=(t.py-lastTy)*0.004f;
                if(pitch>0.8f)pitch=0.8f;
                if(pitch<-0.8f)pitch=-0.8f;
            }
            lastTx=t.px; lastTy=t.py;
        } else { lastTx=-1; lastTy=-1; }
        float step=2.2f*dt, nx=camX, nz=camZ;
        if(k&KEY_X){ nx+=cosf(yaw)*step; nz+=sinf(yaw)*step; }
        if(k&KEY_B){ nx-=cosf(yaw)*step; nz-=sinf(yaw)*step; }
        if(k&KEY_Y){ nx+=cosf(yaw-PF/2.0f)*step; nz+=sinf(yaw-PF/2.0f)*step; }
        if(k&KEY_A){ nx-=cosf(yaw-PF/2.0f)*step; nz-=sinf(yaw-PF/2.0f)*step; }
        if(nx>=0.3f&&nx<=MW-0.3f&&nz>=0.3f&&nz<=MH-0.3f&&!wallAt((int)nx,(int)nz)){ camX=nx; camZ=nz; }
        curDistrict=dm[(int)camX][(int)camZ];
        updateCars(dt);
        for(int i=0;i<90;i++){
            ry[i]+=dt*220.0f;
            if(ry[i]>SH){ ry[i]-=SH; rx[i]=(float)(rand()%SW); }
        }
        for(int i=0;i<ND;i++)dp[i]+=dt*(0.3f+i*0.05f);
        for(int i=0;i<NSH;i++){
            if(!shAlive[i])continue;
            float dx=camX-shX[i],dz=camZ-shZ[i];
            if(dx*dx+dz*dz<0.7f){ shAlive[i]=0; credits+=100; colShards++; spawnShard(i); }
        }
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
    float pulse=0.6f+0.4f*sinf(tclock*3.0f);
    C2D_SceneBegin(top); C2D_TargetClear(top,C(0.02f,0.02f,0.07f));
    C2D_DrawRectSolid(0,0,0,SW,SH,C(0.02f,0.02f,0.07f));
    for(int i=0;i<60;i++)
        C2D_DrawCircleSolid((i*97)%SW,(i*53)%SH,0.1f,1,C(0.4f,0.5f,0.9f));
    text2d(SW/2-120,SH/2-46,1.0f,"CYBERPUNK 3DS",0.2f,0.4f+pulse,1.0f);
    text2d(SW/2-70,SH/2-14,0.5f,"night city 2077 demo",1.0f,0.2f,0.6f);
    text2d(SW/2-120,SH/2+28,0.4f,"TOUCH: look   X/B/Y/A: move",0.5f,0.7f,1.0f);
    C2D_SceneBegin(bottom); C2D_TargetClear(bottom,black);
    C2D_DrawRectSolid(0,0,0,BW,BH,C(0.03f,0.03f,0.08f));
    const char* it[2]={"JUMP IN","EXIT"};
    for(int i=0;i<2;i++){
        int y=66+i*80;
        u32 cc=(i==menuSel)?C(0.1f,0.6f,0.7f):C(0.08f,0.08f,0.16f);
        C2D_DrawRectSolid(60,y,0.3f,200,56,cc);
        text2d(128,y+16,0.6f,it[i],0.95f,0.95f,1.0f);
        if(i==menuSel)text2d(70,y+4,0.3f,">",0.2f,1.0f,1.0f);
    }
    text2d(60,236,0.3f,"A: select",0.5f,0.6f,0.8f);
}

static void drawOver(void){
    C2D_SceneBegin(top); C2D_TargetClear(top,C(0.4f,0.05f,0.05f));
    C2D_DrawRectSolid(0,0,0,SW,SH,C(0.4f,0.05f,0.05f));
    text2d(SW/2-90,SH/2-20,0.7f,"FLATLINED",1,0.3f,0.3f);
    C2D_SceneBegin(bottom); C2D_TargetClear(bottom,black);
    C2D_DrawRectSolid(0,0,0,BW,BH,black);
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

    AttrInfo_Init(&ai);
    AttrInfo_AddLoader(&ai,0,GPU_FLOAT,3);
    AttrInfo_AddLoader(&ai,1,GPU_UNSIGNED_BYTE,4);

    buildCity();
    for(int i=0;i<90;i++){ rx[i]=(float)(rand()%SW); ry[i]=(float)(rand()%SH); }
    for(int i=0;i<ND;i++)dp[i]=i*2.0f+1.0f;
    for(int i=0;i<NSH;i++)shAlive[i]=0;

    u64 lastT=osGetTime();
    while(aptMainLoop()){
        hidScanInput();
        if(hidKeysDown()&KEY_START)break;
        if(wantExit)break;
        update(1.0f/60.0f);
        C2D_TextBufClear(tbuf);
        u64 now=osGetTime();
        int dtm=(int)(now-lastT); lastT=now;
        if(dtm>0){ float inst=1000.0f/dtm; fps=fps*0.9f+inst*0.1f; }
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        if(state==ST_PLAY){ render3d(); drawHUD(); }
        else if(state==ST_OVER){ drawOver(); }
        else { drawMenu(); }
        C3D_FrameEnd(0);
    }
    C2D_TextBufDelete(tbuf);
    C2D_FontFree(fnt);
    C2D_Fini(); C3D_Fini(); gfxExit();
    return 0;
}
