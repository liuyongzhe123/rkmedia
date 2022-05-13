#include <assert.h>
#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "im2d.h"
#include "rga.h"
#include "rkmedia_api.h"
#include "rkmedia_vdec.h"
#include "rkmedia_venc.h"
#include "milayer.h"
#include "milog.h"

#define VDEC_CHANNEL_COUNT 4
#define VDEC_CHANNEL_GETDATA_TIMEOUTMS 500
#define VDEC_RAWDATA_LENGTH (1920 * 1080 * 3 * 4)
#define PIXEL_ALIGNMENT_STEPSIZE 4

typedef struct _VdecContext
{
    int channelid;
    int codectype;
    pthread_t vdecthreadhandle;
    bool vdecthreadexit;
    demuxToDecodeContext *demuxtodecodecontext;
    unsigned char *rawdata;
} VdecContext;

VdecContext gVdecContext[4] = {0};
bool gRKSysInit = false;
pthread_mutex_t gMutex = PTHREAD_MUTEX_INITIALIZER;

void *VdecThread(void *args)
{
    int ret = MI_SUCESS;
    //默认硬解码
    RK_BOOL bIsHardware = RK_TRUE;
    VDEC_CHN_ATTR_S stVdecAttr;
    VdecContext *vdeccontext = (VdecContext *)args;
    vdeccontext->vdecthreadexit = true;
    stVdecAttr.enCodecType = (CODEC_TYPE_E)vdeccontext->codectype;
    stVdecAttr.enMode = VIDEO_MODE_FRAME;
    if (bIsHardware)
    {
        if (stVdecAttr.enCodecType == RK_CODEC_TYPE_JPEG)
        {
            stVdecAttr.enMode = VIDEO_MODE_FRAME;
        }
        else
        {
            stVdecAttr.enMode = VIDEO_MODE_STREAM;
        }
        stVdecAttr.enDecodecMode = VIDEO_DECODEC_HADRWARE;
    }
    else
    {
        stVdecAttr.enMode = VIDEO_MODE_STREAM;
        stVdecAttr.enDecodecMode = VIDEO_DECODEC_SOFTWARE;
    }

    ret = RK_MPI_VDEC_CreateChn(vdeccontext->channelid, &stVdecAttr);
    if (ret)
    {
        LogE("Create Vdec[%d] codectype[%d] failed! ret=%d\n", vdeccontext->channelid, vdeccontext->codectype, ret);
        return NULL;
    }
    else
    {
        LogI("Create Vdec[%d] codectype[%d] success\n", vdeccontext->channelid, vdeccontext->codectype);
    }

    while (vdeccontext->vdecthreadexit)
    {
        pthread_mutex_lock(&vdeccontext->demuxtodecodecontext->demux_to_decode_filled_queue_lock);
        if (vdeccontext->demuxtodecodecontext->demux_to_decode_filled_queue->empty())
        {
            pthread_mutex_unlock(&vdeccontext->demuxtodecodecontext->demux_to_decode_filled_queue_lock);
            usleep(5 * 1000);
            continue;
        }
        PacketInfo *packetinfo = vdeccontext->demuxtodecodecontext->demux_to_decode_filled_queue->front();
        MEDIA_BUFFER packetmb = RK_MPI_MB_CreateBuffer(packetinfo->datalength, RK_FALSE, 0);
        if (!packetmb)
        {
            LogE("RK_MPI_MB_CreateBuffer failed\n");
        }
        memcpy(RK_MPI_MB_GetPtr(packetmb), packetinfo->data, packetinfo->datalength);
        RK_MPI_MB_SetSize(packetmb, packetinfo->datalength);
        vdeccontext->demuxtodecodecontext->demux_to_decode_filled_queue->pop();
        pthread_mutex_unlock(&vdeccontext->demuxtodecodecontext->demux_to_decode_filled_queue_lock);

        // 如果解码类型发生变化
        if (vdeccontext->codectype != packetinfo->codectype)
        {
            ret = RK_MPI_VDEC_DestroyChn(vdeccontext->channelid);
            if (ret)
            {
                LogE("Destroy Vdec[%d] codectype[%d] failed! ret=%d\n", vdeccontext->channelid, vdeccontext->codectype, ret);
                return NULL;
            }
            else
            {
                LogE("Destroy Vdec[%d] codectype[%d] success\n", vdeccontext->channelid, vdeccontext->codectype);
                vdeccontext->codectype = packetinfo->codectype;
                stVdecAttr.enCodecType = (CODEC_TYPE_E)vdeccontext->codectype;
                ret = RK_MPI_VDEC_CreateChn(vdeccontext->channelid, &stVdecAttr);
                if (ret)
                {
                    LogE("Create Vdec[%d] codectype[%d] failed! ret=%d\n", vdeccontext->channelid, vdeccontext->codectype, ret);
                    return NULL;
                }
                else
                {
                    LogE("Create Vdec[%d] codectype[%d] success\n", vdeccontext->channelid, vdeccontext->codectype);
                }
            }
        }

        ret = RK_MPI_SYS_SendMediaBuffer(RK_ID_VDEC, vdeccontext->channelid, packetmb);
        if (ret)
        {
            LogE("RK_MPI_SYS_SendMediaBuffer failed ret = %d\n", ret);
        }
        RK_MPI_MB_ReleaseBuffer(packetmb);

        //用完之后再push
        pthread_mutex_lock(&vdeccontext->demuxtodecodecontext->demux_to_decode_empty_queue_lock);
        vdeccontext->demuxtodecodecontext->demux_to_decode_empty_queue->push(packetinfo);
        pthread_mutex_unlock(&vdeccontext->demuxtodecodecontext->demux_to_decode_empty_queue_lock);
    }
    return NULL;
}

