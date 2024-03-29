/*
 * Copyright (c) 2000, 2001 Fabrice Bellard.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */
#define PACKET_SIZE 3200

typedef struct {
    int num;
    unsigned char seq;
    /* use for reading */
    AVPacket pkt;
    int frag_offset;
    int timestamp;
    int64_t duration;

    int ds_span;                /* descrambling  */
    int ds_packet_size;
    int ds_chunk_size;
    int ds_data_size;
    int ds_silence_data;

    int64_t packet_pos;

} ASFStream;

typedef struct {
    uint32_t v1;
    uint16_t v2;
    uint16_t v3;
    uint8_t v4[8];
} GUID;

typedef struct {
    GUID guid;                  // generated by client computer
    uint64_t file_size;         // in bytes
                                // invalid if broadcasting
    uint64_t create_time;       // time of creation, in 100-nanosecond units since 1.1.1601
                                // invalid if broadcasting
    uint64_t packets_count;     // how many packets are there in the file
                                // invalid if broadcasting
    uint64_t play_time;         // play time, in 100-nanosecond units
                                // invalid if broadcasting
    uint64_t send_time;         // time to send file, in 100-nanosecond units
                                // invalid if broadcasting (could be ignored)
    uint32_t preroll;           // timestamp of the first packet, in milliseconds
                                // if nonzero - substract from time
    uint32_t ignore;            // preroll is 64bit - but let's just ignore it
    uint32_t flags;             // 0x01 - broadcast
                                // 0x02 - seekable
                                // rest is reserved should be 0
    uint32_t min_pktsize;       // size of a data packet
                                // invalid if broadcasting
    uint32_t max_pktsize;       // shall be the same as for min_pktsize
                                // invalid if broadcasting
    uint32_t max_bitrate;       // bandwith of stream in bps
                                // should be the sum of bitrates of the
                                // individual media streams
} ASFMainHeader;


typedef struct {
    uint32_t packet_number;
    uint16_t packet_count;
} ASFIndex;


typedef struct {
    uint32_t seqno;
    unsigned int packet_size;
    int is_streamed;
    int asfid2avid[128];        /* conversion table from asf ID 2 AVStream ID */
    ASFStream streams[128];     /* it's max number and it's not that big */
    /* non streamed additonnal info */
    int64_t nb_packets;
    int64_t duration; /* in 100ns units */
    /* packet filling */
    unsigned char multi_payloads_present;
    int packet_size_left;
    int prev_packet_sent_time;
    int packet_timestamp_start;
    int packet_timestamp_end;
    unsigned int packet_nb_payloads;
    int packet_nb_frames;
    uint8_t packet_buf[PACKET_SIZE];
    ByteIOContext pb;
    /* only for reading */
    uint64_t data_offset; /* begining of the first data packet */
    uint64_t data_object_offset; /* data object offset (excl. GUID & size)*/
    uint64_t data_object_size;   /* size of the data object */

    ASFMainHeader hdr;

    int packet_flags;
    int packet_property;
    int packet_timestamp;
    int packet_segsizetype;
    int packet_segments;
    int packet_seq;
    int packet_replic_size;
    int packet_key_frame;
    int packet_padsize;
    int packet_frag_offset;
    int packet_frag_size;
    int packet_frag_timestamp;
    int packet_multi_size;
    int packet_obj_size;
    int packet_time_delta;
    int packet_time_start;
    int64_t packet_pos;

    int stream_index;


    int64_t last_indexed_pts;
    ASFIndex* index_ptr;
    uint32_t nb_index_count;
    uint32_t nb_index_memory_alloc;
    uint16_t maximum_packet;

    ASFStream* asf_st; /* currently decoded stream */
} ASFContext;

static const GUID asf_header = {
    0x75B22630, 0x668E, 0x11CF, { 0xA6, 0xD9, 0x00, 0xAA, 0x00, 0x62, 0xCE, 0x6C },
};

static const GUID file_header = {
    0x8CABDCA1, 0xA947, 0x11CF, { 0x8E, 0xE4, 0x00, 0xC0, 0x0C, 0x20, 0x53, 0x65 },
};

static const GUID stream_header = {
    0xB7DC0791, 0xA9B7, 0x11CF, { 0x8E, 0xE6, 0x00, 0xC0, 0x0C, 0x20, 0x53, 0x65 },
};

