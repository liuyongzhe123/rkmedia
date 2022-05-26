#include "milayer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <dlfcn.h>

char *pcFileName = "/root/1080p30fpsh264.h264";
int u32Loop = 0;
int u32Number = 0;

static char optstr[] = "?::i:l:n:";
static void print_usage()
{
    printf("usage example: rkmedia_vdec_test -w 720 -h 480 -i /userdata/out.jpeg "
           "-f 0 -t JPEG.\n");
    printf("\t-i: InputFilePath, Default: NULL\n");
    printf("\t-l: LoopSwitch; 0:NoLoop; 1:Loop. Default: 0.\n");
    printf("\t-n: vdec thread count: 0.\n");
}
demuxToDecodeContext demuxtodecodecontext;
pthread_t demux_capture_loop;
FILE *infile = NULL;

void *ReadFile(void *args)
{
    PacketInfo *frameInfoTmp;
    int data_size;
    int read_size;
    data_size = 1024 * 5;
    while (1)
    {
    RETRY:
        char buf[1024 * 5];
        pthread_mutex_lock(&demuxtodecodecontext.demux_to_decode_empty_queue_lock);
        int read_size = fread(buf, 1, data_size, infile);
        if (!read_size || feof(infile))
        {
            if (true)
            {
                pthread_mutex_unlock(&demuxtodecodecontext.demux_to_decode_empty_queue_lock);
                fseek(infile, 0, SEEK_SET);
                goto RETRY;
            }
            else
            {
                break;
            }
        }
        frameInfoTmp = demuxtodecodecontext.demux_to_decode_empty_queue->front();
        demuxtodecodecontext.demux_to_decode_empty_queue->pop();
        frameInfoTmp->codectype = CODEC_H264;
        frameInfoTmp->datalength = read_size;
        memcpy(frameInfoTmp->data, buf, frameInfoTmp->datalength);
        pthread_mutex_unlock(&demuxtodecodecontext.demux_to_decode_empty_queue_lock);

        pthread_mutex_lock(&demuxtodecodecontext.demux_to_decode_filled_queue_lock);
        demuxtodecodecontext.demux_to_decode_filled_queue->push(frameInfoTmp);
        pthread_mutex_unlock(&demuxtodecodecontext.demux_to_decode_filled_queue_lock);
        usleep(30 * 1000);
    }
}

int main(int argc, char *argv[])
{
    int c, ret;
    while ((c = getopt(argc, argv, optstr)) != -1)
    {
        switch (c)
        {
        case 'i':
            pcFileName = optarg;
            break;
        default:
            print_usage();
            return 0;
        }
    }

    infile = fopen(pcFileName, "rb");
    if (!infile)
    {
        fprintf(stderr, "Could not open %s\n", pcFileName);
        return NULL;
    }

    FrameInfo *frameinfo = (FrameInfo *)malloc(sizeof(FrameInfo));
    int res = -1;
    demuxtodecodecontext.demux_to_decode_empty_queue = new std::queue<PacketInfo *>;
    demuxtodecodecontext.demux_to_decode_filled_queue = new std::queue<PacketInfo *>;
    pthread_mutex_init(&demuxtodecodecontext.demux_to_decode_empty_queue_lock, NULL);
    pthread_cond_init(&demuxtodecodecontext.demux_to_decode_empty_queue_cond, NULL);
    pthread_mutex_init(&demuxtodecodecontext.demux_to_decode_filled_queue_lock, NULL);
    pthread_cond_init(&demuxtodecodecontext.demux_to_decode_filled_queue_cond, NULL);

    PacketInfo *packetInfo;
    for (int i = 0; i < 10; i++)
    {
        packetInfo = new PacketInfo;
        packetInfo->data = (unsigned char *)malloc(1024 * 512 * sizeof(unsigned char));
        demuxtodecodecontext.demux_to_decode_empty_queue->push(packetInfo);
    }

    VdecChnCreate(0, CODEC_H264, &demuxtodecodecontext);
    sleep(1);
    pthread_create(&demux_capture_loop, NULL, ReadFile, NULL);

    while (1)
    {
        frameinfo->rawdata = (unsigned char *)malloc(1920 * 1080 * 3);
        res = VdecGetFrame(0, frameinfo);
        if (res == 0)
        {
            FILE *fp = fopen("./1.nv12", "wb");
            fwrite(frameinfo->rawdata, frameinfo->size.width * frameinfo->size.height * 3 / 2, 1, fp);
            fclose(fp);
            break;
        }
    }

    printf("-----------------------------------------\n");
    FILE *fp1 = fopen("./in-320X240.nv12", "rb");
    FrameInfo *frameinfo1 = (FrameInfo *)malloc(sizeof(FrameInfo));
    frameinfo1->size.width = 320;
    frameinfo1->size.height = 240;
    frameinfo1->pixelformat = PIXEL_FORMAT_NV12;
    frameinfo1->rawdata = (unsigned char *)malloc(320 * 244 * 3);
    fread(frameinfo1->rawdata, 320 * 240 * 3 / 2, 1, fp1);
    fclose(fp1);

    JpegInfo *jpeginfo = (JpegInfo *)malloc(sizeof(JpegInfo));
    jpeginfo->jpegdata = (unsigned char *)malloc(320 * 244 * 3);

    SaveImage(frameinfo1, jpeginfo);
    FILE *fpjpg = fopen("./1.jpg", "wb");
    fwrite(jpeginfo->jpegdata, jpeginfo->jpeglength, 1, fpjpg);
    fclose(fpjpg);

    printf("-----------------------------------------\n");
    ReSizeRect dsize = {0};
    dsize.width = 960;
    dsize.height = 540;
    FrameInfo *dstframeinfo = (FrameInfo *)malloc(sizeof(FrameInfo));
    dstframeinfo->rawdata = (unsigned char *)malloc(960 * 540 * 3);
    ResizeFrame(frameinfo1, dsize, dstframeinfo);

    FILE *fpresize = fopen("./reszie.nv12", "wb");
    fwrite(dstframeinfo->rawdata, dsize.width * dsize.height * 3 / 2, 1, fpresize);
    fclose(fpresize);
    printf("resize nv12 end\n");

    printf("-----------------------------------------\n");
    CropRect rect = {0};
    rect.upperleftx = 100;
    rect.upperlefty = 100;
    rect.width = 100;
    rect.height = 100;
    CropFrame(frameinfo1, rect, dstframeinfo);
    FILE *fpcrop = fopen("./crop.nv12", "wb");
    fwrite(dstframeinfo->rawdata, rect.width * rect.height * 3 / 2, 1, fpcrop);
    fclose(fpcrop);
    printf("crop nv12 end\n");

    printf("-----------------------------------------\n");
    // int pixelformat = 512;
    int pixelformat = PIXEL_FORMAT_BGR;
    PixelFormatFrame(frameinfo1, pixelformat, dstframeinfo);
    FILE *fppixel = fopen("./pixel.bgr", "wb");
    fwrite(dstframeinfo->rawdata, frameinfo1->size.width * frameinfo1->size.height * 3, 1, fppixel);
    fclose(fppixel);
    printf("pixel end\n");
}