int VdecChnCreate(int channelid, int codectype, demuxToDecodeContext *demuxtodecodecontext)
{
    int ret = MI_SUCESS;
    if (gRKSysInit == false)
    {
        RK_MPI_SYS_Init();
        LOG_LEVEL_CONF_S pstConf;
        pstConf.s32Level = 0;
        strcpy(pstConf.cModName, "all");
        RK_MPI_LOG_SetLevelConf(&pstConf);
        gRKSysInit = true;
        CreateLogFile("milayer", "/tmp");
    }

    if (channelid < 0 || channelid >= VDEC_CHANNEL_COUNT)
    {
        LogE("channelid is invalid\n");
        return MI_ERROR_PARAM_INVALID;
    }

    if (gVdecContext[channelid].vdecthreadexit == false)
    {
        gVdecContext[channelid].channelid = channelid;
        pthread_mutex_lock(&gMutex);
        gVdecContext[channelid].codectype = codectype;
        pthread_mutex_unlock(&gMutex);
        gVdecContext[channelid].demuxtodecodecontext = demuxtodecodecontext;
        gVdecContext[channelid].rawdata = (unsigned char *)malloc(VDEC_RAWDATA_LENGTH);
        if (gVdecContext[channelid].rawdata == NULL)
        {
            LogE("channelid = %d malloc falied\n", channelid);
            return MI_ERROR_NOMEM;
        }
        ret = pthread_create(&gVdecContext[channelid].vdecthreadhandle, NULL, VdecThread, &gVdecContext[channelid]);
        if (ret < 0)
        {
            LogE("VdecThread channelid = %d is failed\n", channelid);
            return MI_ERROR_NOMEM;
        }
    }

    return ret;
}

