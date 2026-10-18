/* A2560Me video/background preferences. GPL-2.0-or-later. */
#include <library.h>
#include <xdialog.h>
#include <mint/mintbind.h>
#include <mint/cookie.h>
#include "resource.h"
#include "desk.h"
#include "screen.h"
#include "video.h"
#include "mevideo_io.h"

#define ME_PREF "D:\\DESKTOP\\VIDEO.INF"
#define ME_BOOT "D:\\MINT\\MINT.CNF"
#define ME_FVDI "D:\\MINT\\FVDI\\FVDI.SYS"
#define ME_OFF "# A2560Me video: monochrome (fVDI disabled)"

static const short widths[] = {1280, 640, 1280, 640};
static const short heights[] = {960, 480, 1024, 512};
static const char *colours[] = {"White", "Black", "Red", "Green", "Blue", "Cyan",
    "Yellow", "Magenta", "Light grey", "Dark grey", "Light red", "Light green",
    "Light blue", "Light cyan", "Light yellow", "Light magenta"};

enum { ROOTBOX, TITLE, CURRENT, RESLABEL, RESOLUTION, DEPTHLABEL, DEPTH,
    BGLABEL, BACKGROUND, COLOUR, PATTERN, WALLPAPER, FILENAME, PREVIEW, NOTE, SAVE, CANCEL, COUNT };

/* Main's normal configuration serializer includes all the owner's icons/settings. */
extern const char *me_desktop_file(void);
extern int me_desktop_snapshot(const char *path);

static void widget(OBJECT *o, short i, short type, short x, short y,
                   short w, short h, const char *text, short flags)
{
    memset(&o[i], 0, sizeof(o[i]));
    o[i].ob_next = i == COUNT-1 ? 0 : i+1;
    o[i].ob_head = o[i].ob_tail = -1;
    o[i].ob_type = type; o[i].ob_flags = flags;
    o[i].ob_spec.free_string = (char *)text;
    o[i].ob_x=x*8; o[i].ob_y=y*16; o[i].ob_width=w*8; o[i].ob_height=h*16;
}

#include "mevideo_preview.h"

static int save_video(short mode, short depth, short picture, const char *bitmap,
                      short colour, short pattern)
{
    char driver[80], directory[160], image[180], image_temp[180], image_backup[180];
    char desk_temp[520], desk_backup[520];
    const char *desktop_path = me_desktop_file();
    char *p;
    short oldcolour = options.dsk_colour, oldpattern = options.dsk_pattern;
    struct me_file files[5];
    FILE *f;
    int ok, i;
    if (depth == 1) mode=0;
    memset(files, 0, sizeof(files));
    files[0].path=ME_BOOT; files[0].temp="D:\\MINT\\MINT.VNT"; files[0].backup="D:\\MINT\\MINT.VBK";
    files[1].path=ME_FVDI; files[1].temp="D:\\MINT\\FVDI\\FVDI.VNT"; files[1].backup="D:\\MINT\\FVDI\\FVDI.VBK";
    files[2].path=ME_PREF; files[2].temp="D:\\DESKTOP\\VIDEO.VNT"; files[2].backup="D:\\DESKTOP\\VIDEO.VBK";
    if (!desktop_path || strlen(desktop_path) > 490) return 0;
    strcpy(desk_temp, desktop_path);
    p=strrchr(desk_temp, '\\');
    if (!p) return 0;
    strcpy(p+1, "VIDEOTMP.INF");
    strcpy(desk_backup, desk_temp); strcpy(strrchr(desk_backup,'\\')+1, "VIDEOBCK.INF");
    files[3].path=desktop_path; files[3].temp=desk_temp; files[3].backup=desk_backup;
    sprintf(directory, "D:\\MINT\\XAAES\\%d%d.%d", widths[mode], heights[mode], depth);
    sprintf(image, "%s\\xa_form.mfd", directory);
    sprintf(image_temp, "%s\\xa_form.vnt", directory);
    sprintf(image_backup, "%s\\xa_form.vbk", directory);
    files[4].path=image; files[4].temp=picture ? image_temp : NULL; files[4].backup=image_backup;
    if (picture) {
        (void)Dcreate(directory);
        if (!me_bmp(bitmap, image_temp, widths[mode], heights[mode], depth)) {
            form_alert(1, "[1][Cannot read wallpaper.|Use an uncompressed 24/32-bit BMP.|The previous settings are unchanged.][OK]");
            return 0;
        }
    }
    sprintf(driver, "01r a2560me.sys%s %d%s\n", mode&1 ? " half" : "", mode>=2 ? 1024 : 960, depth==32 ? " rgb24" : "");
    ok=me_replace(ME_BOOT, files[0].temp, "exec u:/d/mint/fvdi/fvdi.prg", ME_OFF,
        depth != 1 ? "exec u:/d/mint/fvdi/fvdi.prg\n" : ME_OFF "\n");
    if (ok) ok=me_replace(ME_FVDI, files[1].temp, "01r a2560me.sys", NULL, driver);
    if (ok) {
        f=fopen(files[2].temp,"wb");
        if (!f) ok=0;
        else {
            if (fprintf(f,"A2560Me video 1\n%d %d %d\n%s\n",mode,depth,picture,bitmap)<0) ok=0;
            if (fclose(f)) ok=0;
        }
    }
    if (ok) {
        options.dsk_colour = picture ? 0 : colour;
        options.dsk_pattern = pattern;
        ok = me_desktop_snapshot(desk_temp) >= 0;
        options.dsk_colour=oldcolour; options.dsk_pattern=oldpattern;
    }
    if (ok) ok=me_commit(files,5);
    if (!ok) {
        for (i=0; i<5; ++i) if (files[i].temp) remove(files[i].temp);
        form_alert(1,"[1][Could not save video settings.|Check the drive and startup files.|Existing settings have been retained.][OK]");
    } else {
        options.dsk_colour = picture ? 0 : colour;
        options.dsk_pattern = pattern;
    }
    return ok;
}

