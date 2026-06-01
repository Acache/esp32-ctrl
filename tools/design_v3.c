/**
 * Desk-Mate App Design v3 — English only, complete, compilable
 */

#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "lvgl/lvgl.h"
#include <SDL.h>
#include "hal/hal.h"

#define CLR(r,g,b) lv_color_make(r,g,b)
#define BG CLR(0x1A,0x1A,0x2E)
#define CD CLR(0x16,0x16,0x28)
#define C2 CLR(0x20,0x20,0x38)
#define C3 CLR(0x12,0x12,0x22)
#define AQ CLR(0x4E,0xCA,0xF5)
#define RD CLR(0xE9,0x45,0x60)
#define GN CLR(0x0F,0xD1,0x6C)
#define PU CLR(0xBB,0x86,0xFC)
#define OR CLR(0xFF,0x8C,0x00)
#define WH CLR(0xEE,0xEE,0xEE)
#define GY CLR(0x88,0x88,0x88)
#define DM CLR(0x55,0x55,0x55)
#define BD CLR(0x2A,0x2A,0x3E)

static lv_obj_t *cdx(lv_obj_t *p, int x, int y, int w, int h, lv_color_t bg) {
    lv_obj_t *c=lv_obj_create(p); lv_obj_set_size(c,w,h); lv_obj_set_pos(c,x,y);
    lv_obj_set_style_bg_color(c,bg,0); lv_obj_set_style_bg_opa(c,LV_OPA_COVER,0);
    lv_obj_set_style_radius(c,12,0); lv_obj_set_style_border_width(c,1,0);
    lv_obj_set_style_border_color(c,BD,0); lv_obj_set_style_pad_all(c,0,0);
    lv_obj_set_scrollbar_mode(c,LV_SCROLLBAR_MODE_OFF); return c;
}

static lv_obj_t *lb(lv_obj_t *p, const char *t, int x, int y, lv_color_t c, const lv_font_t *f) {
    lv_obj_t *l=lv_label_create(p); lv_label_set_text(l,t); lv_obj_set_pos(l,x,y);
    lv_obj_set_style_text_color(l,c,0); if(f) lv_obj_set_style_text_font(l,f,0); return l;
}

static lv_obj_t *bt(lv_obj_t *p, int x, int y, int w, int h, lv_color_t bg, lv_color_t fg, const char *t) {
    lv_obj_t *b=lv_obj_create(p); lv_obj_set_size(b,w,h); lv_obj_set_pos(b,x,y);
    lv_obj_set_style_bg_color(b,bg,0); lv_obj_set_style_bg_opa(b,LV_OPA_COVER,0);
    lv_obj_set_style_radius(b,h/2,0); lv_obj_set_style_border_width(b,0,0);
    lv_obj_t *l=lv_label_create(b); lv_label_set_text(l,t); lv_obj_center(l);
    lv_obj_set_style_text_color(l,fg,0); lv_obj_set_style_text_font(l,&lv_font_montserrat_12,0); return b;
}

static void page0(lv_obj_t *p) {
    lb(p,"Pet Assistant",20,10,WH,&lv_font_montserrat_14);
    lv_obj_t *pc=cdx(p,20,36,440,130,CD);
    lv_obj_t *e=lv_label_create(pc); lv_label_set_text(e,"(◕‿◕)"); lv_obj_center(e);
    lv_obj_set_style_text_color(e,AQ,0); lv_obj_set_style_text_font(e,&lv_font_montserrat_48,0);
    lv_obj_t *bc=cdx(p,30,176,420,38,C2); lv_obj_set_style_border_color(bc,CLR(0x3A,0x3A,0x5E),0);
    lb(bc,"Hi, need help writing code? 😎",14,8,WH,&lv_font_montserrat_14);
    lv_obj_t *tk=cdx(p,20,228,215,85,CD); lb(tk,"🤖 Token Plan",12,6,GY,,&lv_font_montserrat_12)lv_font_montserrat_12);
    lb(tk,"1,234,567",12,26,AQ,&lv_font_montserrat_16); lb(tk,"60% used | synced 2m ago",12,54,DM,,&lv_font_montserrat_12)lv_font_montserrat_12);
    lv_obj_t *pm=cdx(p,245,228,215,85,CD); lb(pm,"🍅 Pomodoro",12,6,GY,,&lv_font_montserrat_12)lv_font_montserrat_12);
    lb(pm,"25:00",12,26,RD,&lv_font_montserrat_16); lb(pm,"Today 3 🍅 | Total 127",12,54,DM,,&lv_font_montserrat_12)lv_font_montserrat_12);
    lv_obj_t *ap=cdx(p,20,328,440,32,CLR(0x2A,0x15,0x15));
    lv_obj_set_style_border_color(ap,CLR(0x5A,0x20,0x20),0); lb(ap,"⚡ 1 Agent ACP pending [Confirm] [Cancel]",14,6,RD,,&lv_font_montserrat_12)lv_font_montserrat_12);
}