int VdecGetFrame(int channelid, FrameInfo *frameinfo)
{
    if (frameinfo->rawdata == NULL)
    {
        LogE("frameinfo is invalid\n");
        return MI_ERROR_PARAM_INVALID;
    }
    int ret = MI_SUCESS;
    MEDIA_BUFFER framemb;
    MB_IMAGE_INFO_S stImageInfo;

    framemb = RK_MPI_SYS_GetMediaBuffer(RK_ID_VDEC, channelid, VDEC_CHANNEL_GETDATA_TIMEOUTMS);
    if (!framemb)
    {
        LogE("RK_MPI_SYS_GetMediaBuffer get null buffer in %d ms\n", VDEC_CHANNEL_GETDATA_TIMEOUTMS);
        return MI_ERROR_TIMEOUT;
    }
    ret = RK_MPI_MB_GetImageInfo(framemb, &stImageInfo);
    if (ret)
    {
        LogE("Get image info failed! ret = %d\n", ret);
        RK_MPI_MB_ReleaseBuffer(framemb);
    }
    else
    {
        // LogE("Get Frame:ptr:%p, fd:%d, size:%zu, mode:%d, channel:%d, "
        //        "timestamp:%lld, ImgInfo:<wxh %dx%d, %dx%d, fmt 0x%x>\n",
        //        RK_MPI_MB_GetPtr(framemb), RK_MPI_MB_GetFD(framemb), RK_MPI_MB_GetSize(framemb),
        //        RK_MPI_MB_GetModeID(framemb), RK_MPI_MB_GetChannelID(framemb),
        //        RK_MPI_MB_GetTimestamp(framemb), stImageInfo.u32Width,
        //        stImageInfo.u32Height, stImageInfo.u32HorStride,
        //        stImageInfo.u32VerStride, stImageInfo.enImgType);
        if (RK_MPI_MB_GetPtr(framemb) == NULL || RK_MPI_MB_GetSize(framemb) == 0)
        {
            RK_MPI_MB_ReleaseBuffer(framemb);
            return MI_ERROR_PARAM_INVALID;
        }
        else
        {
            //1080P的h265需要做裁剪
            if (gVdecContext[channelid].codectype == CODEC_H265 &&
                stImageInfo.u32Width == 1920 &&
                stImageInfo.u32Height == 1080 &&
                (stImageInfo.u32Width != stImageInfo.u32HorStride/*2304*/))
            {
                FrameInfo srcframeinfo;
                srcframeinfo.pixelformat = PIXEL_FORMAT_NV12;
                srcframeinfo.rawdata = (unsigned char *)RK_MPI_MB_GetPtr(framemb);
                srcframeinfo.size.width = stImageInfo.u32HorStride;
                srcframeinfo.size.height = stImageInfo.u32VerStride;
                CropRect rect = {0, 0, 1920, 1080};
                FrameInfo dstframeinfo;
                dstframeinfo.rawdata = (unsigned char *)malloc(1920 * 1080 * 3 / 2);
                if (dstframeinfo.rawdata == NULL)
                {
                    LogE("dstframeinfo.rawdata malloc failed\n");
                    return MI_ERROR_NOMEM;
                }
                CropFrame(&srcframeinfo, rect, &dstframeinfo);
                memcpy(frameinfo->rawdata, dstframeinfo.rawdata, 1920 * 1080 * 3 / 2);
                if (dstframeinfo.rawdata == NULL)
                {
                    free(dstframeinfo.rawdata);
                    dstframeinfo.rawdata = NULL;
                }
            }
            else
            {
                memcpy(frameinfo->rawdata, RK_MPI_MB_GetPtr(framemb), RK_MPI_MB_GetSize(framemb));
            }    
            frameinfo->pixelformat = PIXEL_FORMAT_NV12;
            frameinfo->size.width = stImageInfo.u32Width;
            frameinfo->size.height = stImageInfo.u32Height;
            frameinfo->timestamp = RK_MPI_MB_GetTimestamp(framemb);
            RK_MPI_MB_ReleaseBuffer(framemb);
        }
    }

    return ret;
}

