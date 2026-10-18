/* GEM background samples and paged, persistent BMP thumbnails. GPL-2.0+. */
#ifndef MEVIDEO_PREVIEW_H
#define MEVIDEO_PREVIEW_H
#include <dirent.h>
#include <strings.h>
#define ME_THUMBS "D:\\WALLPAPR\\THUMBS"
struct me_sample { USERBLK user; unsigned char *pixels; short width,height,planes,colour,pattern,picture; };

static _WORD _CDECL me_draw_sample(PARMBLK *pb)
{
    struct me_sample *s=(struct me_sample *)pb->pb_parm;
    _WORD clip[4], rect[4], pxy[8], colours2[2]={1,0};
    MFDB source, screen;
    short px,py;
    clip[0]=pb->pb_xc; clip[1]=pb->pb_yc;
    clip[2]=pb->pb_xc+pb->pb_wc-1; clip[3]=pb->pb_yc+pb->pb_hc-1;
    vs_clip(vdi_handle,1,clip);
    vswr_mode(vdi_handle,MD_REPLACE);
    vsf_perimeter(vdi_handle,0);
    vsf_color(vdi_handle,s->picture ? 0 : s->colour);
    vsf_interior(vdi_handle,s->picture || !s->pattern ? FIS_SOLID : s->pattern<=24 ? FIS_PATTERN : FIS_HATCH);
    if(s->pattern) vsf_style(vdi_handle,s->pattern<=24 ? s->pattern : s->pattern-24);
    rect[0]=pb->pb_x; rect[1]=pb->pb_y; rect[2]=pb->pb_x+pb->pb_w-1; rect[3]=pb->pb_y+pb->pb_h-1;
    v_bar(vdi_handle,rect);
    if(s->pixels && s->picture) {
        memset(&source,0,sizeof(source)); memset(&screen,0,sizeof(screen));
        source.fd_addr=s->pixels; source.fd_w=s->width; source.fd_h=s->height;
        source.fd_wdwidth=s->width/16; source.fd_nplanes=s->planes;
        px=pb->pb_x+(pb->pb_w-s->width)/2; py=pb->pb_y+(pb->pb_h-s->height)/2;
        pxy[0]=pxy[1]=0; pxy[2]=s->width-1; pxy[3]=s->height-1;
        pxy[4]=px; pxy[5]=py; pxy[6]=px+s->width-1; pxy[7]=py+s->height-1;
        if(s->planes==1 && xd_nplanes!=1) vrt_cpyfm(vdi_handle,MD_REPLACE,pxy,&source,&screen,colours2);
        else vro_cpyfm(vdi_handle,S_ONLY,pxy,&source,&screen);
    }
    vsf_interior(vdi_handle,FIS_HOLLOW); vsf_color(vdi_handle,1); vsf_perimeter(vdi_handle,1);
    v_bar(vdi_handle,rect);
    if(pb->pb_currstate & OS_SELECTED) {
        rect[0]+=2; rect[1]+=2; rect[2]-=2; rect[3]-=2; v_bar(vdi_handle,rect);
    }
    vs_clip(vdi_handle,0,clip);
    return pb->pb_currstate & ~OS_SELECTED;
}

static void me_sample_object(OBJECT *o,short index,struct me_sample *s)
{
    s->user.ub_code=me_draw_sample; s->user.ub_parm=(long)s;
    o[index].ob_type=G_USERDEF; o[index].ob_spec.userblk=&s->user;
}

static void me_load_sample(struct me_sample *s,const char *path,short depth)
{
    free(s->pixels); s->pixels=NULL;
    /* Samples use the running VDI format; monochrome can be previewed in colour. */
    s->planes=depth==1 ? 1 : xd_nplanes;
    (void)Dcreate(ME_THUMBS);
    if(*path) s->pixels=me_thumbnail(path,ME_THUMBS,s->width,s->height,s->planes);
}

/* Four visible entries bound memory use; page scans support arbitrary folders. */
static int me_wallpapers(const char *directory,int offset,char paths[4][260],int *more)
{
    DIR *dir=opendir(directory);
    struct dirent *entry;
    int count=0,skip=0;
    *more=0;
    if(!dir) return 0;
    while((entry=readdir(dir))!=NULL) {
        const char *dot=strrchr(entry->d_name,'.');
        if(!dot || strcasecmp(dot,".BMP") || strlen(directory)+strlen(entry->d_name)+2>260) continue;
        if(skip++<offset) continue;
        if(count==4) { *more=1; break; }
        sprintf(paths[count++],"%s\\%s",directory,entry->d_name);
    }
    closedir(dir); return count;
}

