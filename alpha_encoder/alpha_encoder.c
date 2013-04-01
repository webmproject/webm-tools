/*
 *  Copyright (c) 2013 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * This is an example of an VP8 encoder which can produce VP8 videos with alpha.
 * It takes an input file in raw AYUV format, passes it through the encoder, and
 * writes the compressed frames to disk in webm format.
 * 
 * The design on how alpha encoding works can be found here: http://goo.gl/wCP1y
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#define VPX_CODEC_DISABLE_COMPAT 1
#include "vpx/vpx_encoder.h"
#include "vpx/vp8cx.h"
#include "libmkv/EbmlWriter.h"
#include "libmkv/EbmlIDs.h"
#define interface (vpx_codec_vp8_cx())
#define fourcc    0x30385056

#undef CHAR_BIT
#define CHAR_BIT __CHAR_BIT__
#undef SHRT_MIN
#define SHRT_MIN (-SHRT_MAX - 1)
#undef SHRT_MAX
#define SHRT_MAX __SHRT_MAX__

#define LITERALU64(hi,lo) ((((uint64_t)hi)<<32)|lo)

static void die(const char *fmt, ...) {
    va_list ap;

    va_start(ap, fmt);
    vprintf(fmt, ap);
    if(fmt[strlen(fmt)-1] != '\n')
        printf("\n");
    exit(EXIT_FAILURE);
}

static void die_codec(vpx_codec_ctx_t *ctx, const char *s) {
    const char *detail = vpx_codec_error_detail(ctx);

    printf("%s: %s\n", s, vpx_codec_error(ctx));
    if(detail)
        printf("    %s\n",detail);
    exit(EXIT_FAILURE);
}

/* Murmur hash derived from public domain reference implementation at
 *   http://sites.google.com/site/murmurhash/
 */
static unsigned int murmur ( const void * key, int len, unsigned int seed )
{
    const unsigned int m = 0x5bd1e995;
    const int r = 24;

    unsigned int h = seed ^ len;

    const unsigned char * data = (const unsigned char *)key;

    while(len >= 4)
    {
        unsigned int k;

        k  = data[0];
        k |= data[1] << 8;
        k |= data[2] << 16;
        k |= data[3] << 24;

        k *= m;
        k ^= k >> r;
        k *= m;

        h *= m;
        h ^= k;

        data += 4;
        len -= 4;
    }

    switch(len)
    {
    case 3: h ^= data[2] << 16;
    case 2: h ^= data[1] << 8;
    case 1: h ^= data[0];
            h *= m;
    };

    h ^= h >> 13;
    h *= m;
    h ^= h >> 15;

    return h;
}

static int read_frame(FILE *f, vpx_image_t *img, vpx_image_t *img_alpha) {

    int w = img->d_w;
    int h = img->d_h;
    int uv_w = (1 + w) / 2;
    int uv_h = (1 + h) / 2;
    unsigned char *planes[4];
    int i, row;

    planes[0] = img->planes[VPX_PLANE_Y];
    planes[1] = img->planes[VPX_PLANE_U];
    planes[2] = img->planes[VPX_PLANE_V];
    planes[3] = img_alpha->planes[VPX_PLANE_Y];

    for(i = 0; i < 4; i++)
    {
        int width = (i == 0 || i == 3) ? w : (1 + w) / 2;
        int height = (i == 0 || i == 3) ? h : (1 + h) / 2;
        for(row = 0; row < height; row++)
        {
            if(fread(planes[i], 1, width, f) != width)
                return 0;
            planes[i] += img->stride[i > 2 ? 0 : i];
        }
    }
    memset(img_alpha->planes[VPX_PLANE_U], 0, uv_w * uv_h);
    memset(img_alpha->planes[VPX_PLANE_V], 0, uv_w * uv_h);
    return 1;
}

typedef off_t EbmlLoc;

struct cue_entry
{
    unsigned int time;
    uint64_t     loc;
};

