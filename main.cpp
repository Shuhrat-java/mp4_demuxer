#include <iostream>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
}

void printError(int err_code);

void demux3();

void mux_();

void mux2();

void read_from_file();
int reader(void *opaque, uint8_t *buffer, int size);
int writer(void *opaque, uint8_t *buffer, int size);

int64_t seeker(void *opaque, int64_t offset, int whence);

const char *muxed = "/path_to/muxed.mp4";

const char *audiofile = "/path/this.aac";

const char *videofile = "/path/this.h264";
const char *file_name = "/path/this_v.mp4";

const char *file_nam = "/path.../test.mp4";
const char *ofilename = "/path.../test.ts";

int64_t frame_index;

int main() {
    read_from_file();
    return 0;
}

void mux_() {

    av_register_all();

    AVFormatContext *main_context = nullptr;
    AVFormatContext *video_context = nullptr;
    AVFormatContext *audio_context = nullptr;

    AVPacket pkt;
    int videoindex = -1, audioindex = -1;

    int error_index = -1;

    error_index = avformat_open_input(&video_context, videofile, NULL, NULL);
    error_index = avformat_find_stream_info(video_context, NULL);

    error_index = avformat_open_input(&audio_context, audiofile, nullptr, nullptr);
    error_index = avformat_find_stream_info(audio_context, nullptr);

    error_index = avformat_alloc_output_context2(&main_context, nullptr, nullptr, muxed);

    int videoindex_out, audioindex_out;
    for (int i = 0; i < video_context->nb_streams; ++i) {

        AVStream *instream, *outstream;

        if (video_context->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoindex = i;

            instream = video_context->streams[i];
            outstream = avformat_new_stream(main_context, instream->codec->codec);

            avcodec_copy_context(outstream->codec, instream->codec);
            videoindex_out = outstream->index;
        }
    }

    for (int i = 0; i < audio_context->nb_streams; i++) {
        AVStream *instream, *out_stream;

        if (audio_context->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {

            audioindex = i;

            instream = audio_context->streams[i];

            out_stream = avformat_new_stream(main_context, instream->codec->codec);
            avcodec_copy_context(out_stream->codec, instream->codec);

            audioindex_out = out_stream->index;
        }
    }

    if (avio_open(&main_context->pb, muxed, AVIO_FLAG_WRITE) < 0) {
        printf("AVIO open error");
        return;
    }

    error_index = avformat_write_header(main_context, nullptr);
    int64_t pts_v = 0, pts_a = 0;
    while (true) {
        AVFormatContext *ifmt_ctx;
        int stream_index = 0;
        AVStream *in_stream, *out_stream;

        if (av_compare_ts(pts_v, video_context->streams[videoindex]->time_base, pts_a,
                          audio_context->streams[audioindex]->time_base) <= 0) {
            ifmt_ctx = video_context;
            stream_index = videoindex_out;

            if (av_read_frame(ifmt_ctx, &pkt) >= 0) {
                do {
                    in_stream = ifmt_ctx->streams[pkt.stream_index];
                    out_stream = main_context->streams[stream_index];

                    if (pkt.stream_index == videoindex) {
                        //FIX..No PTS (Example: Raw H.264)
                        //Simple Write PTS
                        if (pkt.pts == AV_NOPTS_VALUE) {
                            //Write PTS
                            AVRational time_base1 = in_stream->time_base;
                            //Duration between 2 frames (us)
                            int64_t calc_duration = (double) AV_TIME_BASE / av_q2d(in_stream->r_frame_rate);
                            //Parameters
                            pkt.pts = (double) (frame_index * calc_duration) /
                                      (double) (av_q2d(time_base1) * AV_TIME_BASE);
                            pkt.dts = pkt.pts;
                            pkt.duration = (double) calc_duration / (double) (av_q2d(time_base1) * AV_TIME_BASE);
                            frame_index++;
                        }

                        pts_v = pkt.pts;
                        break;
                    }
                } while (av_read_frame(ifmt_ctx, &pkt) >= 0);
            } else {
                break;
            }
        } else {
            ifmt_ctx = audio_context;
            stream_index = audioindex_out;
            if (av_read_frame(ifmt_ctx, &pkt) >= 0) {
                do {
                    in_stream = ifmt_ctx->streams[pkt.stream_index];
                    out_stream = main_context->streams[stream_index];

                    if (pkt.stream_index == audioindex) {

                        //FIX..No PTS
                        //Simple Write PTS
                        if (pkt.pts == AV_NOPTS_VALUE) {
                            //Write PTS
                            AVRational time_base1 = in_stream->time_base;
                            //Duration between 2 frames (us)
                            int64_t calc_duration = (double) AV_TIME_BASE / av_q2d(in_stream->r_frame_rate);
                            //Parameters
                            pkt.pts = (double) (frame_index * calc_duration) /
                                      (double) (av_q2d(time_base1) * AV_TIME_BASE);
                            pkt.dts = pkt.pts;
                            pkt.duration = (double) calc_duration / (double) (av_q2d(time_base1) * AV_TIME_BASE);
                            frame_index++;
                        }
                        pts_a = pkt.pts;

                        break;
                    }
                } while (av_read_frame(ifmt_ctx, &pkt) >= 0);
            } else {
                break;
            }

        }

        AVBitStreamFilterContext *h264bsfc = av_bitstream_filter_init("h264_mp4toannexb");

        av_bitstream_filter_filter(h264bsfc, in_stream->codec, NULL, &pkt.data, &pkt.size, pkt.data, pkt.size, 0);

        AVBitStreamFilterContext *aacbsfc = av_bitstream_filter_init("aac_adtstoasc");
        av_bitstream_filter_filter(aacbsfc, out_stream->codec, NULL, &pkt.data, &pkt.size, pkt.data, pkt.size, 0);



        //Convert PTS/DTS
        pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base,
                                   (AVRounding) (AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base,
                                   (AVRounding) (AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
        pkt.pos = -1;
        pkt.stream_index = stream_index;

        printf("Write 1 Packet. size:%5d\tpts:%lld\n", pkt.size, pkt.pts);
        //Write
        if (av_interleaved_write_frame(main_context, &pkt) < 0) {
            printf("Error muxing packet\n");
            break;
        }
        av_free_packet(&pkt);

    }

    av_write_trailer(main_context);

}

void printError(int err_code) {

    char a[255];

    av_strerror(err_code, a, 255);

}

void demux3() {

    int videoindex = -1, audioindex = -1;

    AVOutputFormat *outfa = NULL, *outfv = NULL;
    av_register_all();

    AVFormatContext *incontext = nullptr;
    AVFormatContext *out_v_context = nullptr;
    AVFormatContext *out_a_context = nullptr;

    int errCode = avformat_open_input(&incontext, file_name, nullptr, nullptr);
    if (errCode < 0) {
        printf("\nError opening the file");
        return;
    }

    errCode = avformat_find_stream_info(incontext, nullptr);
    if (errCode < 0) {
        printf("\nError find stream info");
        return;
    }
    errCode = avformat_alloc_output_context2(&out_v_context, nullptr, nullptr,
                                             videofile);
    if (!out_v_context) {
        printf("Could not create output context\n");
        return;
    }
    if (errCode < 0) {
        printf("\nCouldnot allocate video context");
        return;
    }
    errCode = avformat_alloc_output_context2(&out_a_context, nullptr, nullptr,
                                             audiofile);
    if (!out_a_context) {
        printf("Could not create output context\n");
        return;
    }

    if (errCode < 0) {
        printf("\nCouldnot allocate audio context");
        return;
    }

    outfv = out_v_context->oformat;
    outfa = out_a_context->oformat;

    for (int i = 0; i < incontext->nb_streams; i++) {

        AVStream *outstream = NULL;

        AVStream *instream = incontext->streams[i];

        if (incontext->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {

            videoindex = i;

            outstream = avformat_new_stream(out_v_context, instream->codec->codec);


            errCode = avcodec_copy_context(outstream->codec, instream->codec);

            if (errCode < 0) {
                printf("Could not copy context");
                return;
            }

            outstream->codec->codec_tag = 0;

            if (out_v_context->oformat->flags & AVFMT_GLOBALHEADER) {
                outstream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
            }

        } else if (incontext->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {

            audioindex = i;

            outstream = avformat_new_stream(out_a_context, instream->codec->codec);

            errCode = avcodec_copy_context(outstream->codec, instream->codec);

            if (errCode < 0) {
                printf("Could not copy context");
                return;
            }

            outstream->codec->codec_tag = 0;

            if (out_a_context->oformat->flags & AVFMT_GLOBALHEADER) {
                outstream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
            }

        }
    }

    av_dump_format(incontext, 0, file_name, 0);
    av_dump_format(out_v_context, 0, videofile, 1);
    av_dump_format(out_a_context, 0, audiofile, 1);

    if (!(outfv->flags & AVFMT_NOFILE)) {
        if (avio_open(&out_v_context->pb, videofile, AVIO_FLAG_WRITE) < 0) {
            printf("Error opening audio file");
        }
    }

    if (!(outfa->flags & AVFMT_NOFILE)) {
        if (avio_open(&out_a_context->pb, audiofile, AVIO_FLAG_WRITE) < 0) {
            printf("Error opening audio file");
        }
    }


    errCode = avformat_write_header(out_v_context, nullptr);
    if (errCode < 0) {
        printf("ERror writing header in video file");
        return;
    }

    errCode = avformat_write_header(out_a_context, nullptr);
    if (errCode < 0) {
        printf("ERror writing header in video file");
        return;
    }
    AVPacket packet;
    AVBitStreamFilterContext *bsfc = av_bitstream_filter_init("h264_mp4toannexb");
    while (av_read_frame(incontext, &packet) >= 0) {

        AVFormatContext *output_context;
        AVStream *out_stream, *in_stream;

        in_stream = incontext->streams[packet.stream_index];

        if (packet.stream_index == videoindex) {

            output_context = out_v_context;

            out_stream = out_v_context->streams[0];


            av_bitstream_filter_filter(bsfc, in_stream->codec, NULL, &packet.data, &packet.size, packet.data,
                                       packet.size, 0);

        } else if (packet.stream_index == audioindex) {

            output_context = out_a_context;

            out_stream = out_a_context->streams[0];

        }
        if (!out_stream) {
            printf("Failed allocating output stream\n");
            return;
        }

        packet.pts = av_rescale_q_rnd(packet.pts, in_stream->time_base, out_stream->time_base,
                                      (AVRounding) (AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        packet.dts = av_rescale_q_rnd(packet.dts, in_stream->time_base, out_stream->time_base,
                                      (AVRounding) (AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        packet.duration = av_rescale_q(packet.duration, in_stream->time_base, out_stream->time_base);

        packet.pos = -1;
        packet.stream_index = 0;

        av_interleaved_write_frame(output_context, &packet);

        av_free_packet(&packet);

    }

    av_write_trailer(out_v_context);
    av_write_trailer(out_a_context);

    avformat_free_context(out_a_context);
    avformat_free_context(out_v_context);
    avformat_close_input(&incontext);

}

void mux2() {

    AVOutputFormat *ofmt = NULL;
    //Input AVFormatContext and Output AVFormatContext
    AVFormatContext *ifmt_ctx_v = NULL, *ifmt_ctx_a = NULL, *ofmt_ctx = NULL;
    AVPacket pkt;
    int ret, i;
    int videoindex_v = -1, videoindex_out = -1;
    int audioindex_a = -1, audioindex_out = -1;
    int frame_index = 0;
    int64_t cur_pts_v = 0, cur_pts_a = 0;

    const char *in_filename_v = videofile;

    const char *in_filename_a = audiofile;

    const char *out_filename = muxed;

    av_register_all();
    //Input
    if ((ret = avformat_open_input(&ifmt_ctx_v, in_filename_v, 0, 0)) < 0) {
        printf("Could not open input file.");
        return;
    }
    if ((ret = avformat_find_stream_info(ifmt_ctx_v, 0)) < 0) {
        printf("Failed to retrieve input stream information");
        return;
    }

    if ((ret = avformat_open_input(&ifmt_ctx_a, in_filename_a, 0, 0)) < 0) {
        printf("Could not open input file.");
        return;
    }
    if ((ret = avformat_find_stream_info(ifmt_ctx_a, 0)) < 0) {
        printf("Failed to retrieve input stream information");
        return;
    }
    printf("===========Input Information==========\n");
    av_dump_format(ifmt_ctx_v, 0, in_filename_v, 0);
    av_dump_format(ifmt_ctx_a, 0, in_filename_a, 0);
    printf("======================================\n");
    //Output
    avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, out_filename);
    if (!ofmt_ctx) {
        printf("Could not create output context\n");
        ret = AVERROR_UNKNOWN;
        return;
    }
    ofmt = ofmt_ctx->oformat;

    for (i = 0; i < ifmt_ctx_v->nb_streams; i++) {
        //Create output AVStream according to input AVStream
        if (ifmt_ctx_v->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
            AVStream *in_stream = ifmt_ctx_v->streams[i];
            AVStream *out_stream = avformat_new_stream(ofmt_ctx, in_stream->codec->codec);
            videoindex_v = i;
            if (!out_stream) {
                printf("Failed allocating output stream\n");
                ret = AVERROR_UNKNOWN;
                return;
            }
            videoindex_out = out_stream->index;
            //Copy the settings of AVCodecContext
            if (avcodec_copy_context(out_stream->codec, in_stream->codec) < 0) {
                printf("Failed to copy context from input to output stream codec context\n");
                return;
            }
            out_stream->codec->codec_tag = 0;
            if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
                out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
            break;
        }
    }

    for (i = 0; i < ifmt_ctx_a->nb_streams; i++) {
        //Create output AVStream according to input AVStream
        if (ifmt_ctx_a->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
            AVStream *in_stream = ifmt_ctx_a->streams[i];
            AVStream *out_stream = avformat_new_stream(ofmt_ctx, in_stream->codec->codec);
            audioindex_a = i;
            if (!out_stream) {
                printf("Failed allocating output stream\n");
                ret = AVERROR_UNKNOWN;
                return;
            }
            audioindex_out = out_stream->index;
            //Copy the settings of AVCodecContext
            if (avcodec_copy_context(out_stream->codec, in_stream->codec) < 0) {
                printf("Failed to copy context from input to output stream codec context\n");
                return;
            }
            out_stream->codec->codec_tag = 0;
            if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
                out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;

            break;
        }
    }

    printf("==========Output Information==========\n");
    av_dump_format(ofmt_ctx, 0, out_filename, 1);
    printf("======================================\n");
    //Open output file
    if (!(ofmt->flags & AVFMT_NOFILE)) {
        if (avio_open(&ofmt_ctx->pb, out_filename, AVIO_FLAG_WRITE) < 0) {
            printf("Could not open output file '%s'", out_filename);
            return;
        }
    }
    //Write file header
    if (avformat_write_header(ofmt_ctx, NULL) < 0) {
        printf("Error occurred when opening output file\n");
        return;
    }


    AVBitStreamFilterContext *h264bsfc = av_bitstream_filter_init("h264_mp4toannexb");

    AVBitStreamFilterContext *aacbsfc = av_bitstream_filter_init("aac_adtstoasc");


    while (1) {
        AVFormatContext *ifmt_ctx;
        int stream_index = 0;
        AVStream *in_stream, *out_stream;

        //Get an AVPacket
        if (av_compare_ts(cur_pts_v, ifmt_ctx_v->streams[videoindex_v]->time_base, cur_pts_a,
                          ifmt_ctx_a->streams[audioindex_a]->time_base) <= 0) {
            ifmt_ctx = ifmt_ctx_v;
            stream_index = videoindex_out;

            if (av_read_frame(ifmt_ctx, &pkt) >= 0) {
                do {
                    in_stream = ifmt_ctx->streams[pkt.stream_index];
                    out_stream = ofmt_ctx->streams[stream_index];

                    if (pkt.stream_index == videoindex_v) {
                        //FIX..No PTS (Example: Raw H.264)
                        //Simple Write PTS
                        if (pkt.pts == AV_NOPTS_VALUE) {
                            //Write PTS
                            AVRational time_base1 = in_stream->time_base;
                            //Duration between 2 frames (us)
                            int64_t calc_duration = (double) AV_TIME_BASE / av_q2d(in_stream->r_frame_rate);
                            //Parameters
                            pkt.pts = (double) (frame_index * calc_duration) /
                                      (double) (av_q2d(time_base1) * AV_TIME_BASE);
                            pkt.dts = pkt.pts;
                            pkt.duration = (double) calc_duration / (double) (av_q2d(time_base1) * AV_TIME_BASE);
                            frame_index++;
                        }

                        cur_pts_v = pkt.pts;
                        break;
                    }
                } while (av_read_frame(ifmt_ctx, &pkt) >= 0);
            } else {
                break;
            }
        } else {
            ifmt_ctx = ifmt_ctx_a;
            stream_index = audioindex_out;
            if (av_read_frame(ifmt_ctx, &pkt) >= 0) {
                do {
                    in_stream = ifmt_ctx->streams[pkt.stream_index];
                    out_stream = ofmt_ctx->streams[stream_index];

                    if (pkt.stream_index == audioindex_a) {

                        //FIX..No PTS
                        //Simple Write PTS
                        if (pkt.pts == AV_NOPTS_VALUE) {
                            //Write PTS
                            AVRational time_base1 = in_stream->time_base;
                            //Duration between 2 frames (us)
                            int64_t calc_duration = (double) AV_TIME_BASE / av_q2d(in_stream->r_frame_rate);
                            //Parameters
                            pkt.pts = (double) (frame_index * calc_duration) /
                                      (double) (av_q2d(time_base1) * AV_TIME_BASE);
                            pkt.dts = pkt.pts;
                            pkt.duration = (double) calc_duration / (double) (av_q2d(time_base1) * AV_TIME_BASE);
                            frame_index++;
                        }
                        cur_pts_a = pkt.pts;

                        break;
                    }
                } while (av_read_frame(ifmt_ctx, &pkt) >= 0);
            } else {
                break;
            }

        }
        av_bitstream_filter_filter(h264bsfc, in_stream->codec, NULL, &pkt.data, &pkt.size, pkt.data, pkt.size, 0);

        av_bitstream_filter_filter(aacbsfc, out_stream->codec, NULL, &pkt.data, &pkt.size, pkt.data, pkt.size, 0);



        //Convert PTS/DTS
        pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base,
                                   (AVRounding) (AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base,
                                   (AVRounding) (AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
        pkt.pos = -1;
        pkt.stream_index = stream_index;

        printf("Write 1 Packet. size:%5d\tpts:%lld\n", pkt.size, pkt.pts);
        //Write
        if (av_interleaved_write_frame(ofmt_ctx, &pkt) < 0) {
            printf("Error muxing packet\n");
            break;
        }
        av_free_packet(&pkt);

    }
    //Write file trailer
    av_write_trailer(ofmt_ctx);

    av_bitstream_filter_close(h264bsfc);

    av_bitstream_filter_close(aacbsfc);

    avformat_close_input(&ifmt_ctx_v);
    avformat_close_input(&ifmt_ctx_a);
    /* close output */
    if (ofmt_ctx && !(ofmt->flags & AVFMT_NOFILE))
        avio_close(ofmt_ctx->pb);
    avformat_free_context(ofmt_ctx);
    if (ret < 0 && ret != AVERROR_EOF) {
        printf("Error occurred.\n");
        return;
    }

}

void read_from_file() {

    av_register_all();

//    fv = fopen("/home/prodvd/Videos/onlyvideo.h264", "w");
//
//    fa = fopen("/home/prodvd/Videos/onlyaudio.aac", "w");

    AVFormatContext *ifmt_ctx = nullptr;
    AVFormatContext *ofmt_ctx = nullptr;

    int err_code = avformat_open_input(&ifmt_ctx, file_nam, nullptr, nullptr);

    if (err_code != 0) {

        char err[255];

        av_strerror(err_code, err, 255);

        printf("\nError: %s", err);

        return;
    }

    //av_dump_format(ifmt_ctx, 0, nullptr, 0);

    err_code = avformat_find_stream_info(ifmt_ctx, nullptr);
    if (err_code < 0) {

        char err[255];

        av_strerror(err_code, err, 255);

        printf("\nError: %s", err);

        return;
    }

    int video_stream = -1, audio_stream = -1;

    for (int i = 0; i < ifmt_ctx->nb_streams; ++i) {
        if (ifmt_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream = i;
            printf("\nVideo stream index:%d", video_stream);
            break;
        }
    }

    for (int i = 0; i < ifmt_ctx->nb_streams; ++i) {
        if (ifmt_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
            audio_stream = i;
            printf("\nAudio stream index:%d", audio_stream);
            break;
        }
    }

    //  If all ok
    if (video_stream >= 0 && audio_stream >= 0) {
        //  Allocate output mpegts container
        err_code = avformat_alloc_output_context2(&ofmt_ctx, nullptr, "mpegts", ofilename);
        if (err_code != 0) {
            return;
        }
        FILE *fx = fopen(ofilename, "w");
        int size = 1880;
        uint8_t *buffer = (uint8_t *) av_malloc(size);

        //  Set custion output
        ofmt_ctx->pb = avio_alloc_context(buffer, size, 1, fx, nullptr, writer, seeker);


        // Add streams
        AVStream *ovstream = avformat_new_stream(ofmt_ctx, nullptr);
        AVStream *oastream = avformat_new_stream(ofmt_ctx, nullptr);

        //  Get input stream pointers
        AVStream *ivstream = ifmt_ctx->streams[video_stream];
        AVStream *iastream = ifmt_ctx->streams[audio_stream];

        //  Configure output video stream and codec
        avcodec_copy_context(ovstream->codec, ivstream->codec);
        avcodec_copy_context(oastream->codec, iastream->codec);


        //  Write header
        err_code = avformat_write_header(ofmt_ctx, nullptr);
        if (err_code != 0)
            return;

        AVPacket packet;

        av_init_packet(&packet);

        AVBitStreamFilterContext * bsfc = av_bitstream_filter_init("h264_mp4toannexb");
        if(bsfc == nullptr )
            printf("Bit stream filter could not be found\n");

        //  Write data
        while (av_read_frame(ifmt_ctx, &packet) >= 0) {

            AVPacket newPkt = packet;
            if(bsfc && packet.stream_index == ivstream->index)
            {
                err_code = av_bitstream_filter_filter(bsfc, ovstream->codec, nullptr, &newPkt.data, &newPkt.size, packet.data, packet.size, packet.flags & AV_PKT_FLAG_KEY);
            }
            av_write_frame(ofmt_ctx, &newPkt);
            av_free_packet(&packet);
        }

        //  Write trailer
        av_write_trailer(ofmt_ctx);
    }


//    av_dump_format(ifmt_ctx, 0, nullptr, 0);

    avformat_close_input(&ifmt_ctx);
}

void read_customio() {

    FILE *opaque = fopen(file_name, "r");

    av_register_all();

    AVFormatContext *ifmt_ctx = avformat_alloc_context();

    int buffer_size = 2048;

    uint8_t *buffer = (uint8_t *) av_malloc(buffer_size);

    ifmt_ctx->pb = avio_alloc_context(buffer, buffer_size, 0, opaque, reader, nullptr, seeker);

    int err_code = avformat_open_input(&ifmt_ctx, nullptr, nullptr, nullptr);

    if (err_code != 0) {

        char err[255];

        av_strerror(err_code, err, 255);

        printf("\nError: %s", err);

        return;
    }

    av_dump_format(ifmt_ctx, 0, nullptr, 0);

    err_code = avformat_find_stream_info(ifmt_ctx, nullptr);

    if (err_code < 0) {

        char err[255];

        av_strerror(err_code, err, 255);

        printf("\nError: %s", err);

        return;
    }

    av_dump_format(ifmt_ctx, 0, nullptr, 0);

    avformat_close_input(&ifmt_ctx);

}

int reader(void *opaque, uint8_t *buffer, int size) {

    FILE *fx = (FILE *) opaque;

    int read = fread(buffer, 1, size, fx);

    return read;

}

int64_t seeker(void *opaque, int64_t offset, int whence) {

    FILE *fx = (FILE *) opaque;

    printf("\nSeeking   offset:%lld  whence:%d", offset, whence);

    if (whence == 0x10000) {

        size_t cur_pos = ftell(fx);

        fseek(fx, 0, SEEK_END);

        uint64_t file_size = ftell(fx);

        fseek(fx, cur_pos, SEEK_SET);

        return file_size;
    }
    int64_t seek = fseek(fx, offset, whence);

    return seek;

}

int writer(void *opaque, uint8_t *buffer, int size) {

    FILE *fx = (FILE *) opaque;
    printf("\nWriting:%d", size);
    if (fx) {

        return fwrite(buffer, 1, size, fx);
    }
    return -1;
}