static const GUID ext_stream_header = {
    0x14E6A5CB, 0xC672, 0x4332, { 0x83, 0x99, 0xA9, 0x69, 0x52, 0x06, 0x5B, 0x5A },
};

static const GUID audio_stream = {
    0xF8699E40, 0x5B4D, 0x11CF, { 0xA8, 0xFD, 0x00, 0x80, 0x5F, 0x5C, 0x44, 0x2B },
};

static const GUID audio_conceal_none = {
    // 0x49f1a440, 0x4ece, 0x11d0, { 0xa3, 0xac, 0x00, 0xa0, 0xc9, 0x03, 0x48, 0xf6 },
    // New value lifted from avifile
    0x20fb5700, 0x5b55, 0x11cf, { 0xa8, 0xfd, 0x00, 0x80, 0x5f, 0x5c, 0x44, 0x2b },
};

static const GUID audio_conceal_spread = {
     0xBFC3CD50, 0x618F, 0x11CF, { 0x8B, 0xB2, 0x00, 0xAA, 0x00, 0xB4, 0xE2, 0x20 },
};

static const GUID video_stream = {
    0xBC19EFC0, 0x5B4D, 0x11CF, { 0xA8, 0xFD, 0x00, 0x80, 0x5F, 0x5C, 0x44, 0x2B },
};

static const GUID video_conceal_none = {
    0x20FB5700, 0x5B55, 0x11CF, { 0xA8, 0xFD, 0x00, 0x80, 0x5F, 0x5C, 0x44, 0x2B },
};

static const GUID command_stream = {
    0x59DACFC0, 0x59E6, 0x11D0, { 0xA3, 0xAC, 0x00, 0xA0, 0xC9, 0x03, 0x48, 0xF6 },
};

static const GUID comment_header = {
    0x75b22633, 0x668e, 0x11cf, { 0xa6, 0xd9, 0x00, 0xaa, 0x00, 0x62, 0xce, 0x6c },
};

static const GUID codec_comment_header = {
    0x86D15240, 0x311D, 0x11D0, { 0xA3, 0xA4, 0x00, 0xA0, 0xC9, 0x03, 0x48, 0xF6 },
};
static const GUID codec_comment1_header = {
    0x86d15241, 0x311d, 0x11d0, { 0xa3, 0xa4, 0x00, 0xa0, 0xc9, 0x03, 0x48, 0xf6 },
};

static const GUID data_header = {
    0x75b22636, 0x668e, 0x11cf, { 0xa6, 0xd9, 0x00, 0xaa, 0x00, 0x62, 0xce, 0x6c },
};

static const GUID head1_guid = {
    0x5fbf03b5, 0xa92e, 0x11cf, { 0x8e, 0xe3, 0x00, 0xc0, 0x0c, 0x20, 0x53, 0x65 },
};

static const GUID head2_guid = {
    0xabd3d211, 0xa9ba, 0x11cf, { 0x8e, 0xe6, 0x00, 0xc0, 0x0c, 0x20, 0x53, 0x65 },
};

static const GUID extended_content_header = {
        0xD2D0A440, 0xE307, 0x11D2, { 0x97, 0xF0, 0x00, 0xA0, 0xC9, 0x5E, 0xA8, 0x50 },
};

static const GUID simple_index_header = {
        0x33000890, 0xE5B1, 0x11CF, { 0x89, 0xF4, 0x00, 0xA0, 0xC9, 0x03, 0x49, 0xCB },
};

static const GUID ext_stream_embed_stream_header = {
        0x3afb65e2, 0x47ef, 0x40f2, { 0xac, 0x2c, 0x70, 0xa9, 0x0d, 0x71, 0xd3, 0x43}
};

static const GUID ext_stream_audio_stream = {
        0x31178c9d, 0x03e1, 0x4528, { 0xb5, 0x82, 0x3d, 0xf9, 0xdb, 0x22, 0xf5, 0x03}
};

/* I am not a number !!! This GUID is the one found on the PC used to
   generate the stream */
static const GUID my_guid = {
    0, 0, 0, { 0, 0, 0, 0, 0, 0, 0, 0 },
};

#define ASF_PACKET_FLAG_ERROR_CORRECTION_PRESENT 0x80 //1000 0000