typedef enum stereo_format
{
    STEREO_FORMAT_MONO       = 0,
    STEREO_FORMAT_LEFT_RIGHT = 1,
    STEREO_FORMAT_BOTTOM_TOP = 2,
    STEREO_FORMAT_TOP_BOTTOM = 3,
    STEREO_FORMAT_RIGHT_LEFT = 11
} stereo_format_t;


struct EbmlGlobal
{
    int debug;

    FILE    *stream;
    int64_t last_pts_ms;
    vpx_rational_t  framerate;

    /* These pointers are to the start of an element */
    off_t    position_reference;
    off_t    seek_info_pos;
    off_t    segment_info_pos;
    off_t    track_pos;
    off_t    cue_pos;
    off_t    cluster_pos;

    /* This pointer is to a specific element to be serialized */
    off_t    track_id_pos;

    /* These pointers are to the size field of the element */
    EbmlLoc  startSegment;
    EbmlLoc  startCluster;

    uint32_t cluster_timecode;
    int      cluster_open;

    struct cue_entry *cue_list;
    unsigned int      cues;

};


void Ebml_Write(EbmlGlobal *glob, const void *buffer_in, unsigned long len)
{
    (void) fwrite(buffer_in, 1, len, glob->stream);
}

#define WRITE_BUFFER(s) \
for(i = len-1; i>=0; i--)\
{ \
    x = (char)(*(const s *)buffer_in >> (i * CHAR_BIT)); \
    Ebml_Write(glob, &x, 1); \
}
void Ebml_Serialize(EbmlGlobal *glob, const void *buffer_in, int buffer_size, unsigned long len)
{
    char x;
    int i;

    /* buffer_size:
     * 1 - int8_t;
     * 2 - int16_t;
     * 3 - int32_t;
     * 4 - int64_t;
     */
    switch (buffer_size)
    {
        case 1:
            WRITE_BUFFER(int8_t)
            break;
        case 2:
            WRITE_BUFFER(int16_t)
            break;
        case 4:
            WRITE_BUFFER(int32_t)
            break;
        case 8:
            WRITE_BUFFER(int64_t)
            break;
        default:
            break;
    }
}
#undef WRITE_BUFFER

/* Need a fixed size serializer for the track ID. libmkv provides a 64 bit
 * one, but not a 32 bit one.
 */
static void Ebml_SerializeUnsigned32(EbmlGlobal *glob, unsigned long class_id, uint64_t ui)
{
    unsigned char sizeSerialized = 4 | 0x80;
    Ebml_WriteID(glob, class_id);
    Ebml_Serialize(glob, &sizeSerialized, sizeof(sizeSerialized), 1);
    Ebml_Serialize(glob, &ui, sizeof(ui), 4);
}


static void
Ebml_StartSubElement(EbmlGlobal *glob, EbmlLoc *ebmlLoc,
                          unsigned long class_id)
{
    /* todo this is always taking 8 bytes, this may need later optimization */
    /* this is a key that says length unknown */
    uint64_t unknownLen = LITERALU64(0x01FFFFFF, 0xFFFFFFFF);

    Ebml_WriteID(glob, class_id);
    *ebmlLoc = ftello(glob->stream);
    Ebml_Serialize(glob, &unknownLen, sizeof(unknownLen), 8);
}

static void
Ebml_EndSubElement(EbmlGlobal *glob, EbmlLoc *ebmlLoc)
{
    off_t pos;
    uint64_t size;

    /* Save the current stream pointer */
    pos = ftello(glob->stream);

    /* Calculate the size of this element */
    size = pos - *ebmlLoc - 8;
    size |= LITERALU64(0x01000000,0x00000000);

    /* Seek back to the beginning of the element and write the new size */
    fseeko(glob->stream, *ebmlLoc, SEEK_SET);
    Ebml_Serialize(glob, &size, sizeof(size), 8);

    /* Reset the stream pointer */
    fseeko(glob->stream, pos, SEEK_SET);
}


