#include "milayer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <dlfcn.h>

FrameInfo * frameinfo1 = NULL;
void *savejpg(void *args)
{
	    int index = *((int *)args);
  JpegInfo *jpeginfo = (JpegInfo *)malloc(sizeof(JpegInfo));
    jpeginfo->jpegdata = (unsigned char *)malloc(1920 * 1080 * 3);
   while(1)
   {
    SaveImage(frameinfo1, jpeginfo);
    FILE *fpjpg = fopen("./1.jpg", "wb");
    fwrite(jpeginfo->jpegdata, jpeginfo->jpeglength, 1, fpjpg);
    fclose(fpjpg);
    printf("index= %d\n", index);
   }
   return NULL;

}

int main(int argc, const char *argv[])
{
	int threadindex[4]={0};
	    FILE *fp1 = fopen(argv[1], "rb");
    frameinfo1 = (FrameInfo *)malloc(sizeof(FrameInfo));
    frameinfo1->size.width = 1920;
    frameinfo1->size.height = 1080;
    frameinfo1->pixelformat = PIXEL_FORMAT_NV12;
    frameinfo1->rawdata = (unsigned char *)malloc(1920 * 1080 * 3);
    fread(frameinfo1->rawdata, 1920 * 1080 * 3 / 2, 1, fp1);
    fclose(fp1);

    for (int i = 0; i < 4; i++)
    {
        threadindex[i] = i;
        sleep(1);
        pthread_t demux_capture_loop;
        pthread_create(&demux_capture_loop, NULL, savejpg, &threadindex[i]);
	printf("i=%d\n", i);
    }



    while(1)
    {
	    sleep(1);
    }
}
