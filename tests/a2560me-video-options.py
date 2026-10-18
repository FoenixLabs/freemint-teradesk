#!/usr/bin/env python3
"""Exercise the production Foenix Video Options branch with VDI dimensions."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
text=(root/'video.c').read_text()
a=text.index('\t/* Foenix has no Atari shifter.')
b=text.index('\n\t/* Set radiobutton',a)
branch=text[a:b]
source=r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define _WORD short
static long vdo, reservation;
static int xd_nplanes;
static struct { short g_w,g_h; } xd_screen;
static char message_seen[256];
static void find_cookie(long key,long *p) { assert(key==0x4656524dL); *p=reservation; }
static void form_alert(int b,const char *s) { assert(b==1); strcpy(message_seen,s); }
static short options(void) {
'''+branch+r'''
 return -1;
}
int main(void) {
 vdo=0xf258; reservation=0x3f800000; xd_nplanes=16;
 xd_screen.g_w=1280; xd_screen.g_h=960;
 assert(options()==0 && strstr(message_seen,"1280x960 / 640x480"));
 assert(!strstr(message_seen,"320x200"));
 xd_screen.g_w=640; xd_screen.g_h=512;
 assert(options()==0 && strstr(message_seen,"1280x1024 / 640x512"));
 assert(strstr(message_seen,"Current: 640x512, 16 bpp"));
 xd_screen.g_w=1280; xd_screen.g_h=960; xd_nplanes=1; reservation=0;
 assert(options()==0 && strstr(message_seen,"Monochrome mode only"));
 vdo=0; assert(options()==-1);
 puts("PASS: Foenix Video Options full/half timing, actual VDI depth, monochrome fallback and Atari exclusion");
}
'''
with tempfile.TemporaryDirectory() as tmp:
 c=Path(tmp)/'video.c'; exe=Path(tmp)/'video';c.write_text(source)
 subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(c),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