static int me_choose_wallpaper(char *bitmap,short depth,short mode)
{
    enum { CTITLE=1,CFOLDER,CTHUMB,CLABEL=CTHUMB+4,CPREV=CLABEL+4,CNEXT,CBROWSE,CUSE,CCANCEL,CSTATUS,CNUM };
    OBJECT o[CNUM]; struct me_sample samples[4];
    char directory[260]="D:\\WALLPAPR", paths[4][260], labels[4][26], candidate[260],folder[54],status[54];
    short i,button,selected=-1;
    int offset=0,count=0,more=0,reload=1,result=0;
    _WORD x,y,w,h;
    strcpy(candidate,bitmap);
    if(*bitmap) { char *p; strcpy(directory,bitmap); p=strrchr(directory,'\\'); if(p) *p=0; else strcpy(directory,"D:\\WALLPAPR"); }
    memset(samples,0,sizeof(samples));
    widget(o,0,G_BOX,0,0,58,27,NULL,0);
    o[0].ob_next=-1; o[0].ob_head=1; o[0].ob_tail=CNUM-1; o[0].ob_spec.index=0x00021100L;
    widget(o,CTITLE,G_STRING,17,1,30,1,"Choose wallpaper",0);
    widget(o,CFOLDER,G_STRING,3,3,52,1,folder,0);
    for(i=0;i<4;i++) {
        widget(o,CTHUMB+i,G_BOX,5+(i%2)*28,4+(i/2)*9,20,6,NULL,OF_SELECTABLE|OF_EXIT);
        samples[i].width=mode>=2 ? 112 : 128; samples[i].height=mode>=2 ? 90 : 96; samples[i].picture=1;
        me_sample_object(o,CTHUMB+i,&samples[i]);
        widget(o,CLABEL+i,G_STRING,3+(i%2)*28,11+(i/2)*9,25,1,labels[i],0);
    }
    widget(o,CPREV,G_BUTTON,3,23,10,1,"Previous",OF_SELECTABLE|OF_EXIT);
    widget(o,CNEXT,G_BUTTON,15,23,10,1,"Next",OF_SELECTABLE|OF_EXIT);
    widget(o,CBROWSE,G_BUTTON,29,23,25,1,"Browse BMP...",OF_SELECTABLE|OF_EXIT);
    widget(o,CUSE,G_BUTTON,3,25,25,1,"Use wallpaper",OF_SELECTABLE|OF_EXIT|OF_DEFAULT);
    widget(o,CCANCEL,G_BUTTON,32,25,22,1,"Cancel",OF_SELECTABLE|OF_EXIT);
    widget(o,CSTATUS,G_STRING,3,21,52,1,status,OF_LASTOB); o[CSTATUS].ob_next=0;
    me_center(o,&x,&y,&w,&h); form_dial(FMD_START,0,0,0,0,x,y,w,h);
    for(;;) {
        if(reload) {
            count=me_wallpapers(directory,offset,paths,&more); selected=-1;
            snprintf(folder,sizeof(folder),"%.39s (page %d)",directory,offset/4+1);
            strcpy(status,count ? "Select a thumbnail to preview its wallpaper." : "No BMP files here. Use Browse BMP...");
            for(i=0;i<4;i++) {
                labels[i][0]=0;
                free(samples[i].pixels); samples[i].pixels=NULL;
                o[CTHUMB+i].ob_flags=OF_SELECTABLE|OF_EXIT|(i<count ? 0 : OF_HIDETREE);
                if(i<count) {
                    const char *p=strrchr(paths[i],'\\');
                    snprintf(labels[i],sizeof(labels[i]),"%.24s",p ? p+1 : paths[i]);
                    me_load_sample(&samples[i],paths[i],depth);
                    if(!strcmp(candidate,paths[i])) selected=i;
                }
            }
            reload=0;
        }
        for(i=0;i<4;i++) o[CTHUMB+i].ob_state=i==selected ? OS_SELECTED : 0;
        o[CPREV].ob_state=offset ? 0 : OS_DISABLED;
        o[CNEXT].ob_state=more ? 0 : OS_DISABLED;
        o[CUSE].ob_state=selected>=0 && samples[selected].pixels ? 0 : OS_DISABLED;
        objc_draw(o,0,MAX_DEPTH,x,y,w,h); button=form_do(o,0)&0x7fff;
        if(button<0 || button>=CNUM) continue;
        o[button].ob_state &= ~OS_SELECTED;
        if(button==CCANCEL) break;
        if(button>=CTHUMB && button<CTHUMB+count) {
            selected=button-CTHUMB;
            if(samples[selected].pixels) { strcpy(candidate,paths[selected]); snprintf(status,sizeof(status),"%.50s",labels[selected]); }
            else strcpy(status,"Cannot preview: use an uncompressed 24/32-bit BMP.");
        }
        if(button==CPREV && offset) { offset-=4; reload=1; }
        if(button==CNEXT && more) { offset+=4; reload=1; }
        if(button==CBROWSE) {
            char path[520],name[260]=""; _WORD picked=0;
            sprintf(path,"%s\\*.BMP",directory);
            if(fsel_exinput(path,name,&picked,"Choose wallpaper folder / BMP") && picked) {
                char *p=strrchr(path,'\\');
                if(p && (size_t)(p-path+1)+strlen(name)<sizeof(candidate)) {
                    strcpy(p+1,name); strcpy(candidate,path); *p=0; strcpy(directory,path);
                    /* Find the selected file's page without retaining a full listing. */
                    offset=0;
                    do { count=me_wallpapers(directory,offset,paths,&more);
                        for(i=0;i<count;i++) if(!strcmp(paths[i],candidate)) break;
                        if(i<count || !more) break;
                        offset+=4;
                    } while(1);
                    reload=1;
                }
            }
        }
        if(button==CUSE && selected>=0 && samples[selected].pixels) { strcpy(bitmap,candidate); result=1; break; }
    }
    form_dial(FMD_FINISH,0,0,0,0,x,y,w,h);
    for(i=0;i<4;i++) free(samples[i].pixels);
    return result;
}
#endif
