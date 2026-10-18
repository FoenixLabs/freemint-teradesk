#!/usr/bin/env python3
"""Test production video config transactions and XaAES wallpaper conversion."""
from pathlib import Path
import subprocess,tempfile,struct
root=Path(__file__).resolve().parents[1]
source=r'''
#include <assert.h>
#include <dirent.h>
#include "mevideo_io.h"
static void put(const char *p,const char *v) { FILE *f=fopen(p,"wb"); assert(f);fputs(v,f);assert(!fclose(f)); }
static void expect(const char *p,const char *v) { char b[256]={0};FILE *f=fopen(p,"rb");assert(f);fread(b,1,255,f);fclose(f);assert(!strcmp(b,v)); }
int main(void) {
 struct me_file files[3]={{"a","a.tmp","a.bak",0,0},{"b","b.tmp","b.bak",0,0},{"c","c.tmp","c.bak",0,0}};
 unsigned char pixels[64];FILE *f;
 put("a","old-a");put("b","old-b");put("c","old-c");put("a.tmp","new-a");put("b.tmp","new-b");
 assert(!me_commit(files,3));expect("a","old-a");expect("b","old-b");expect("c","old-c");
 put("a.tmp","new-a");put("b.tmp","new-b");files[2].temp=NULL;
 assert(me_commit(files,3));expect("a","new-a");expect("b","new-b");assert(!me_exists("c"));
 assert(!me_exists("a.bak") && !me_exists("b.bak") && !me_exists("c.bak"));
 put("boot","# owner's network startup\nexec keep-me\nexec fvdi\nGEM=desktop\n");
 assert(me_replace("boot","boot.tmp","exec fvdi",NULL,"# mono\n"));
 expect("boot.tmp","# owner's network startup\nexec keep-me\n# mono\nGEM=desktop\n");
 assert(!me_replace("boot","boot.tmp","absent",NULL,"bad\n"));assert(!me_exists("boot.tmp"));
 put("duplicate","exec fvdi\nexec fvdi\n");assert(!me_replace("duplicate","boot.tmp","exec fvdi",NULL,"bad\n"));
 assert(me_bmp("rgb.bmp","rgb.mfd",16,2,16));
 f=fopen("rgb.mfd","rb");assert(fread(pixels,1,64,f)==64);fclose(f);
 assert(pixels[0]==0xf8 && pixels[1]==0 && pixels[16]==7 && pixels[17]==0xe0);
 assert(pixels[32]==0 && pixels[33]==0x1f && pixels[48]==0xff && pixels[49]==0xff);
 assert(me_bmp("bw.bmp","bw.mfd",16,1,1));f=fopen("bw.mfd","rb");assert(fread(pixels,1,2,f)==2);fclose(f);
 assert(pixels[0]==0xff && pixels[1]==0);
 assert(me_bmp("top.bmp","top.mfd",16,2,16));expect("unused","");
 assert(!me_bmp("truncated.bmp","bad.mfd",16,2,16));assert(!me_exists("bad.mfd"));
 assert(me_bmp("rgb.bmp","rgb32.mfd",16,2,32));
 f=fopen("rgb32.mfd","rb");assert(fread(pixels,1,64,f)==64);fclose(f);
 assert(pixels[0]==0 && pixels[1]==255 && pixels[2]==0 && pixels[3]==0);
 assert(pixels[32]==0 && pixels[33]==0 && pixels[34]==255 && pixels[35]==0);
 { unsigned char *a,*b; unsigned n;
   a=me_thumbnail("rgb.bmp","cache",16,2,16);assert(a);
   b=me_thumbnail("rgb.bmp","cache",16,2,16);assert(b && !memcmp(a,b,64));free(b);
   /* Corrupt cache payloads must be regenerated, not painted into the GUI. */
   { DIR *dir=opendir(".");struct dirent *e;assert(dir);
     while((e=readdir(dir))!=NULL) if(!strncmp(e->d_name,"cache\\",6) && strstr(e->d_name,".BIN")) put(e->d_name,"bad");
     closedir(dir);
   }
   b=me_thumbnail("rgb.bmp","cache",16,2,16);assert(b && !memcmp(a,b,64));free(b);
   /* Read-only/missing cache still previews. */
   b=me_thumbnail("rgb.bmp",NULL,16,2,16);assert(b && !memcmp(a,b,64));free(b);
   b=me_thumbnail("rgb.bmp","missing/cache",16,2,32);assert(b && b[1]==255);free(b);
   f=fopen("rgb.bmp","r+b");assert(f);assert(!fseek(f,62,SEEK_SET));fputc(255,f);fputc(0,f);fputc(0,f);assert(!fseek(f,0,SEEK_END));fputc(0,f);fclose(f);
   b=me_thumbnail("rgb.bmp","cache",16,2,16);assert(b && b[0]==0 && b[1]==31);free(b);free(a);
   for(n=0;n<3;n++) assert(!me_thumbnail("truncated.bmp","cache",16,2,n==0?1:n==1?16:32));
   assert(!me_thumbnail("rgb.bmp",NULL,17,2,16));
 }
 assert(me_bmp("alpha.bmp","alpha.mfd",16,1,32));
 f=fopen("alpha.mfd","rb");assert(fread(pixels,1,64,f)==64);fclose(f);
 assert(pixels[0]==0 && pixels[1]==0x12 && pixels[2]==0x34 && pixels[3]==0x56);
 assert(!me_bmp("rgb.bmp","bad.mfd",16,2,24));
 puts("PASS: video config preservation, duplicate rejection, multi-file rollback, wallpaper removal, RGB565/RGB888/mono BMP conversion and thumbnail cache and malformed input");
}
'''.replace('expect("unused","");','')
def bmp(w,h,rows):
 stride=(w*3+3)&~3
 payload=b''.join(row+b'\0'*(stride-len(row)) for row in rows)
 return b'BM'+struct.pack('<IHHI',54+len(payload),0,0,54)+struct.pack('<IiiHHIIiiII',40,w,h,1,24,0,len(payload),0,0,0,0)+payload
with tempfile.TemporaryDirectory() as tmp:
 d=Path(tmp);(d/'test.c').write_text(source)
 (d/'rgb.bmp').write_bytes(bmp(2,2,[b'\xff\0\0\xff\xff\xff',b'\0\0\xff\0\xff\0']))
 (d/'top.bmp').write_bytes(bmp(2,-2,[b'\0\0\xff\0\xff\0',b'\xff\0\0\xff\xff\xff']))
 (d/'bw.bmp').write_bytes(bmp(2,1,[b'\0\0\0\xff\xff\xff']))
 (d/'alpha.bmp').write_bytes(b'BM'+struct.pack('<IHHI',58,0,0,54)+struct.pack('<IiiHHIIiiII',40,1,1,1,32,0,4,0,0,0,0)+bytes([0x56,0x34,0x12,0xab]))
 (d/'truncated.bmp').write_bytes((d/'rgb.bmp').read_bytes()[:-1])
 subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(root),str(d/'test.c'),'-o',str(d/'test')],check=True)
 subprocess.run([str(d/'test')],cwd=d,check=True)
 assert (d/'top.mfd').read_bytes()==(d/'rgb.mfd').read_bytes()
