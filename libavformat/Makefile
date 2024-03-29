#
# libavformat Makefile
# (c) 2000-2003 Fabrice Bellard
#
include ../config.mak

CFLAGS=$(OPTFLAGS) -I.. -I$(SRC_PATH) -I$(SRC_PATH)/libavutil \
       -I$(SRC_PATH)/libavcodec -DHAVE_AV_CONFIG_H -D_FILE_OFFSET_BITS=64 \
       -D_LARGEFILE_SOURCE -D_ISOC9X_SOURCE

OBJS= utils.o cutils.o os_support.o allformats.o
CPPOBJS=

HEADERS = avformat.h avio.h rtp.h rtsp.h rtspcodes.h

# muxers/demuxers
OBJS-$(CONFIG_FOURXM_DEMUXER)            += 4xm.o
OBJS-$(CONFIG_ADTS_MUXER)                += adtsenc.o
OBJS-$(CONFIG_AIFF_DEMUXER)              += aiff.o riff.o
OBJS-$(CONFIG_AIFF_MUXER)                += aiff.o riff.o
OBJS-$(CONFIG_AMR_DEMUXER)               += amr.o
OBJS-$(CONFIG_AMR_MUXER)                 += amr.o
OBJS-$(CONFIG_ASF_DEMUXER)               += asf.o riff.o
OBJS-$(CONFIG_ASF_MUXER)                 += asf-enc.o riff.o
OBJS-$(CONFIG_ASF_STREAM_MUXER)          += asf-enc.o riff.o
OBJS-$(CONFIG_AU_DEMUXER)                += au.o riff.o
OBJS-$(CONFIG_AU_MUXER)                  += au.o riff.o
OBJS-$(CONFIG_AVI_DEMUXER)               += avidec.o riff.o
OBJS-$(CONFIG_AVI_MUXER)                 += avienc.o riff.o
OBJS-$(CONFIG_AVS_DEMUXER)               += avs.o
OBJS-$(CONFIG_CRC_MUXER)                 += crc.o
OBJS-$(CONFIG_FRAMECRC_MUXER)            += crc.o
OBJS-$(CONFIG_DAUD_DEMUXER)              += daud.o
OBJS-$(CONFIG_DV_DEMUXER)                += dv.o
OBJS-$(CONFIG_DV_MUXER)                  += dv.o
OBJS-$(CONFIG_EA_DEMUXER)                += electronicarts.o
OBJS-$(CONFIG_FFM_DEMUXER)               += ffm.o
OBJS-$(CONFIG_FFM_MUXER)                 += ffm.o
OBJS-$(CONFIG_FLIC_DEMUXER)              += flic.o
OBJS-$(CONFIG_FLV_DEMUXER)               += flvdec.o
OBJS-$(CONFIG_FLV_MUXER)                 += flvenc.o
OBJS-$(CONFIG_GIF_MUXER)                 += gif.o
OBJS-$(CONFIG_GIF_DEMUXER)               += gifdec.o
OBJS-$(CONFIG_GXF_DEMUXER)               += gxf.o
OBJS-$(CONFIG_IDCIN_DEMUXER)             += idcin.o
OBJS-$(CONFIG_ROQ_DEMUXER)               += idroq.o
OBJS-$(CONFIG_IMAGE2_DEMUXER)            += img2.o
OBJS-$(CONFIG_IMAGE2PIPE_DEMUXER)        += img2.o
OBJS-$(CONFIG_IMAGE2_MUXER)              += img2.o
OBJS-$(CONFIG_IMAGE2PIPE_MUXER)          += img2.o
OBJS-$(CONFIG_IMAGE_DEMUXER)             += img.o
OBJS-$(CONFIG_IMAGEPIPE_DEMUXER)         += img.o
OBJS-$(CONFIG_IMAGE_MUXER)               += img.o
OBJS-$(CONFIG_IMAGEPIPE_MUXER)           += img.o
OBJS-$(CONFIG_IPMOVIE_DEMUXER)           += ipmovie.o
OBJS-$(CONFIG_MATROSKA_DEMUXER)          += matroska.o riff.o
OBJS-$(CONFIG_MM_DEMUXER)                += mm.o
OBJS-$(CONFIG_MMF_DEMUXER)               += mmf.o riff.o
OBJS-$(CONFIG_MMF_MUXER)                 += mmf.o riff.o
OBJS-$(CONFIG_MOV_DEMUXER)               += mov.o riff.o isom.o
OBJS-$(CONFIG_MOV_MUXER)                 += movenc.o riff.o isom.o
OBJS-$(CONFIG_TGP_MUXER)                 += movenc.o riff.o isom.o
OBJS-$(CONFIG_MP4_MUXER)                 += movenc.o riff.o isom.o
OBJS-$(CONFIG_PSP_MUXER)                 += movenc.o riff.o isom.o
OBJS-$(CONFIG_TG2_MUXER)                 += movenc.o riff.o isom.o
OBJS-$(CONFIG_MP3_DEMUXER)               += mp3.o
OBJS-$(CONFIG_MP2_MUXER)                 += mp3.o
OBJS-$(CONFIG_MP3_MUXER)                 += mp3.o
OBJS-$(CONFIG_MPEG1SYSTEM_MUXER)         += mpeg.o
OBJS-$(CONFIG_MPEG1VCD_MUXER)            += mpeg.o
OBJS-$(CONFIG_MPEG2VOB_MUXER)            += mpeg.o
OBJS-$(CONFIG_MPEG2SVCD_MUXER)           += mpeg.o
OBJS-$(CONFIG_MPEG2DVD_MUXER)            += mpeg.o
OBJS-$(CONFIG_MPEGPS_DEMUXER)            += mpeg.o
OBJS-$(CONFIG_MPEGTS_DEMUXER)            += mpegts.o
OBJS-$(CONFIG_MPEGTS_MUXER)              += mpegtsenc.o
OBJS-$(CONFIG_MPJPEG_MUXER)              += mpjpeg.o
OBJS-$(CONFIG_MXF_DEMUXER)               += mxf.o
OBJS-$(CONFIG_NSV_DEMUXER)               += nsvdec.o riff.o
OBJS-$(CONFIG_NUT_DEMUXER)               += nut.o riff.o
OBJS-$(CONFIG_NUT_MUXER)                 += nut.o riff.o
OBJS-$(CONFIG_NUV_DEMUXER)               += nuv.o riff.o
OBJS-$(CONFIG_OGG_DEMUXER)               += ogg2.o           \
                                            oggparsevorbis.o \
                                            oggparsetheora.o \
                                            oggparseflac.o   \
                                            oggparseogm.o    \
                                            riff.o
