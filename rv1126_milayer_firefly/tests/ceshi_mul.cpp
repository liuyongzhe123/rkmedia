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
demuxToDecodeContext demuxtodecodecontext[4];

void *ReadFile(void *args)
{
    int index = *((int *)args);
    printf("index= %d\n", index);
    FILE *infile = NULL;
    infile = fopen(pcFileName, "rb");
    if (!infile)
    {
        fprintf(stderr, "Could not open %s\n", pcFileName);
        return NULL;
    }
    PacketInfo *frameInfoTmp;
    int data_size;
    int read_size;
    data_size = 1024 * 4;
    while (1)
    {
    RETRY:
        char buf[1024 * 4];
        pthread_mutex_lock(&demuxtodecodecontext[index].demux_to_decode_empty_queue_lock);
        int read_size = fread(buf, 1, data_size, infile);
        if (!read_size || feof(infile))
        {
            if (true)
            {
                pthread_mutex_unlock(&demuxtodecodecontext[index].demux_to_decode_empty_queue_lock);
                fseek(infile, 0, SEEK_SET);
                goto RETRY;
            }
            else
            {
                break;
            }
        }
        frameInfoTmp = demuxtodecodecontext[index].demux_to_decode_empty_queue->front();
        demuxtodecodecontext[index].demux_to_decode_empty_queue->pop();
        frameInfoTmp->codectype = CODEC_H264;
        frameInfoTmp->datalength = read_size;
        memcpy(frameInfoTmp->data, buf, frameInfoTmp->datalength);
        pthread_mutex_unlock(&demuxtodecodecontext[index].demux_to_decode_empty_queue_lock);

        pthread_mutex_lock(&demuxtodecodecontext[index].demux_to_decode_filled_queue_lock);
        demuxtodecodecontext[index].demux_to_decode_filled_queue->push(frameInfoTmp);
        pthread_mutex_unlock(&demuxtodecodecontext[index].demux_to_decode_filled_queue_lock);
        usleep(30 * 1000);
    }
    return NULL;
}

void *savejpg(void *args)
{
    int res = -1;
    int index = *((int *)args);
    JpegInfo *jpeginfo = (JpegInfo *)malloc(sizeof(JpegInfo));
    jpeginfo->jpegdata = (unsigned char *)malloc(1920 * 1080 * 3);

    FrameInfo *frameinfo = (FrameInfo *)malloc(sizeof(FrameInfo));
    frameinfo->rawdata = (unsigned char *)malloc(1920 * 1080 * 3);
    while (1)
    {
        res = VdecGetFrame(index, frameinfo);
	
        if (res == 0)
        {
            SaveImage(frameinfo, jpeginfo);
            FILE *fpjpg = fopen("./1.jpg", "wb");
            fwrite(jpeginfo->jpegdata, jpeginfo->jpeglength, 1, fpjpg);
            fclose(fpjpg);
            printf("index= %d\n", index);
        }
    }
    return NULL;
}

int main(int argc, char *argv[])
{
    int c, ret;
    int threadindex[4] = {0};
    int savejpgthreadindex[4] = {0};
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

    int res = -1;
    FrameInfo *frameinfo = (FrameInfo *)malloc(sizeof(FrameInfo));
    frameinfo->rawdata = (unsigned char *)malloc(1920 * 1080 * 3);
    for (int i = 0; i < 4; i++)
    {
        threadindex[i] = i;
        demuxtodecodecontext[i].demux_to_decode_empty_queue = new std::queue<PacketInfo *>;
        demuxtodecodecontext[i].demux_to_decode_filled_queue = new std::queue<PacketInfo *>;
        pthread_mutex_init(&demuxtodecodecontext[i].demux_to_decode_empty_queue_lock, NULL);
        pthread_cond_init(&demuxtodecodecontext[i].demux_to_decode_empty_queue_cond, NULL);
        pthread_mutex_init(&demuxtodecodecontext[i].demux_to_decode_filled_queue_lock, NULL);
        pthread_cond_init(&demuxtodecodecontext[i].demux_to_decode_filled_queue_cond, NULL);

        PacketInfo *packetInfo;
        for (int j = 0; j < 10; j++)
        {
            packetInfo = new PacketInfo;
            packetInfo->data = (unsigned char *)malloc(1024 * 512 * sizeof(unsigned char));
            demuxtodecodecontext[i].demux_to_decode_empty_queue->push(packetInfo);
        }

        VdecChnCreate(i, CODEC_H264, &demuxtodecodecontext[i]);
        sleep(1);
        pthread_t demux_capture_loop;
        pthread_create(&demux_capture_loop, NULL, ReadFile, &threadindex[i]);
    }

    for (int i = 0; i < 4; i++)
    {
        savejpgthreadindex[i] = i;
        sleep(1);
        pthread_t savejpghandle;
        pthread_create(&savejpghandle, NULL, savejpg, &savejpgthreadindex[i]);
        printf("i=%d\n", i);
    }

    while (1)
    {
        sleep(1);
    }

    return 0;
}