static void page1(lv_obj_t *p) {
    lb(p,"BLE Devices",20,10,WH,&lv_font_montserrat_14); bt(p,380,6,80,26,AQ,CLR(0x11,0x11,0x22),"🔍 Scan");
    lv_obj_t *c1=cdx(p,20,42,440,78,CD); lb(c1,"💻",12,10,WH,&lv_font_montserrat_16);
    lb(c1,"MacBook Air",48,10,WH,&lv_font_montserrat_14); lb(c1,"● Connected 📶 -56dBm | 1.3MB/s",48,34,GN,,&lv_font_montserrat_12)lv_font_montserrat_12);
    bt(c1,378,26,50,24,CLR(0x3A,0x20,0x20),RD,"Disc");
    lb(p,"Paired Devices",20,135,GY,,&lv_font_montserrat_12)lv_font_montserrat_12);
    lv_obj_t *c2=cdx(p,20,155,440,48,CD); lb(c2,"📱",12,8,WH,&lv_font_montserrat_16);
    lb(c2,"iPhone 15 Pro",44,8,WH,&lv_font_montserrat_14); lb(c2,"○ Disconnected | Last: 2h ago",44,30,DM,,&lv_font_montserrat_12)lv_font_montserrat_12);
    lb(p,"Nearby Devices",20,218,GY,,&lv_font_montserrat_12)lv_font_montserrat_12);
    const char*nr[][3]={{"⌚","DeskMate-XXXX","Strong"},{"🎧","AirPods Pro","Medium"},{"❓","Unknown Dev","Weak"}};
    for(int i=0;i<3;i++) {
        lv_obj_t*nc=cdx(p,20,238+i*48,440,43,CD); lb(nc,nr[i][0],12,8,WH,&lv_font_montserrat_16);
        lb(nc,nr[i][1],42,6,WH,&lv_font_montserrat_14);
        char buf[32]; snprintf(buf,32,"Signal: %s | tap to connect",nr[i][2]); lb(nc,buf,42,26,DM,,&lv_font_montserrat_12)lv_font_montserrat_12);
        bt(nc,378,10,50,22,CLR(0x15,0x30,0x20),GN,"Connect");
    }
}