OBJS-$(CONFIG_STR_DEMUXER)               += psxstr.o
OBJS-$(CONFIG_SHORTEN_DEMUXER)           += raw.o
OBJS-$(CONFIG_FLAC_DEMUXER)              += raw.o
OBJS-$(CONFIG_FLAC_MUXER)                += raw.o
OBJS-$(CONFIG_AC3_DEMUXER)               += raw.o
OBJS-$(CONFIG_AC3_MUXER)                 += raw.o
OBJS-$(CONFIG_DTS_DEMUXER)               += raw.o
OBJS-$(CONFIG_AAC_DEMUXER)               += raw.o
OBJS-$(CONFIG_H261_DEMUXER)              += raw.o
OBJS-$(CONFIG_H261_MUXER)                += raw.o
OBJS-$(CONFIG_H263_DEMUXER)              += raw.o
OBJS-$(CONFIG_H263_MUXER)                += raw.o
OBJS-$(CONFIG_M4V_DEMUXER)               += raw.o
OBJS-$(CONFIG_M4V_MUXER)                 += raw.o
OBJS-$(CONFIG_H264_DEMUXER)              += raw.o
OBJS-$(CONFIG_H264_MUXER)                += raw.o
OBJS-$(CONFIG_MPEGVIDEO_DEMUXER)         += raw.o
OBJS-$(CONFIG_MPEG1VIDEO_MUXER)          += raw.o
OBJS-$(CONFIG_MPEG2VIDEO_MUXER)          += raw.o
OBJS-$(CONFIG_MJPEG_DEMUXER)             += raw.o
OBJS-$(CONFIG_INGENIENT_DEMUXER)         += raw.o
OBJS-$(CONFIG_MJPEG_MUXER)               += raw.o
OBJS-$(CONFIG_RAWVIDEO_DEMUXER)          += raw.o
OBJS-$(CONFIG_RAWVIDEO_MUXER)            += raw.o
OBJS-$(CONFIG_NULL_MUXER)                += raw.o
OBJS-$(CONFIG_RM_DEMUXER)                += rm.o
OBJS-$(CONFIG_RM_MUXER)                  += rm.o
OBJS-$(CONFIG_SEGAFILM_DEMUXER)          += segafilm.o
OBJS-$(CONFIG_VMD_DEMUXER)               += sierravmd.o
OBJS-$(CONFIG_SMACKER_DEMUXER)           += smacker.o
OBJS-$(CONFIG_SOL_DEMUXER)               += sol.o
OBJS-$(CONFIG_SWF_DEMUXER)               += swf.o
OBJS-$(CONFIG_SWF_MUXER)                 += swf.o
OBJS-$(CONFIG_TTA_DEMUXER)               += tta.o
OBJS-$(CONFIG_VOC_DEMUXER)               += voc.o riff.o
OBJS-$(CONFIG_VOC_MUXER)                 += voc.o riff.o
OBJS-$(CONFIG_WAV_DEMUXER)               += wav.o riff.o
OBJS-$(CONFIG_WAV_MUXER)                 += wav.o riff.o
OBJS-$(CONFIG_WC3_DEMUXER)               += wc3movie.o
OBJS-$(CONFIG_WSAUD_DEMUXER)             += westwood.o
OBJS-$(CONFIG_WSVQA_DEMUXER)             += westwood.o
OBJS-$(CONFIG_YUV4MPEGPIPE_MUXER)        += yuv4mpeg.o
OBJS-$(CONFIG_YUV4MPEGPIPE_DEMUXER)      += yuv4mpeg.o