//   ASF data packet structure
//   =========================
//
//
//  -----------------------------------
// | Error Correction Data             |  Optional
//  -----------------------------------
// | Payload Parsing Information (PPI) |
//  -----------------------------------
// | Payload Data                      |
//  -----------------------------------
// | Padding Data                      |
//  -----------------------------------


// PPI_FLAG - Payload parsing information flags
#define ASF_PPI_FLAG_MULTIPLE_PAYLOADS_PRESENT 1

#define ASF_PPI_FLAG_SEQUENCE_FIELD_IS_BYTE  0x02 //0000 0010
#define ASF_PPI_FLAG_SEQUENCE_FIELD_IS_WORD  0x04 //0000 0100
#define ASF_PPI_FLAG_SEQUENCE_FIELD_IS_DWORD 0x06 //0000 0110
#define ASF_PPI_MASK_SEQUENCE_FIELD_SIZE     0x06 //0000 0110

#define ASF_PPI_FLAG_PADDING_LENGTH_FIELD_IS_BYTE  0x08 //0000 1000
#define ASF_PPI_FLAG_PADDING_LENGTH_FIELD_IS_WORD  0x10 //0001 0000
#define ASF_PPI_FLAG_PADDING_LENGTH_FIELD_IS_DWORD 0x18 //0001 1000
#define ASF_PPI_MASK_PADDING_LENGTH_FIELD_SIZE     0x18 //0001 1000

#define ASF_PPI_FLAG_PACKET_LENGTH_FIELD_IS_BYTE  0x20 //0010 0000
#define ASF_PPI_FLAG_PACKET_LENGTH_FIELD_IS_WORD  0x40 //0100 0000
#define ASF_PPI_FLAG_PACKET_LENGTH_FIELD_IS_DWORD 0x60 //0110 0000
#define ASF_PPI_MASK_PACKET_LENGTH_FIELD_SIZE     0x60 //0110 0000

// PL_FLAG - Payload flags
#define ASF_PL_FLAG_REPLICATED_DATA_LENGTH_FIELD_IS_BYTE   0x01 //0000 0001
#define ASF_PL_FLAG_REPLICATED_DATA_LENGTH_FIELD_IS_WORD   0x02 //0000 0010
#define ASF_PL_FLAG_REPLICATED_DATA_LENGTH_FIELD_IS_DWORD  0x03 //0000 0011
#define ASF_PL_MASK_REPLICATED_DATA_LENGTH_FIELD_SIZE      0x03 //0000 0011

#define ASF_PL_FLAG_OFFSET_INTO_MEDIA_OBJECT_LENGTH_FIELD_IS_BYTE  0x04 //0000 0100
#define ASF_PL_FLAG_OFFSET_INTO_MEDIA_OBJECT_LENGTH_FIELD_IS_WORD  0x08 //0000 1000
#define ASF_PL_FLAG_OFFSET_INTO_MEDIA_OBJECT_LENGTH_FIELD_IS_DWORD 0x0c //0000 1100
#define ASF_PL_MASK_OFFSET_INTO_MEDIA_OBJECT_LENGTH_FIELD_SIZE     0x0c //0000 1100

#define ASF_PL_FLAG_MEDIA_OBJECT_NUMBER_LENGTH_FIELD_IS_BYTE  0x10 //0001 0000
#define ASF_PL_FLAG_MEDIA_OBJECT_NUMBER_LENGTH_FIELD_IS_WORD  0x20 //0010 0000
#define ASF_PL_FLAG_MEDIA_OBJECT_NUMBER_LENGTH_FIELD_IS_DWORD 0x30 //0011 0000
#define ASF_PL_MASK_MEDIA_OBJECT_NUMBER_LENGTH_FIELD_SIZE     0x30 //0011 0000

#define ASF_PL_FLAG_STREAM_NUMBER_LENGTH_FIELD_IS_BYTE  0x40 //0100 0000
#define ASF_PL_MASK_STREAM_NUMBER_LENGTH_FIELD_SIZE     0xc0 //1100 0000

#define ASF_PL_FLAG_PAYLOAD_LENGTH_FIELD_IS_BYTE  0x40 //0100 0000
#define ASF_PL_FLAG_PAYLOAD_LENGTH_FIELD_IS_WORD  0x80 //1000 0000
#define ASF_PL_MASK_PAYLOAD_LENGTH_FIELD_SIZE     0xc0 //1100 0000

#define ASF_PL_FLAG_KEY_FRAME 0x80 //1000 0000