//支持等比例缩放
int ResizeFrame(FrameInfo *srcframeinfo, ReSizeRect dsize, FrameInfo *dstframeinfo)
{
    if ((srcframeinfo->pixelformat != PIXEL_FORMAT_NV12 &&
         srcframeinfo->pixelformat != PIXEL_FORMAT_BGR &&
         srcframeinfo->pixelformat != PIXEL_FORMAT_RGB) ||
        srcframeinfo->rawdata == NULL ||
        srcframeinfo->size.width <= 0 ||
        srcframeinfo->size.height <= 0 ||
        dsize.width <= 0 ||
        dsize.height <= 0 ||
        dstframeinfo->rawdata == NULL)
    {
        LogE("srcframeinfo is invalid\n");
        return MI_ERROR_PARAM_INVALID;
    }

    int ret = MI_SUCESS;
    rga_buffer_t src;
    rga_buffer_t dst;
    int extendedlength = 0;
    unsigned char *tmprawdata = NULL;
    float srcratio = srcframeinfo->size.width / (float)srcframeinfo->size.height;
    float dstratio = dsize.width / (float)dsize.height;
    int extendedwidth = srcframeinfo->size.width;
    int extendedheight = srcframeinfo->size.height;
    //需要扩充下侧
    if (srcratio > dstratio)
    {
        LogI("需要扩充下侧\n");
        extendedlength = srcframeinfo->size.width / dstratio - srcframeinfo->size.height;
        extendedheight = (extendedlength + srcframeinfo->size.height) / PIXEL_ALIGNMENT_STEPSIZE * PIXEL_ALIGNMENT_STEPSIZE;
        if (srcframeinfo->pixelformat == PIXEL_FORMAT_NV12)
        {
            tmprawdata = (unsigned char *)malloc(srcframeinfo->size.width * extendedheight * 3 / 2);
            for (int i = 0; i < extendedheight; i++)
            {
                for (int j = 0; j < srcframeinfo->size.width; j++)
                {
                    if (i >= srcframeinfo->size.height)
                    {
                        *(tmprawdata + i * srcframeinfo->size.width + j) = 0;
                    }
                    else
                    {
                        *(tmprawdata + i * srcframeinfo->size.width + j) = *(srcframeinfo->rawdata + i * srcframeinfo->size.width + j);
                    }
                }
            }

            int tmpextendedheight = extendedheight;
            for (int i = srcframeinfo->size.height; i < extendedheight * 3 / 2; i++)
            {
                for (int j = 0; j < srcframeinfo->size.width; j++)
                {
                    if (i >= srcframeinfo->size.height * 3 / 2)
                    {
                        *(tmprawdata + j + tmpextendedheight * srcframeinfo->size.width) = 0;
                    }
                    else
                    {
                        *(tmprawdata + j + tmpextendedheight * srcframeinfo->size.width) =
                            *(srcframeinfo->rawdata + j + i * srcframeinfo->size.width);
                    }
                }
                tmpextendedheight++;
            }
        }
        else
        {
            tmprawdata = (unsigned char *)malloc(srcframeinfo->size.width * extendedheight * 3);
            for (int i = 0; i < extendedheight; i++)
            {
                for (int j = 0; j < srcframeinfo->size.width * 3; j = j + 3)
                {
                    if (i >= srcframeinfo->size.height)
                    {
                        *(tmprawdata + i * srcframeinfo->size.width * 3 + j) = 0;
                        *(tmprawdata + i * srcframeinfo->size.width * 3 + j + 1) = 0;
                        *(tmprawdata + i * srcframeinfo->size.width * 3 + j + 2) = 0;
                    }
                    else
                    {
                        *(tmprawdata + i * srcframeinfo->size.width * 3 + j) =
                            *(srcframeinfo->rawdata + i * srcframeinfo->size.width * 3 + j);
                        *(tmprawdata + i * srcframeinfo->size.width * 3 + j + 1) =
                            *(srcframeinfo->rawdata + i * srcframeinfo->size.width * 3 + j + 1);
                        *(tmprawdata + i * srcframeinfo->size.width * 3 + j + 2) =
                            *(srcframeinfo->rawdata + i * srcframeinfo->size.width * 3 + j + 2);
                    }
                }
            }
        }
    }
    //需要扩充右侧
    else if (srcratio < dstratio)
    {
        LogI("需要扩充右侧\n");
        extendedlength = dstratio * srcframeinfo->size.height - srcframeinfo->size.width;
        extendedwidth = (extendedlength + srcframeinfo->size.width) / PIXEL_ALIGNMENT_STEPSIZE * PIXEL_ALIGNMENT_STEPSIZE;
        if (srcframeinfo->pixelformat == PIXEL_FORMAT_NV12)
        {
            tmprawdata = (unsigned char *)malloc(srcframeinfo->size.height * extendedwidth * 3 / 2);
            for (int i = 0; i < srcframeinfo->size.height * 3 / 2; i++)
            {
                for (int j = 0; j < extendedwidth; j++)
                {
                    if (j >= srcframeinfo->size.width)
                    {
                        *(tmprawdata + j + i * extendedwidth) = 0;
                    }
                    else
                    {
                        *(tmprawdata + j + i * extendedwidth) = *(srcframeinfo->rawdata + j + i * srcframeinfo->size.width);
                    }
                }
            }
        }
        else
        {
            tmprawdata = (unsigned char *)malloc(srcframeinfo->size.height * extendedwidth * 3);
            for (int i = 0; i < srcframeinfo->size.height; i++)
            {
                for (int j = 0; j < extendedwidth * 3; j = j + 3)
                {
                    if (j >= srcframeinfo->size.width * 3)
                    {
                        *(tmprawdata + i * extendedwidth * 3 + j) = 0;
                        *(tmprawdata + i * extendedwidth * 3 + j + 1) = 0;
                        *(tmprawdata + i * extendedwidth * 3 + j + 2) = 0;
                    }
                    else
                    {
                        *(tmprawdata + i * extendedwidth * 3 + j) = *(srcframeinfo->rawdata + i * srcframeinfo->size.width * 3 + j);
                        *(tmprawdata + i * extendedwidth * 3 + j + 1) = *(srcframeinfo->rawdata + i * srcframeinfo->size.width * 3 + j + 1);
                        *(tmprawdata + i * extendedwidth * 3 + j + 2) = *(srcframeinfo->rawdata + i * srcframeinfo->size.width * 3 + j + 2);
                    }
                }
            }
        }
    }
    else
    {
        LogI("默认等比例缩放\n");
    }

    if (srcratio != dstratio)
    {
        src = wrapbuffer_virtualaddr(tmprawdata, extendedwidth, extendedheight, srcframeinfo->pixelformat);
    }
    else
    {
        src = wrapbuffer_virtualaddr(srcframeinfo->rawdata, extendedwidth, extendedheight, srcframeinfo->pixelformat);
    }
    dst = wrapbuffer_virtualaddr(dstframeinfo->rawdata, dsize.width,
                                 dsize.height, srcframeinfo->pixelformat);

    IM_STATUS STATUS = imresize(src, dst);
    if (STATUS != IM_STATUS_SUCCESS)
    {
        LogE("imcrop failed: %s\n", imStrError(STATUS));
        return STATUS;
    }

    dstframeinfo->pixelformat = srcframeinfo->pixelformat;
    dstframeinfo->size.width = dsize.width;
    dstframeinfo->size.height = dsize.height;
    dstframeinfo->timestamp = srcframeinfo->timestamp;

    if (tmprawdata != NULL)
    {
        free(tmprawdata);
        tmprawdata = NULL;
    }

    return ret;
}