static void
write_webm_seek_element(EbmlGlobal *ebml, unsigned long id, off_t pos)
{
    uint64_t offset = pos - ebml->position_reference;
    EbmlLoc start;
    Ebml_StartSubElement(ebml, &start, Seek);
    Ebml_SerializeBinary(ebml, SeekID, id);
    Ebml_SerializeUnsigned64(ebml, SeekPosition, offset);
    Ebml_EndSubElement(ebml, &start);
}

static void
write_webm_seek_info(EbmlGlobal *ebml)
{

    off_t pos;

    /* Save the current stream pointer */
    pos = ftello(ebml->stream);

    if(ebml->seek_info_pos)
        fseeko(ebml->stream, ebml->seek_info_pos, SEEK_SET);
    else
        ebml->seek_info_pos = pos;

    {
        EbmlLoc start;

        Ebml_StartSubElement(ebml, &start, SeekHead);
        write_webm_seek_element(ebml, Tracks, ebml->track_pos);
        write_webm_seek_element(ebml, Cues,   ebml->cue_pos);
        write_webm_seek_element(ebml, Info,   ebml->segment_info_pos);
        Ebml_EndSubElement(ebml, &start);
    }
    {
        /* segment info */
        EbmlLoc startInfo;
        uint64_t frame_time;
        char version_string[64];

        /* Assemble version string */
        if(ebml->debug)
            strcpy(version_string, "vpxenc");
        else
        {
            strcpy(version_string, "vpxenc ");
            strncat(version_string,
                    vpx_codec_version_str(),
                    sizeof(version_string) - 1 - strlen(version_string));
        }

        frame_time = (uint64_t)1000 * ebml->framerate.den
                     / ebml->framerate.num;
        ebml->segment_info_pos = ftello(ebml->stream);
        Ebml_StartSubElement(ebml, &startInfo, Info);
        Ebml_SerializeUnsigned(ebml, TimecodeScale, 1000000);
        Ebml_SerializeFloat(ebml, Segment_Duration,
                            (double)(ebml->last_pts_ms + frame_time));
        Ebml_SerializeString(ebml, 0x4D80, version_string);
        Ebml_SerializeString(ebml, 0x5741, version_string);
        Ebml_EndSubElement(ebml, &startInfo);
    }
}

static void
write_webm_file_header(EbmlGlobal                *glob,
                       const vpx_codec_enc_cfg_t *cfg,
                       const struct vpx_rational *fps,
                       stereo_format_t            stereo_fmt)
{
    {
        EbmlLoc start;
        Ebml_StartSubElement(glob, &start, EBML);
        Ebml_SerializeUnsigned(glob, EBMLVersion, 1);
        Ebml_SerializeUnsigned(glob, EBMLReadVersion, 1);
        Ebml_SerializeUnsigned(glob, EBMLMaxIDLength, 4);
        Ebml_SerializeUnsigned(glob, EBMLMaxSizeLength, 8);
        Ebml_SerializeString(glob, DocType, "webm");
        Ebml_SerializeUnsigned(glob, DocTypeVersion, 2);
        Ebml_SerializeUnsigned(glob, DocTypeReadVersion, 2);
        Ebml_EndSubElement(glob, &start);
    }
    {
        Ebml_StartSubElement(glob, &glob->startSegment, Segment);
        glob->position_reference = ftello(glob->stream);
        glob->framerate = *fps;
        write_webm_seek_info(glob);


        {
            EbmlLoc trackStart;
            glob->track_pos = ftello(glob->stream);
            Ebml_StartSubElement(glob, &trackStart, Tracks);
            {
                unsigned int trackNumber = 1;
                uint64_t     trackID = 0;

                EbmlLoc start;
                Ebml_StartSubElement(glob, &start, TrackEntry);
                Ebml_SerializeUnsigned(glob, TrackNumber, trackNumber);
                glob->track_id_pos = ftello(glob->stream);
                Ebml_SerializeUnsigned32(glob, TrackUID, trackID);
                Ebml_SerializeUnsigned(glob, TrackType, 1);
                Ebml_SerializeUnsigned(glob, MaxBlockAdditionID, 1);
                Ebml_SerializeString(glob, CodecID, "V_VP8");
                {
                    unsigned int pixelWidth = cfg->g_w;
                    unsigned int pixelHeight = cfg->g_h;
                    float        frameRate   = (float)fps->num/(float)fps->den;

                    EbmlLoc videoStart;
                    Ebml_StartSubElement(glob, &videoStart, Video);
                    Ebml_SerializeUnsigned(glob, PixelWidth, pixelWidth);
                    Ebml_SerializeUnsigned(glob, PixelHeight, pixelHeight);
                    Ebml_SerializeUnsigned(glob, StereoMode, stereo_fmt);
                    Ebml_SerializeUnsigned(glob, AlphaMode, 1);
                    Ebml_SerializeFloat(glob, FrameRate, frameRate);
                    Ebml_EndSubElement(glob, &videoStart);
                }
                Ebml_EndSubElement(glob, &start); /* Track Entry */
            }
            Ebml_EndSubElement(glob, &trackStart);
        }
        /* segment element is open */
    }
}


