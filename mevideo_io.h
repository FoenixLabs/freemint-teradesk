/* A2560Me Video Options file transactions and BMP conversion. GPL-2.0-or-later. */
#ifndef MEVIDEO_IO_H
#define MEVIDEO_IO_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#ifdef __XATTR
#undef st_mtime
#undef st_atime
#undef st_ctime
#endif
#include <sys/stat.h>

struct me_file { const char *path, *temp, *backup; int existed, installed; };

static int me_exists(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fclose(f); return 1;
}

static int me_commit(struct me_file *files, int count)
{
    int i, j;
    for (i = 0; i < count; ++i) {
        files[i].existed = me_exists(files[i].path);
        files[i].installed = 0;
        remove(files[i].backup);
        if (files[i].existed && rename(files[i].path, files[i].backup)) break;
        if (files[i].temp && rename(files[i].temp, files[i].path)) {
            if (files[i].existed) rename(files[i].backup, files[i].path);
            break;
        }
        files[i].installed = 1;
    }
    if (i != count) {
        for (j = i - 1; j >= 0; --j) {
            remove(files[j].path);
            if (files[j].existed) rename(files[j].backup, files[j].path);
        }
        for (j = 0; j < count; ++j) if (files[j].temp) remove(files[j].temp);
        return 0;
    }
    for (i = 0; i < count; ++i) remove(files[i].backup);
    return 1;
}

/* Replace exactly one known startup/driver directive, preserving owner edits. */
static int me_replace(const char *path, const char *temp, const char *match,
                      const char *alternate, const char *replacement)
{
    FILE *in = fopen(path, "rb"), *out;
    char line[1024];
    int found = 0, ok = 1;
    if (!in) return 0;
    out = fopen(temp, "wb");
    if (!out) { fclose(in); return 0; }
    while (fgets(line, sizeof(line), in)) {
        if (!strchr(line, '\n') && !feof(in)) { ok = 0; break; }
        if (!strncmp(line, match, strlen(match)) ||
            (alternate && !strncmp(line, alternate, strlen(alternate)))) {
            if (++found != 1 || fputs(replacement, out) == EOF) ok = 0;
        } else if (fputs(line, out) == EOF) ok = 0;
    }
    if (ferror(in) || ferror(out)) ok = 0;
    fclose(in);
    if (fclose(out)) ok = 0;
    if (!ok || found != 1) { remove(temp); return 0; }
    return 1;
}

static uint32_t me_le32(const unsigned char *p)
{ return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24; }
static unsigned me_le16(const unsigned char *p)
{ return p[0] | (unsigned)p[1]<<8; }