int CropFrame(FrameInfo *srcframeinfo, CropRect rect, FrameInfo *dstframeinfo)
{
    if ((srcframeinfo->pixelformat != PIXEL_FORMAT_NV12 &&
         srcframeinfo->pixelformat != PIXEL_FORMAT_BGR &&
         srcframeinfo->pixelformat != PIXEL_FORMAT_RGB) ||
        srcframeinfo->rawdata == NULL ||
        srcframeinfo->size.width <= 0 ||
        srcframeinfo->size.height <= 0 ||
        rect.width <= 0 ||
        rect.height <= 0 ||
        dstframeinfo->rawdata == NULL)
    {
        LogE("srcframeinfo is invalid\n");
        return MI_ERROR_PARAM_INVALID;
    }

    int ret = MI_SUCESS;
    rga_buffer_t src;
    rga_buffer_t dst;

    src = wrapbuffer_virtualaddr(srcframeinfo->rawdata, srcframeinfo->size.width,
                                 srcframeinfo->size.height, srcframeinfo->pixelformat);
    dst = wrapbuffer_virtualaddr(dstframeinfo->rawdata, rect.width,
                                 rect.height, srcframeinfo->pixelformat);
    im_rect src_rect = {rect.upperleftx, rect.upperlefty, rect.width, rect.height};
    im_rect dst_rect = {0};
    ret = imcheck(src, dst, src_rect, dst_rect, IM_CROP);
    if (IM_STATUS_NOERROR != ret)
    {
        LogE("%d, check error! %s", __LINE__, imStrError((IM_STATUS)ret));
        return ret;
    }
    IM_STATUS STATUS = imcrop(src, dst, src_rect);
    if (STATUS != IM_STATUS_SUCCESS)
    {
        LogE("imcrop failed: %s\n", imStrError(STATUS));
        return STATUS;
    }

    dstframeinfo->pixelformat = srcframeinfo->pixelformat;
    dstframeinfo->size.width = rect.width;
    dstframeinfo->size.height = rect.height;
    dstframeinfo->timestamp = srcframeinfo->timestamp;

    return ret;
}