static void
write_webm_block(EbmlGlobal                *glob,
                 const vpx_codec_enc_cfg_t *cfg,
                 const vpx_codec_cx_pkt_t  *pkt,
                 const vpx_codec_cx_pkt_t  *pkt_alpha,
                 const vpx_image_t         *img_alpha)
{
    unsigned long  block_length;
    unsigned char  track_number;
    unsigned short block_timecode = 0;
    unsigned char  flags;
    int64_t        pts_ms;
    int            start_cluster = 0, is_keyframe;
    EbmlLoc block_group, block_additions, block_more;

    /* Calculate the PTS of this frame in milliseconds */
    pts_ms = pkt->data.frame.pts * 1000
             * (uint64_t)cfg->g_timebase.num / (uint64_t)cfg->g_timebase.den;
    if(pts_ms <= glob->last_pts_ms)
        pts_ms = glob->last_pts_ms + 1;
    glob->last_pts_ms = pts_ms;

    /* Calculate the relative time of this block */
    if(pts_ms - glob->cluster_timecode > SHRT_MAX)
        start_cluster = 1;
    else
        block_timecode = (unsigned short)pts_ms - glob->cluster_timecode;

    is_keyframe = (pkt->data.frame.flags & VPX_FRAME_IS_KEY);
    if(start_cluster || is_keyframe)
    {
        if(glob->cluster_open)
            Ebml_EndSubElement(glob, &glob->startCluster);

        /* Open the new cluster */
        block_timecode = 0;
        glob->cluster_open = 1;
        glob->cluster_timecode = (uint32_t)pts_ms;
        glob->cluster_pos = ftello(glob->stream);
        Ebml_StartSubElement(glob, &glob->startCluster, Cluster); /* cluster */
        Ebml_SerializeUnsigned(glob, Timecode, glob->cluster_timecode);

        /* Save a cue point if this is a keyframe. */
        if(is_keyframe)
        {
            struct cue_entry *cue, *new_cue_list;

            new_cue_list = (struct cue_entry*) realloc(
                               glob->cue_list,
                               (glob->cues+1) * sizeof(struct cue_entry));
            if(new_cue_list)
                glob->cue_list = new_cue_list;
            else
                die("Failed to realloc cue list.");

            cue = &glob->cue_list[glob->cues];
            cue->time = glob->cluster_timecode;
            cue->loc = glob->cluster_pos;
            glob->cues++;
        }
    }

    /* Write the Simple Block */
    Ebml_StartSubElement(glob, &block_group, BlockGroup);
    Ebml_WriteID(glob, Block);

    block_length = (unsigned long)pkt->data.frame.sz + 4;
    block_length |= 0x10000000;
    Ebml_Serialize(glob, &block_length, sizeof(block_length), 4);

    track_number = 1;
    track_number |= 0x80;
    Ebml_Write(glob, &track_number, 1);

    Ebml_Serialize(glob, &block_timecode, sizeof(block_timecode), 2);

    flags = 0;
    if(is_keyframe)
        flags |= 0x80;
    if(pkt->data.frame.flags & VPX_FRAME_IS_INVISIBLE)
        flags |= 0x08;
    Ebml_Write(glob, &flags, 1);

    Ebml_Write(glob, pkt->data.frame.buf, (unsigned long)pkt->data.frame.sz);

    if(img_alpha)
    {
        Ebml_StartSubElement(glob, &block_additions, BlockAdditions);
        Ebml_StartSubElement(glob, &block_more, BlockMore);

        Ebml_SerializeUnsigned64(glob, BlockAddID, 1);

        Ebml_WriteID(glob, BlockAdditional);
        block_length = pkt_alpha->data.frame.sz;
        block_length |= 0x10000000;
        Ebml_Serialize(glob, &block_length, sizeof(block_length), 4);
        Ebml_Write(glob, pkt_alpha->data.frame.buf,
                   (unsigned long)pkt_alpha->data.frame.sz);

        Ebml_EndSubElement(glob, &block_more);
        Ebml_EndSubElement(glob, &block_additions);
    }
    Ebml_EndSubElement(glob, &block_group);
}