/* Output XaAES's native, headerless, full-screen MFDB, independent of host endian. */
static int me_bitmap(const char *source, const char *dest, unsigned width,
                  unsigned height, unsigned depth, unsigned char *buffer)
{
    FILE *in = fopen(source, "rb"), *out = NULL;
    unsigned char header[54], *row = NULL, *pixels = NULL;
    uint32_t offset, sw, sh, stride, sy, previous = 0xffffffffUL, bytes;
    unsigned x, y, pixelbytes, outputbytes;
    long length;
    int topdown, ok = 0;
    if (!in || (depth != 1 && depth != 16 && depth != 32) || !width || width > 1280 ||
        !height || height > 1024 || (width & 15)) goto done;
    if (fread(header, 1, 54, in) != 54 || header[0]!='B' || header[1]!='M' ||
        me_le32(header+14) < 40 || me_le16(header+26)!=1 || me_le32(header+30)) goto done;
    if (me_le16(header+28)!=24 && me_le16(header+28)!=32) goto done;
    pixelbytes = me_le16(header+28)/8;
    sw = me_le32(header+18); sh = me_le32(header+22);
    topdown = (sh & 0x80000000UL) != 0;
    if (topdown) sh = 0U-sh;
    offset = me_le32(header+10);
    if (!sw || sw > 8192 || !sh || sh > 8192 || offset < 54 || me_le32(header+14) > offset-14) goto done;
    stride = (sw * pixelbytes + 3) & ~3UL;
    bytes = stride * sh;
    if (bytes > 32UL*1024*1024 || offset > 32UL*1024*1024 ||
        fseek(in, 0, SEEK_END) || (length=ftell(in))<0 ||
        (uint32_t)length < offset + bytes) goto done;
    outputbytes = width * depth / 8;
    row = malloc(stride); pixels = malloc(outputbytes);
    if (!row || !pixels || (dest && !(out = fopen(dest, "wb")))) goto done;
    if (!dest && !buffer) goto done;
    for (y=0; y<height; ++y) {
        sy = (uint32_t)y * sh / height;
        if (!topdown) sy = sh-1-sy;
        if (sy != previous) {
            if (fseek(in, offset + sy*stride, SEEK_SET) || fread(row,1,stride,in)!=stride) goto done;
            previous = sy;
        }
        memset(pixels, 0, outputbytes);
        for (x=0; x<width; ++x) {
            const unsigned char *p = row + ((uint32_t)x * sw / width)*pixelbytes;
            if (depth == 32) {
                pixels[x*4]=0; pixels[x*4+1]=p[2]; pixels[x*4+2]=p[1]; pixels[x*4+3]=p[0];
            } else if (depth == 16) {
                unsigned c = ((unsigned)(p[2]>>3)<<11) | ((unsigned)(p[1]>>2)<<5) | (p[0]>>3);
                pixels[x*2] = c>>8; pixels[x*2+1] = c;
            } else {
                unsigned lum = ((unsigned)p[2]*77 + (unsigned)p[1]*150 + (unsigned)p[0]*29)>>8;
                unsigned threshold = 64 + ((x&1)^((y&1)<<1))*48;
                if (lum < threshold) pixels[x/8] |= 0x80U>>(x&7);
            }
        }
        if (out && fwrite(pixels,1,outputbytes,out)!=outputbytes) goto done;
        if (buffer) memcpy(buffer+(unsigned long)y*outputbytes,pixels,outputbytes);
    }
    ok = 1;
done:
    if (out && fclose(out)) ok = 0;
    if (in) fclose(in);
    free(row); free(pixels);
    if (!ok && dest) remove(dest);
    return ok;
}
static int me_bmp(const char *source, const char *dest, unsigned width,
                  unsigned height, unsigned depth)
{ return me_bitmap(source,dest,width,height,depth,NULL); }

/* Cache identity includes the full pathname, source size/mtime, geometry and
 * output depth. Cache failures never prevent previewing a readable BMP.
 * Caller creates the directory; NULL disables disk caching (read-only disks).
 */
static unsigned char *me_thumbnail(const char *source, const char *directory,
    unsigned width, unsigned height, unsigned depth)
{
    struct stat st;
    char identity[640], saved[640], data[520], info[520];
    uint32_t hash=2166136261UL;
    unsigned long bytes=(unsigned long)width*height*depth/8;
    unsigned char *pixels;
    const unsigned char *p;
    FILE *f;
    int hit=0;
    if (strlen(source)>259 || stat(source,&st) || !width || width>1280 ||
        !height || height>1024 || (width&15) || (depth!=1 && depth!=16 && depth!=32)) return NULL;
    snprintf(identity,sizeof(identity),"ME thumbnail 1\n%s\n%lu %lu %u %u %u\n",
        source,(unsigned long)st.st_size,(unsigned long)st.st_mtime,width,height,depth);
    for(p=(const unsigned char *)identity;*p;++p) hash=(hash^*p)*16777619UL;
    pixels=malloc(bytes);
    if (!pixels) return NULL;
    if(directory && strlen(directory)<490) {
        sprintf(data,"%s\\%08lx.BIN",directory,(unsigned long)hash);
        sprintf(info,"%s\\%08lx.INF",directory,(unsigned long)hash);
        f=fopen(info,"rb");
        if(f) { size_t n=fread(saved,1,sizeof(saved)-1,f); saved[n]=0; fclose(f);
            if(!strcmp(saved,identity)) {
                f=fopen(data,"rb");
                if(f) { hit=fread(pixels,1,bytes,f)==bytes && fgetc(f)==EOF && !ferror(f); fclose(f); }
            }
        }
    } else directory=NULL;
    if(!hit) {
        if(!me_bitmap(source,NULL,width,height,depth,pixels)) { free(pixels); return NULL; }
        if(directory) {
            /* Publish metadata last. An interrupted write is a cache miss. */
            remove(info);
            f=fopen(data,"wb");
            if(f) { int ok=fwrite(pixels,1,bytes,f)==bytes; if(fclose(f)) ok=0;
                if(ok && (f=fopen(info,"wb"))!=NULL) { fputs(identity,f); fclose(f); }
            }
        }
    }
    return pixels;
}
#endif