short me_voptions(void)
{
    OBJECT o[COUNT];
    char current[64], resolution[40], depthtext[40], bgtext[40], colourtext[40], patterntext[40];
    char bitmap[260]="", shown[52], header[64];
    struct me_sample sample;
    short mode=0, depth=xd_nplanes == 32 ? 32 : xd_nplanes == 16 ? 16 : 1, picture=0, button, choice;
    short colour=options.dsk_colour, pattern=options.dsk_pattern;
    _WORD x,y,w,h;
    FILE *f=fopen(ME_PREF,"rb");
    int m,d,b;
    long reservation=0;
    int colour_available;
    find_cookie(0x4656524dL, &reservation);
    colour_available=reservation==0x3f800000L && me_exists(ME_FVDI);
    if (f) {
        if (fgets(header,sizeof(header),f) && !strcmp(header,"A2560Me video 1\n") &&
            fscanf(f,"%d %d %d\n",&m,&d,&b)==3 && m>=0 && m<4 && (d==1 || d==16 || d==32) && (b==0 || b==1)) {
            mode=m; depth=d; picture=b;
            if (fgets(bitmap,sizeof(bitmap),f)) bitmap[strcspn(bitmap,"\r\n")]=0;
        }
        fclose(f);
    } else {
        for (m=0; m<4; ++m) if (xd_screen.g_w==widths[m] && xd_screen.g_h==heights[m]) mode=m;
    }
    if (!colour_available) depth=1;
    if (depth==1) mode=0;
    sprintf(current,"Running: %dx%d, %d-bit",xd_screen.g_w,xd_screen.g_h,xd_nplanes);
    memset(&sample,0,sizeof(sample)); sample.width=mode>=2 ? 160 : 192; sample.height=mode>=2 ? 128 : 144;
    me_load_sample(&sample,bitmap,depth);
    widget(o,0,G_BOX,0,0,58,28,NULL,0);
    o[0].ob_next=-1; o[0].ob_head=1; o[0].ob_tail=COUNT-1; o[0].ob_spec.index=0x00021100L;
    widget(o,TITLE,G_STRING,15,1,35,1,"A2560Me Video Options",0);
    widget(o,CURRENT,G_STRING,3,2,52,1,current,0);
    widget(o,RESLABEL,G_STRING,3,4,14,1,"Resolution",0);
    widget(o,RESOLUTION,G_BUTTON,20,4,34,1,resolution,OF_SELECTABLE|OF_EXIT);
    widget(o,DEPTHLABEL,G_STRING,3,6,14,1,"Colour depth",0);
    widget(o,DEPTH,G_BUTTON,20,6,34,1,depthtext,OF_SELECTABLE|OF_EXIT);
    widget(o,BGLABEL,G_STRING,3,8,14,1,"Background",0);
    widget(o,BACKGROUND,G_BUTTON,20,8,34,1,bgtext,OF_SELECTABLE|OF_EXIT);
    widget(o,COLOUR,G_BUTTON,3,10,25,1,colourtext,OF_SELECTABLE|OF_EXIT);
    widget(o,PATTERN,G_BUTTON,30,10,24,1,patterntext,OF_SELECTABLE|OF_EXIT);
    widget(o,WALLPAPER,G_BUTTON,3,22,51,1,"Choose wallpaper...",OF_SELECTABLE|OF_EXIT);
    widget(o,FILENAME,G_STRING,3,23,52,1,shown,0);
    widget(o,PREVIEW,G_BOX,17,12,24,9,NULL,0);
    me_sample_object(o,PREVIEW,&sample);
    widget(o,NOTE,G_STRING,3,24,52,1,"Changes take effect after restart.",0);
    widget(o,SAVE,G_BUTTON,6,26,20,1,"Apply...",OF_SELECTABLE|OF_EXIT|OF_DEFAULT);
    widget(o,CANCEL,G_BUTTON,32,26,20,1,"Cancel",OF_SELECTABLE|OF_EXIT|OF_LASTOB);
    o[FILENAME].ob_y+=4;
    form_center(o,&x,&y,&w,&h);
    form_dial(FMD_START,0,0,0,0,x,y,w,h);
    for (;;) {
        sprintf(resolution,"%d x %d  (click to cycle)",widths[mode],heights[mode]);
        strcpy(depthtext,depth==32 ? "24-bit RGB / 16M colours" : depth==16 ? "16-bit / 65536 colours" : "1-bit / monochrome");
        strcpy(bgtext,picture ? "XaAES wallpaper" : "Colour and pattern");
        sprintf(colourtext,"Colour: %s",colours[colour&15]);
        sprintf(patterntext,"Pattern: %d",pattern);
        if (*bitmap) { const char *p=strrchr(bitmap,'\\'); p=p ? p+1 : bitmap; snprintf(shown,sizeof(shown),"%.50s",p); }
        else strcpy(shown,"No wallpaper selected");
        o[RESOLUTION].ob_state=depth==1 ? OS_DISABLED : 0;
        o[DEPTH].ob_state=colour_available ? 0 : OS_DISABLED;
        o[COLOUR].ob_state=o[PATTERN].ob_state=picture ? OS_DISABLED : 0;
        sample.colour=colour; sample.pattern=pattern; sample.picture=picture;
        objc_draw(o,0,MAX_DEPTH,x,y,w,h);
        button=form_do(o,0)&0x7fff;
        if(button<0 || button>=COUNT) continue;
        o[button].ob_state &= ~OS_SELECTED;
        if (button==CANCEL) break;
        if (button==RESOLUTION && depth!=1) { mode=(mode+1)%4; sample.width=mode>=2 ? 160 : 192; sample.height=mode>=2 ? 128 : 144; me_load_sample(&sample,bitmap,depth); }
        if (button==DEPTH && colour_available) { depth=depth==1 ? 16 : depth==16 ? 32 : 1; if(depth==1) mode=0; colour &= depth==1 ? 1 : 15; sample.width=mode>=2 ? 160 : 192; sample.height=mode>=2 ? 128 : 144; me_load_sample(&sample,bitmap,depth); }
        if (button==BACKGROUND) picture=!picture;
        if (button==COLOUR) colour=(colour+1)%(depth==1 ? 2 : 16);
        if (button==PATTERN) pattern=pattern%37+1;
        if (button==WALLPAPER) {
            if(me_choose_wallpaper(bitmap,depth,mode)) {
                picture=1; me_load_sample(&sample,bitmap,depth);
                if(!sample.pixels) form_alert(1,"[1][Wallpaper preview is unavailable.|Existing settings are unchanged.][OK]");
            }
        }
        if (button==SAVE) {
            if (picture && !*bitmap) { form_alert(1,"[1][Choose a wallpaper BMP first.][OK]"); continue; }
            choice=form_alert(3,"[2][Save video and background settings?|Restart closes running applications.][Save + reboot|Save for later|Cancel]");
            if (choice==3) continue;
            if (save_video(mode,depth,picture,bitmap,colour,pattern)) {
                form_dial(FMD_FINISH,0,0,0,0,x,y,w,h);
                if (choice==1) { Shutdown(1L); form_alert(1,"[1][Restart was not completed.|Saved settings will apply next boot.][OK]"); }
                free(sample.pixels);
                return 0;
            }
        }
    }
    form_dial(FMD_FINISH,0,0,0,0,x,y,w,h);
    free(sample.pixels);
    return 0;
}