static void
write_webm_file_footer(EbmlGlobal *glob, long hash)
{

    if(glob->cluster_open)
        Ebml_EndSubElement(glob, &glob->startCluster);

    {
        EbmlLoc start;
        unsigned int i;

        glob->cue_pos = ftello(glob->stream);
        Ebml_StartSubElement(glob, &start, Cues);
        for(i=0; i<glob->cues; i++)
        {
            struct cue_entry *cue = &glob->cue_list[i];
            EbmlLoc start;

            Ebml_StartSubElement(glob, &start, CuePoint);
            {
                EbmlLoc start;

                Ebml_SerializeUnsigned(glob, CueTime, cue->time);

                Ebml_StartSubElement(glob, &start, CueTrackPositions);
                Ebml_SerializeUnsigned(glob, CueTrack, 1);
                Ebml_SerializeUnsigned64(glob, CueClusterPosition,
                                         cue->loc - glob->position_reference);
                Ebml_EndSubElement(glob, &start);
            }
            Ebml_EndSubElement(glob, &start);
        }
        Ebml_EndSubElement(glob, &start);
    }

    Ebml_EndSubElement(glob, &glob->startSegment);

    /* Patch up the seek info block */
    write_webm_seek_info(glob);

    /* Patch up the track id */
    fseeko(glob->stream, glob->track_id_pos, SEEK_SET);
    Ebml_SerializeUnsigned32(glob, TrackUID, glob->debug ? 0xDEADBEEF : hash);

    fseeko(glob->stream, 0, SEEK_END);
}

