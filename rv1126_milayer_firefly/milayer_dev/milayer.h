#ifndef MI_LAYER_H
#define MI_LAYER_H

#include <queue>
#include <pthread.h>

#define MI_SUCESS 0
#define MI_ERROR_PARAM_INVALID -1
#define MI_ERROR_NOMEM -2
#define MI_ERROR_TIMEOUT -3
//其他错误码详见SDK

enum CodecType
{
    CODEC_H264 = 5,
    CODEC_H265 = 6,
};

enum FrameType
{
    FRAME_I = 0,
    FRAME_P = 1,
};

enum PixelFormatType
{
    PIXEL_FORMAT_NV12 = 0xa << 8,
    PIXEL_FORMAT_BGR = 0x7 << 8,
    PIXEL_FORMAT_RGB = 0x2 << 8,
};

typedef struct _ReSizeRect
{
    int width;
    int height;
} ReSizeRect;

typedef struct _CropRect
{
    // x,y为左上角坐标值,w,h为裁剪图的宽高
    int upperleftx;
    int upperlefty;
    int width;
    int height;
} CropRect;

typedef struct _JpegInfo
{
    unsigned char *jpegdata; // buff大小按照原始图片的长*宽*3/2大小申请
    int jpeglength;
} JpegInfo;

typedef struct _PacketInfo
{
    long long timestamp;
    int codectype;       // h264、h265
    int frametype;       // h26x帧类型，I帧，P帧
    unsigned char *data; // 帧数据
    int datalength;      // 帧长度
} PacketInfo;

typedef struct _FrameInfo
{
    int pixelformat;        // rawdata的pixelformat
    unsigned char *rawdata; // rawdata
    ReSizeRect size;        // rawdata尺寸
    long long timestamp;    // 解码后的视频帧时间戳
} FrameInfo;

typedef struct
{
    std::queue<PacketInfo *> *demux_to_decode_empty_queue;
    pthread_mutex_t demux_to_decode_empty_queue_lock;
    pthread_cond_t demux_to_decode_empty_queue_cond;
    std::queue<PacketInfo *> *demux_to_decode_filled_queue;
    pthread_mutex_t demux_to_decode_filled_queue_lock;
    pthread_cond_t demux_to_decode_filled_queue_cond;
} demuxToDecodeContext;

/**
 * @brief  创建解码通道
 * @param  channelcount         [in]解码通道ID
 * @param  codectype            [in]视频编码类型CodecType
 * @param  demuxtodecodecontext [in]Vdemux和Vdec模块之间数据传输的队列
 * @return success=0, fail=详见错误码说明
 */
int VdecChnCreate(int channelid, int codectype, demuxToDecodeContext *demuxtodecodecontext);

/**
 * @brief  获取对应通道ID的Frame,该接口有获取Frame数据失败的可能性
 * @param  channelid     [in]解码通道ID
 * @param  frameinfo     [out]输出原始的frame
 * @return success=0, fail=详见错误码说明
 */
int VdecGetFrame(int channelid, FrameInfo *frameinfo);

/**
 * @brief  Frame等比缩放 1.Unsupported to scaling less than 1/16 ~ 16 times,放大或者缩小的倍数最大为16
 *                       2.only support 4 aligned horizontal stride,默认长度对齐像素长度最小为4
 * @param  srcframeinfo  [in]原始图像
 * @param  dsize         [in]等比缩放之后的宽高
 * @param  dstframeinfo  [out]缩放获取到的图像
 * @return success=0, fail=详见错误码说明
 */
int ResizeFrame(FrameInfo *srcframeinfo, ReSizeRect dsize, FrameInfo *dstframeinfo);

/**
 * @brief  Frame区域裁剪 1.only support 4 aligned horizontal stride,默认长度对齐像素长度最小为4
 * @param  srcframeinfo [in]原始图像
 * @param  rect         [in]裁切区域
 * @param  dstframeinfo [out]裁切后，获取到的图像
 * @return success=0, fail=详见错误码说明
 */
int CropFrame(FrameInfo *srcframeinfo, CropRect rect, FrameInfo *dstframeinfo);

/**
 * @brief  Frame像素空间转换
 * @param  srcframeinfo [in]原始图像
 * @param  pixelformat  [in]颜色空间,0=BGR 1=RGB
 * @param  dstframeinfo [out]空间转换获取到的图像
 * @return success=0, fail=详见错误码说明
 */
int PixelFormatFrame(FrameInfo *srcframeinfo, int pixelformat, FrameInfo *dstframeinfo);

/**
 * @brief  编码jpg图片 1.only support 8 aligned horizontal stride in pixel for YUV420SP with pixel size 1
 *                       默认长度对齐像素长度最小为8
 * @param  srcframeinfo [in]原始图像
 * @param  jpegInfo     [out]编码jpg图片
 * @return success=0, fail=详见错误码说明
 */
int SaveImage(FrameInfo *srcframeinfo, JpegInfo *jpeginfo);

#endif