int PixelFormatFrame(FrameInfo *srcframeinfo, int pixelformat, FrameInfo *dstframeinfo)
{
    if ((srcframeinfo->pixelformat != PIXEL_FORMAT_NV12 &&
         srcframeinfo->pixelformat != PIXEL_FORMAT_BGR &&
         srcframeinfo->pixelformat != PIXEL_FORMAT_RGB) ||
        srcframeinfo->rawdata == NULL ||
        srcframeinfo->size.width <= 0 ||
        srcframeinfo->size.height <= 0 ||
        (pixelformat != PIXEL_FORMAT_NV12 &&
         pixelformat != PIXEL_FORMAT_BGR &&
         pixelformat != PIXEL_FORMAT_RGB) ||
        dstframeinfo->rawdata == NULL)
    {
        LogE("srcframeinfo is invalid\n");
        return MI_ERROR_PARAM_INVALID;
    }
    int ret = MI_SUCESS;
    rga_buffer_t src;
    rga_buffer_t dst;

    src = wrapbuffer_virtualaddr(srcframeinfo->rawdata, srcframeinfo->size.width,
                                 srcframeinfo->size.height, srcframeinfo->pixelformat);
    dst = wrapbuffer_virtualaddr(dstframeinfo->rawdata, srcframeinfo->size.width,
                                 srcframeinfo->size.height, pixelformat);
    IM_STATUS STATUS = imcvtcolor(src, dst, srcframeinfo->pixelformat, pixelformat);
    if (STATUS != IM_STATUS_SUCCESS)
    {
        LogE("imcrop failed: %s\n", imStrError(STATUS));
        return STATUS;
    }
    dstframeinfo->pixelformat = pixelformat;
    dstframeinfo->size.width = srcframeinfo->size.width;
    dstframeinfo->size.height = srcframeinfo->size.height;
    dstframeinfo->timestamp = srcframeinfo->timestamp;

    return ret;
}