int main(int argc, char **argv) {
    FILE                *infile;
    vpx_codec_ctx_t      codec;
    vpx_codec_ctx_t      codec_alpha;
    vpx_codec_enc_cfg_t  cfg;
    vpx_codec_enc_cfg_t  cfg_alpha;
    int                  frame_cnt = 0;
    vpx_image_t          raw;
    vpx_image_t          raw_alpha;
    vpx_codec_err_t      res;
    long                 width;
    long                 height;
    int                  frame_avail;
    int                  got_data;
    int                  flags = 0;
    EbmlGlobal            ebml = {0};
    uint32_t              hash = 0;
    struct vpx_rational  frame_rate = {30,1};

    /* Open files */
    if(argc!=5)
        die("Usage: %s <width> <height> <infile> <outfile>\n", argv[0]);
    width = strtol(argv[1], NULL, 0);
    height = strtol(argv[2], NULL, 0);
    if(width < 16 || height <16)
        die("Invalid resolution: %ldx%ld", width, height);
    if(!vpx_img_alloc(&raw, VPX_IMG_FMT_YV12, width, height, 1))
        die("Failed to allocate image", width, height);
    if(!vpx_img_alloc(&raw_alpha, VPX_IMG_FMT_YV12, width, height, 1))
        die("Failed to allocate alpha image", width, height);
    if(!(ebml.stream = fopen(argv[4], "wb")))
        die("Failed to open %s for writing", argv[4]);

    printf("Using %s\n",vpx_codec_iface_name(interface));

    /* Populate encoder configuration */
    res = vpx_codec_enc_config_default(interface, &cfg, 0);
    if(res) {
        printf("Failed to get config: %s\n", vpx_codec_err_to_string(res));
        return EXIT_FAILURE;
    }
    res = vpx_codec_enc_config_default(interface, &cfg_alpha, 0);
    if(res) {
        printf("Failed to get config: %s\n", vpx_codec_err_to_string(res));
        return EXIT_FAILURE;
    }

    /* Update the default configuration with our settings */
    cfg.rc_target_bitrate = width * height * cfg.rc_target_bitrate
                            / cfg.g_w / cfg.g_h;
    cfg.g_w = width;
    cfg.g_h = height;
    cfg_alpha.g_w = cfg.g_w;
    cfg_alpha.g_h = cfg.g_h;
    cfg_alpha.rc_target_bitrate = cfg.rc_target_bitrate;

    /* write header*/
    write_webm_file_header(&ebml, &cfg, &frame_rate, STEREO_FORMAT_MONO);

    /* Open input file for this encoding pass */
    if(!(infile = fopen(argv[3], "rb")))
        die("Failed to open %s for reading", argv[3]);

    /* Initialize codec */
    if(vpx_codec_enc_init(&codec, interface, &cfg, 0))
        die_codec(&codec, "Failed to initialize encoder");
    if(vpx_codec_enc_init(&codec_alpha, interface, &cfg_alpha, 0))
        die_codec(&codec_alpha, "Failed to initialize alpha encoder");

    frame_avail = 1;
    got_data = 0;
    while(frame_avail || got_data) {
        vpx_codec_iter_t iter = NULL;
        vpx_codec_iter_t iter_alpha = NULL;
        const vpx_codec_cx_pkt_t *pkt;
        const vpx_codec_cx_pkt_t *pkt_alpha;

        frame_avail = read_frame(infile, &raw, &raw_alpha);
        if(vpx_codec_encode(&codec, frame_avail? &raw : NULL, frame_cnt,
                            1, flags, VPX_DL_REALTIME))
            die_codec(&codec, "Failed to encode frame");
        if(vpx_codec_encode(&codec_alpha, frame_avail? &raw_alpha : NULL,
                            frame_cnt, 1, flags, VPX_DL_REALTIME))
            die_codec(&codec, "Failed to encode alpha frame");
        got_data = 0;
        while( (pkt = vpx_codec_get_cx_data(&codec, &iter))
                && (pkt_alpha = vpx_codec_get_cx_data(&codec_alpha, &iter_alpha)) ) {
            got_data = 1;
            switch(pkt->kind) {
            case VPX_CODEC_CX_FRAME_PKT:
                write_webm_block(&ebml, &cfg, pkt, pkt_alpha, &raw_alpha);
                hash = murmur(pkt->data.frame.buf,
                              (int)pkt->data.frame.sz,
                              hash);
                break;
            default:
                break;
            }
            fprintf(stderr, pkt->kind == VPX_CODEC_CX_FRAME_PKT
                    && (pkt->data.frame.flags & VPX_FRAME_IS_KEY)? "K":".");
        }
        frame_cnt++;
    }
    fprintf(stderr, "\n");
    fclose(infile);

    write_webm_file_footer(&ebml, hash);

    fprintf(stderr, "Processed %d frames.\n",frame_cnt - 1);
    vpx_img_free(&raw);
    vpx_img_free(&raw_alpha);
    if(vpx_codec_destroy(&codec))
        die_codec(&codec, "Failed to destroy codec");
    if(vpx_codec_destroy(&codec_alpha))
        die_codec(&codec, "Failed to destroy alpha codec");

    fclose(ebml.stream);
    return EXIT_SUCCESS;
}