static void page2(lv_obj_t *p) {
    lb(p,"Token Plan",20,10,WH,&lv_font_montserrat_14);
    lv_obj_t*c=cdx(p,20,36,440,180,CD); lb(c,"📅 This Month",20,12,GY,,&lv_font_montserrat_12)lv_font_montserrat_12);
    lv_obj_t*pb=lv_obj_create(c); lv_obj_set_size(pb,400,16); lv_obj_set_pos(pb,20,38);
    lv_obj_set_style_bg_color(pb,BD,0); lv_obj_set_style_bg_opa(pb,LV_OPA_COVER,0);
    lv_obj_set_style_radius(pb,8,0); lv_obj_set_style_border_width(pb,0,0);
    lv_obj_t*pf=lv_obj_create(pb); lv_obj_set_size(pf,240,16); lv_obj_set_pos(pf,0,0);
    lv_obj_set_style_bg_color(pf,AQ,0); lv_obj_set_style_bg_opa(pf,LV_OPA_COVER,0);
    lv_obj_set_style_radius(pf,8,0); lv_obj_set_style_border_width(pf,0,0);
    lv_obj_t*pc=lv_label_create(pf); lv_label_set_text(pc,"60%"); lv_obj_center(pc);
    lv_obj_set_style_text_color(pc,CLR(0x11,0x11,0x22),0); lv_obj_set_style_text_font(pc,&lv_font_montserrat_12,0);
    lb(c,"Used",20,64,DM,,&lv_font_montserrat_12)lv_font_montserrat_12); lb(c,"1,234,567",20,82,AQ,&lv_font_montserrat_16);
    lb(c,"Remaining",230,64,DM,,&lv_font_montserrat_12)lv_font_montserrat_12); lb(c,"823,456",230,82,GN,&lv_font_montserrat_16);
    lb(c,"Total: 2,058,023 | Today: 12,340",20,112,DM,,&lv_font_montserrat_12)lv_font_montserrat_12);
    lb(c,"Sync: 5 min | Last: 2m ago",20,138,DM,,&lv_font_montserrat_12)lv_font_montserrat_12);
    lv_obj_t*c2=cdx(p,20,232,440,95,CD); lb(c2,"Model Usage",20,8,GY,,&lv_font_montserrat_12)lv_font_montserrat_12);
    const char*mn[]={"DeepSeek V4","V4 Pro","GPT-4o","Claude 4"};
    lv_color_t mc[]={AQ,PU,GN,OR}; int mv[]={45,25,20,10};
    for(int i=0;i<4;i++) {
        lb(c2,mn[i],20,30+i*16,GY,,&lv_font_montserrat_12)lv_font_montserrat_12);
        int bw=260*mv[i]/100; lv_obj_t*br=lv_obj_create(c2); lv_obj_set_size(br,bw,6);
        lv_obj_set_pos(br,130,35+i*16); lv_obj_set_style_bg_color(br,mc[i],0);
        lv_obj_set_style_bg_opa(br,LV_OPA_COVER,0); lv_obj_set_style_radius(br,3,0);
        lv_obj_set_style_border_width(br,0,0);
        char ib[8]; snprintf(ib,8,"%d%%",mv[i]); lb(c2,ib,134+bw,30+i*16,DM,,&lv_font_montserrat_12)lv_font_montserrat_12);
    }
    bt(p,20,342,120,30,AQ,CLR(0x11,0x11,0x22),"🔄 Sync"); lb(p,"Auto-syncing...",155,348,DM,,&lv_font_montserrat_12)lv_font_montserrat_12);
}

static void page3(lv_obj_t *p) {
    lb(p,"Pomodoro",20,10,WH,&lv_font_montserrat_14);
    lv_obj_t*c=cdx(p,20,36,440,220,CD);
    lv_obj_t*tm=lv_label_create(c); lv_label_set_text(tm,"25:00"); lv_obj_set_width(tm,440);
    lv_obj_set_pos(tm,0,22); lv_obj_set_style_text_align(tm,LV_TEXT_ALIGN_CENTER,0);
    lv_obj_set_style_text_color(tm,RD,0); lv_obj_set_style_text_font(tm,&lv_font_montserrat_48,0);
    lb(c,"🔥 Working...",0,76,AQ,&lv_font_montserrat_14); lb(c,"Click=pause",0,96,DM,,&lv_font_montserrat_12)lv_font_montserrat_12);
    lv_obj_t*pb=lv_obj_create(c); lv_obj_set_size(pb,360,10); lv_obj_set_pos(pb,40,120);
    lv_obj_set_style_bg_color(pb,BD,0); lv_obj_set_style_bg_opa(pb,LV_OPA_COVER,0);
    lv_obj_set_style_radius(pb,5,0); lv_obj_set_style_border_width(pb,0,0);
    lv_obj_t*pf=lv_obj_create(pb); lv_obj_set_size(pf,180,10); lv_obj_set_pos(pf,0,0);
    lv_obj_set_style_bg_color(pf,RD,0); lv_obj_set_style_bg_opa(pf,LV_OPA_COVER,0);
    lv_obj_set_style_radius(pf,5,0); lv_obj_set_style_border_width(pf,0,0);
    bt(c,80,148,120,38,AQ,CLR(0x11,0x11,0x22),"▶ Start"); bt(c,240,148,120,38,CLR(0x30,0x30,0x44),GY,"⏸ Pause");
    bt(c,80,200,95,28,CLR(0x15,0x30,0x20),GN,"Done"); bt(c,190,200,95,28,CLR(0x3A,0x20,0x20),RD,"Skip");
    bt(c,300,200,50,28,CLR(0x30,0x30,0x44),GY,"×"); lb(c,"Today 3 🍅 | Total 127",100,240,DM,,&lv_font_montserrat_12)lv_font_montserrat_12);
    lv_obj_t*c2=cdx(p,20,272,440,95,CD);
    lb(c2,"Timer Settings",20,8,GY,,&lv_font_montserrat_12)lv_font_montserrat_12); lb(c2,"Work: 25 min",20,30,WH,&lv_font_montserrat_14);
    lb(c2,"Break: 5 min",230,30,WH,&lv_font_montserrat_14); lb(c2,"Long break: 15m (every 4)",20,54,DM,,&lv_font_montserrat_12)lv_font_montserrat_12);
    lb(c2,"🍅 Total: 127 pomodoros",20,74,DM,,&lv_font_montserrat_12)lv_font_montserrat_12);
}