int SaveImage(FrameInfo *srcframeinfo, JpegInfo *jpeginfo)
{
    if ((srcframeinfo->pixelformat != PIXEL_FORMAT_NV12 &&
         srcframeinfo->pixelformat != PIXEL_FORMAT_BGR &&
         srcframeinfo->pixelformat != PIXEL_FORMAT_RGB) ||
        srcframeinfo->rawdata == NULL ||
        srcframeinfo->size.width <= 0 ||
        srcframeinfo->size.height <= 0 ||
        jpeginfo->jpegdata == NULL)
    {
        LogE("srcframeinfo is invalid\n");
        return MI_ERROR_PARAM_INVALID;
    }

    int ret = MI_SUCESS;
    RK_U32 u32Fps = 30;
    RK_U32 u32Width = srcframeinfo->size.width;
    RK_U32 u32Height = srcframeinfo->size.height;
    VENC_CHN_ATTR_S venc_chn_attr;

    //必须初始化, 否则编码失败
    memset(&venc_chn_attr, 0, sizeof(VENC_CHN_ATTR_S));
    venc_chn_attr.stVencAttr.u32PicWidth = u32Width;
    venc_chn_attr.stVencAttr.u32PicHeight = u32Height;
    venc_chn_attr.stVencAttr.u32VirWidth = u32Width;
    venc_chn_attr.stVencAttr.u32VirHeight = u32Height;
    if (srcframeinfo->pixelformat == PIXEL_FORMAT_NV12)
    {
        venc_chn_attr.stVencAttr.imageType = IMAGE_TYPE_NV12;
    }
    else if (srcframeinfo->pixelformat == PIXEL_FORMAT_BGR)
    {
        venc_chn_attr.stVencAttr.imageType = IMAGE_TYPE_BGR888;
    }
    else if (srcframeinfo->pixelformat == PIXEL_FORMAT_RGB)
    {
        venc_chn_attr.stVencAttr.imageType = IMAGE_TYPE_RGB888;
    }
    venc_chn_attr.stVencAttr.enType = RK_CODEC_TYPE_MJPEG;
    venc_chn_attr.stRcAttr.enRcMode = VENC_RC_MODE_MJPEGCBR;
    venc_chn_attr.stRcAttr.stMjpegCbr.fr32DstFrameRateDen = 1;
    venc_chn_attr.stRcAttr.stMjpegCbr.fr32DstFrameRateNum = u32Fps;
    venc_chn_attr.stRcAttr.stMjpegCbr.u32SrcFrameRateDen = 1;
    venc_chn_attr.stRcAttr.stMjpegCbr.u32SrcFrameRateNum = u32Fps;
    venc_chn_attr.stRcAttr.stMjpegCbr.u32BitRate = u32Width * u32Height * 8;

    RK_MPI_SYS_Init();

    ret = RK_MPI_VENC_CreateChn(0, &venc_chn_attr);
    if (ret)
    {
        LogE("Create Venc(JPEG) failed! ret=%d\n", ret);
        return ret;
    }

    MB_IMAGE_INFO_S stImageInfo = {u32Width, u32Height, u32Width, u32Height, venc_chn_attr.stVencAttr.imageType};
    MEDIA_BUFFER mb = RK_MPI_MB_CreateImageBuffer(&stImageInfo, RK_TRUE, MB_FLAG_NOCACHED);
    if (!mb)
    {
        LogE("ERROR: no space left!\n");
        return -1;
    }
    RK_S32 s32FrameSize = 0;
    RK_U32 u32FrameId = 0;
    RK_U64 u64TimePeriod = 1000000 / u32Fps;
    s32FrameSize = u32Width * u32Height * 3 / 2;
    memcpy(RK_MPI_MB_GetPtr(mb), srcframeinfo->rawdata, s32FrameSize);
    RK_MPI_MB_SetSize(mb, s32FrameSize);
    RK_MPI_MB_SetTimestamp(mb, u32FrameId * u64TimePeriod);

    //通过编码通道送数据
    ret = RK_MPI_SYS_SendMediaBuffer(RK_ID_VENC, 0, mb);
    if (ret)
    {
        LogE("RK_ID_VENC RK_MPI_SYS_SendMediaBuffer failed ret = %d\n", ret);
    }
    RK_MPI_MB_ReleaseBuffer(mb);

    //通过编码通道获取编码之后的数据
    MEDIA_BUFFER jpegmb = NULL;
    jpegmb = RK_MPI_SYS_GetMediaBuffer(RK_ID_VENC, 0, -1);
    if (!jpegmb)
    {
        LogE("RK_MPI_SYS_GetMediaBuffer get null buffer!\n");
        return -1;
    }
    // LogI("Get packet:ptr:%p, fd:%d, size:%zu, mode:%d, channel:%d, "
    //        "timestamp:%lld\n",
    //        RK_MPI_MB_GetPtr(jpegmb), RK_MPI_MB_GetFD(jpegmb), RK_MPI_MB_GetSize(jpegmb),
    //        RK_MPI_MB_GetModeID(jpegmb), RK_MPI_MB_GetChannelID(jpegmb),
    //        RK_MPI_MB_GetTimestamp(jpegmb));
    memcpy(jpeginfo->jpegdata, RK_MPI_MB_GetPtr(jpegmb), RK_MPI_MB_GetSize(jpegmb));
    jpeginfo->jpeglength = RK_MPI_MB_GetSize(jpegmb);
    RK_MPI_MB_ReleaseBuffer(jpegmb);

    ret = RK_MPI_VENC_DestroyChn(0);
    if (ret)
    {
        LogE("Destroy Venc(JPEG) failed! ret=%d\n", ret);
        return ret;
    }

    return ret;
}