# image formats
OBJS+= pnm.o yuv.o png.o jpeg.o gifdec.o sgi.o
OBJS+= framehook.o

ifeq ($(CONFIG_VIDEO4LINUX),yes)
OBJS-$(CONFIG_VIDEO_GRAB_DEVICE_DEMUXER) += grab.o
endif

ifeq ($(CONFIG_VIDEO4LINUX2),yes)
OBJS-$(CONFIG_V4L2_DEMUXER)              += v4l2.o
endif

ifeq ($(CONFIG_BKTR),yes)
OBJS-$(CONFIG_VIDEO_GRAB_DEVICE_DEMUXER) += grab_bktr.o
endif

ifeq ($(CONFIG_DV1394),yes)
OBJS-$(CONFIG_DV1394_DEMUXER)            += dv1394.o
endif

ifeq ($(CONFIG_DC1394),yes)
OBJS-$(CONFIG_DC1394_DEMUXER)            += dc1394.o
endif

ifeq ($(CONFIG_AUDIO_OSS),yes)
OBJS-$(CONFIG_AUDIO_DEMUXER)             += audio.o
OBJS-$(CONFIG_AUDIO_MUXER)               += audio.o
endif

EXTRALIBS := -L../libavutil -lavutil$(BUILDSUF) \
             -lavcodec$(BUILDSUF) -L../libavcodec $(EXTRALIBS)

ifeq ($(CONFIG_AUDIO_BEOS),yes)
CPPOBJS+= beosaudio.o
endif

# protocols I/O
OBJS+= avio.o aviobuf.o

ifeq ($(CONFIG_PROTOCOLS),yes)
OBJS+= file.o
ifeq ($(CONFIG_NETWORK),yes)
OBJS+= udp.o tcp.o http.o rtsp.o rtp.o rtpproto.o
# BeOS and Darwin network stuff
ifeq ($(NEED_INET_ATON),yes)
OBJS+= barpainet.o
endif
endif
endif

ifeq ($(CONFIG_LIBOGG),yes)
OBJS-$(CONFIG_OGG_MUXER)                 += ogg.o
endif

ifeq ($(CONFIG_GPL),yes)
OBJS-$(CONFIG_GXF_MUXER)                 += gxfenc.o
endif

OBJS += $(OBJS-yes)

NAME=avformat
ifeq ($(BUILD_SHARED),yes)
LIBVERSION=$(LAVFVERSION)
LIBMAJOR=$(LAVFMAJOR)
endif

include $(SRC_PATH)/common.mak