static void page4(lv_obj_t *p) {
    lb(p,"Voice Input",20,10,WH,&lv_font_montserrat_14);
    lv_obj_t*c=cdx(p,20,36,440,340,CD);
    lv_obj_t*mc=lv_obj_create(c); lv_obj_set_size(mc,110,110); lv_obj_set_pos(mc,165,20);
    lv_obj_set_style_bg_color(mc,CLR(0x22,0x22,0x36),0); lv_obj_set_style_bg_opa(mc,LV_OPA_COVER,0);
    lv_obj_set_style_radius(mc,55,0); lv_obj_set_style_border_width(mc,2,0);
    lv_obj_set_style_border_color(mc,AQ,0);
    lv_obj_t*mi=lv_label_create(mc); lv_label_set_text(mi,"🎤"); lv_obj_center(mi);
    lv_obj_set_style_text_font(mi,&lv_font_montserrat_48,0);
    lb(c,"Hold to Speak",0,138,GY,&lv_font_montserrat_14);
    lv_obj_t*rc=cdx(c,20,168,400,56,C2); lv_obj_set_style_border_color(rc,CLR(0x3A,0x3A,0x5E),0);
    lb(rc,"> Write a Python scraper,",12,6,WH,,&lv_font_montserrat_12)lv_font_montserrat_12);
    lb(rc,"> to extract data from a website...",12,28,GY,&lf,&lv_font_montserrat_12lv_font_montserrat_12);
    lb(c,"🔵 Connected: MacBook Air | ~12ms",20,250,GN,,&lv_font_montserrat_12)lv_font_montserrat_12);
    lb(c,"📶 Signal good",300,250,DM,,&lv_font_montserrat_12)lv_font_montserrat_12); lb(c,"Last transcription:",20,280,GY,,&lv_font_montserrat_12)lv_font_montserrat_12);
    lv_obj_t*hc=cdx(c,20,300,400,34,C2); lb(hc,"📋 \"Get token plan on timer...\"",10,8,DM,,&lv_font_montserrat_12)lv_font_montserrat_12);
}

int main(int argc, char**argv) {
    (void)argc;(void)argv; lv_init(); sdl_hal_init(480,480);
    lv_obj_t*scr=lv_scr_act(); lv_obj_set_style_bg_color(scr,BG,0); lv_obj_set_style_bg_opa(scr,LV_OPA_COVER,0);
    lv_obj_t*sb=lv_obj_create(scr); lv_obj_set_size(sb,480,34); lv_obj_set_pos(sb,0,0);
    lv_obj_set_style_bg_color(sb,C3,0); lv_obj_set_style_bg_opa(sb,LV_OPA_COVER,0);
    lv_obj_set_style_border_width(sb,0,0); lv_obj_set_style_pad_all(sb,0,0);
    lb(sb,"🔵 MacBook Air",8,6,AQ,,&lv_font_montserrat_12)lv_font_montserrat_12); lb(sb,"█████░ 83%",330,6,GN,,&lv_font_montserrat_12)lv_font_montserrat_12);
    lb(sb,"12:30",430,6,WH,,&lv_font_montserrat_12)lv_font_montserrat_12);
    lv_obj_t*se=lv_obj_create(scr); lv_obj_set_size(se,480,1); lv_obj_set_pos(se,0,34);
    lv_obj_set_style_bg_color(se,BD,0); lv_obj_set_style_border_width(se,0,0);
    lv_obj_t*cn=lv_obj_create(scr); lv_obj_set_size(cn,460,398); lv_obj_set_pos(cn,10,37);
    lv_obj_set_style_bg_color(cn,BG,0); lv_obj_set_style_bg_opa(cn,LV_OPA_COVER,0);
    lv_obj_set_style_border_width(cn,0,0); lv_obj_set_style_pad_all(cn,0,0);
    lv_obj_set_style_radius(cn,0,0); lv_obj_set_scrollbar_mode(cn,LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_snap_x(cn,LV_SCROLL_SNAP_CENTER);
    lv_obj_set_flex_flow(cn,LV_FLEX_FLOW_ROW); lv_obj_set_style_pad_column(cn,0,0);
    void(*bld[])(lv_obj_t*)={page0,page1,page2,page3,page4};
    lv_obj_t*pp[5]; for(int i=0;i<5;i++) {
        pp[i]=lv_obj_create(cn); lv_obj_set_size(pp[i],460,398);
        lv_obj_set_style_bg_color(pp[i],BG,0); lv_obj_set_style_border_width(pp[i],0,0);
        lv_obj_set_style_pad_all(pp[i],0,0); lv_obj_set_scrollbar_mode(pp[i],LV_SCROLLBAR_MODE_OFF);
        bld[i](pp[i]);
    }
    lv_obj_t*dc=lv_obj_create(scr); lv_obj_set_size(dc,480,16); lv_obj_set_pos(dc,0,464);
    lv_obj_set_style_bg_color(dc,CLR(0,0,0),0); lv_obj_set_style_bg_opa(dc,LV_OPA_TRANSP,0);
    lv_obj_set_style_border_width(dc,0,0); lv_obj_set_style_pad_all(dc,0,0);
    int sx=(480-5*6-4*10)/2; for(int i=0;i<5;i++) {
        lv_obj_t*d=lv_obj_create(dc); lv_obj_set_size(d,6,6); lv_obj_set_pos(d,sx+i*16,5);
        lv_obj_set_style_bg_color(d,i==0?AQ:DM,0); lv_obj_set_style_bg_opa(d,LV_OPA_COVER,0);
        lv_obj_set_style_radius(d,LV_RADIUS_CIRCLE,0); lv_obj_set_style_border_width(d,0,0);
    }
    for(int f=0;f<80;f++){uint32_t s=lv_timer_handler();usleep((s==LV_NO_TIMER_READY?LV_DEF_REFR_PERIOD:s)*1000);}
    const char*sp=getenv("LVGL_SCREENSHOT");if(!sp)sp="/tmp/lvgl_design.bmp";
    lv_draw_buf_t*snap=lv_snapshot_take(lv_screen_active(),LV_COLOR_FORMAT_ARGB8888);
    if(snap&&snap->data) {
        int w=snap->header.w,h=snap->header.h;uint32_t st=snap->header.stride;
        uint32_t*px=(uint32_t*)snap->data;FILE*f=fopen(sp,"wb");
        if(f){int rs=(w*3+3)&~3,fs=54+rs*h;unsigned char hd[54]={0};
        hd[0]='B';hd[1]='M';hd[2]=fs;hd[10]=54;hd[14]=40;hd[18]=w;hd[19]=w>>8;hd[22]=h;hd[23]=h>>8;hd[26]=1;hd[28]=24;
        fwrite(hd,1,54,f);unsigned char*rw=malloc(rs);
        for(int y=h-1;y>=0;y--){for(int x=0;x<w;x++){uint32_t p=px[y*(st/4)+x];rw[x*3]=p&0xff;rw[x*3+1]=(p>>8)&0xff;rw[x*3+2]=(p>>16)&0xff;}fwrite(rw,1,rs,f);}
        free(rw);fclose(f);printf("✅ %s (%dx%d)\n",sp,w,h);}lv_draw_buf_destroy(snap);
    }
    while(1){uint32_t s=lv_timer_handler();usleep((s==LV_NO_TIMER_READY?LV_DEF_REFR_PERIOD:s)*1000);}
    return 0;
}
