extern "C" {
  #include <libavcodec/avcodec.h>
  #include <libavformat/avformat.h>
  #include <libavutil/timestamp.h>
  #include <libavutil/opt.h>
}
#include <iostream>
#include <cstring>

using namespace std;

struct InputParams
{
  char *input_filename;
  char *output_filename;
  double start_time;
  double end_time;
  char *output_format;
};

struct StreamingContext {
  AVStream *avstream;
  AVCodec *avc;
  AVCodecContext *avcc;
  int stream_index;
};

struct MediaContext {
  AVFormatContext *avfc;
  StreamingContext *video_sc;
  StreamingContext *audio_sc;
};

int open_media(const char *in_filename, AVFormatContext **avfc) {
  *avfc = avformat_alloc_context();
  if (!*avfc) {
    cout << "Failed to alloc memory for format" << endl;
    return -1;
  }

  cout << (*avfc)->flags << "flags" << endl;

  cout << "in_fliename: " << in_filename << endl;

  if (avformat_open_input(avfc, in_filename, NULL, NULL) != 0) {
    cout << "Failed to open input file" << endl;
    return -1;
  }

  if (avformat_find_stream_info(*avfc, NULL) < 0) {
    cout << "Failed to get stream info" << endl;
    return -1;
  }

  return 0;
}

int set_stream_codec(AVStream *avs, AVCodec **avc, AVCodecContext **avcc) {
  *avc = (AVCodec *)avcodec_find_decoder(avs->codecpar->codec_id);
  if (!*avc) {
    cout << "Failed to find the codec" << endl;
    return -1;
  }

  *avcc = avcodec_alloc_context3(*avc);
  if (!*avcc) {
    cout << "Failed to alloc memory for codec context" << endl;
    return -1;
  }

  if ((avcodec_parameters_to_context(*avcc, avs->codecpar)) < 0) {
    cout << "Failed to fill codec context" << endl;
    return -1;
  }

  if (avcodec_open2(*avcc, *avc, NULL) < 0) {
    cout << "Failed to open codec" << endl;
    return -1;
  }
  return 0;
}

int set_input_streaming_context(MediaContext *mc) {
  for (int i = 0; i < mc->avfc->nb_streams; i++) {
    if (mc->avfc->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
      mc->video_sc->avstream = mc->avfc->streams[i];
      mc->video_sc->stream_index = i;

      if (set_stream_codec(mc->video_sc->avstream, &mc->video_sc->avc, &mc->video_sc->avcc)) return -1;
    } else if (mc->avfc->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
      mc->audio_sc->avstream = mc->avfc->streams[i];
      mc->audio_sc->stream_index = i;

      if (set_stream_codec(mc->audio_sc->avstream, &mc->audio_sc->avc, &mc->audio_sc->avcc)) return -1;
    } else {
      cout << "Skipping streams other than audio and video" << endl;
    }
  }

  return 0;
}

int set_output_video_streaming_context(MediaContext *output_mc, MediaContext *input_mc, AVRational input_framerate, char *codec_name)
{
  output_mc->video_sc->avstream = avformat_new_stream(output_mc->avfc, NULL);
  output_mc->video_sc->avstream->time_base = input_mc->video_sc->avstream->time_base;
  output_mc->video_sc->avstream->codecpar->codec_tag = 0;
  avcodec_parameters_copy(output_mc->video_sc->avstream->codecpar, input_mc->video_sc->avstream->codecpar);
  return 0;

  // 设置编码器
  output_mc->video_sc->avc = (AVCodec *)avcodec_find_encoder_by_name(codec_name);

  if (!output_mc->video_sc->avc) {
    cout << "Could not find the proper codec" << endl;
    return -1;
  }
  output_mc->video_sc->avcc = avcodec_alloc_context3(output_mc->video_sc->avc);
  if (!output_mc->video_sc->avcc) {
    cout << "Could not allocated memory for codec context";
    return -1;
  }
  av_opt_set(output_mc->video_sc->avcc->priv_data, "preset", "fast", 0);
  
  // output_mc->video_sc->avcc->height = input_mc->video_sc->avcc->height;
  // output_mc->video_sc->avcc->width = input_mc->video_sc->avcc->width;
  // output_mc->video_sc->avcc->sample_aspect_ratio = input_mc->video_sc->avcc->sample_aspect_ratio;
  // if (output_mc->video_sc->avc->pix_fmts)
  //   output_mc->video_sc->avcc->pix_fmt = output_mc->video_sc->avc->pix_fmts[0];
  // else
  //   output_mc->video_sc->avcc->pix_fmt = input_mc->video_sc->avcc->pix_fmt;
  
  // output_mc->video_sc->avcc->bit_rate = 2 * 1000 * 1000;
  // output_mc->video_sc->avcc->rc_buffer_size = 4 * 1000 * 1000;
  // output_mc->video_sc->avcc->rc_max_rate = 2 * 1000 * 1000;
  // output_mc->video_sc->avcc->rc_min_rate = 2.5 * 1000 * 1000;

  // output_mc->video_sc->avcc->time_base = input_mc->video_sc->avcc->time_base;
  output_mc->video_sc->avstream->time_base = input_mc->video_sc->avstream->time_base;

  if(avcodec_open2(output_mc->video_sc->avcc, output_mc->video_sc->avc, NULL) < 0) {
    cout << "Could not open the codec" << endl;
    return -1;
  }
  avcodec_parameters_from_context(output_mc->video_sc->avstream->codecpar, output_mc->video_sc->avcc);
  return 0;
}

int set_output_audio_streaming_context(MediaContext *output_mc, MediaContext *input_mc, char *codec_name)
{
  output_mc->audio_sc->avstream = avformat_new_stream(output_mc->avfc, NULL);
  output_mc->audio_sc->avstream->time_base = output_mc->audio_sc->avstream->time_base;
  output_mc->audio_sc->avstream->codecpar->codec_tag = 0;
  avcodec_parameters_copy(output_mc->audio_sc->avstream->codecpar, input_mc->audio_sc->avstream->codecpar);
  return 0;
  output_mc->audio_sc->avc = (AVCodec *)avcodec_find_encoder_by_name(codec_name);
  if (!output_mc->audio_sc->avc) {
    cout << "Could not find the proper codec" << endl;
    return -1;
  }
  output_mc->audio_sc->avcc = avcodec_alloc_context3(output_mc->audio_sc->avc);
  if (!output_mc->audio_sc->avcc) {
    cout << "Could not allocated memory for codec context" << endl;
    return -1;
  }
  int OUTPUT_CHANNELS = 2;
  int OUTPUT_BIT_RATE = 196000;
  output_mc->audio_sc->avcc->ch_layout.nb_channels = OUTPUT_CHANNELS;
  output_mc->audio_sc->avcc->bit_rate = OUTPUT_BIT_RATE;
  av_channel_layout_default(&output_mc->audio_sc->avcc->ch_layout, OUTPUT_CHANNELS);
  // output_mc->audio_sc->avcc->sample_rate = sample_rate;
  output_mc->audio_sc->avcc->sample_fmt = output_mc->audio_sc->avc->sample_fmts[0];
  output_mc->audio_sc->avcc->bit_rate = OUTPUT_BIT_RATE;
  // output_mc->audio_sc->avcc->time_base = (AVRational){1, sample_rate};

  output_mc->audio_sc->avcc->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;

  if (avcodec_open2(output_mc->audio_sc->avcc, output_mc->audio_sc->avc, NULL) < 0) {
    cout << "Could not open the codec" << endl;
    return -1;
  }

  avcodec_parameters_from_context(output_mc->audio_sc->avstream->codecpar, output_mc->audio_sc->avcc);
  return 0;

}

/**
 * 将视频packet数据写入到文件中
 */
int write_video_packet_into_file(MediaContext *input_mc, MediaContext *output_mc, AVPacket *packet) {
  int response = avcodec_send_packet(input_mc->video_sc->avcc, packet);
  AVFrame *input_frame = av_frame_alloc();

  if (response < 0) {
    cout << "Error while sending packet to codec: " << av_err2str(response) << endl;
    return -1;
  }

  while (response >= 0)
  {
    response = avcodec_receive_frame(input_mc->video_sc->avcc, input_frame);
    if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
      break;
    } else if (response < 0) {
      cout << "Error while receiving frame from codec: " << av_err2str(response) << endl;
      return response;
    }
    if (response >= 0) {

      if (input_frame) input_frame->pict_type = AV_PICTURE_TYPE_NONE;
      AVPacket *output_packet = av_packet_alloc();
      if (!output_packet) {
        cout << "Could not allocate memory for output packet" << endl;
        return -1;
      }
      int response2 = avcodec_send_frame(output_mc->video_sc->avcc, input_frame);
      while (response2 >= 0)
      {
        response2 = avcodec_receive_packet(output_mc->video_sc->avcc, output_packet);
        if (response2 == AVERROR(EAGAIN) || response2 == AVERROR_EOF) {
          break;
        } else if (response2 < 0) {
          cout << "Error while receiving packet from codec: " << av_err2str(response2) << endl;
          return -1;
        }
        output_packet->stream_index = input_mc->video_sc->stream_index;
        response2 = av_interleaved_write_frame(output_mc->avfc, output_packet);
        if (response2 != 0) {
          cout << "Error while writing frame: " << av_err2str(response2) << endl;
          return -1;
        }
      }
      av_packet_unref(output_packet);
      av_packet_free(&output_packet);
    }

    av_frame_unref(input_frame);
  }
  return 0;
}

/**
 * 将音频packet数据写入到文件中
 */
int write_audio_packet_into_file(MediaContext *input_mc, MediaContext *output_mc, AVPacket *packet) {
  int response = avcodec_send_packet(input_mc->audio_sc->avcc, packet);
  AVFrame *input_frame = av_frame_alloc();

  if (response < 0) {
    cout << "Error while sending packet to codec: " << av_err2str(response) << endl;
    return -1;
  }

  while (response >= 0)
  {
    response = avcodec_receive_frame(input_mc->audio_sc->avcc, input_frame);
    if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
      break;
    } else if (response < 0) {
      cout << "Error while receiving frame from codec: " << av_err2str(response) << endl;
      return response;
    }
    if (response >= 0) {

      AVPacket *output_packet = av_packet_alloc();
      if (!output_packet) {
        cout << "Could not allocate memory for output packet" << endl;
        return -1;
      }
      int response2 = avcodec_send_frame(output_mc->audio_sc->avcc, input_frame);
      while (response2 >= 0)
      {
        response2 = avcodec_receive_packet(output_mc->audio_sc->avcc, output_packet);
        if (response2 == AVERROR(EAGAIN) || response2 == AVERROR_EOF) {
          break;
        } else if (response2 < 0) {
          cout << "Error while receiving packet from codec: " << av_err2str(response2) << endl;
          return -1;
        }
        output_packet->stream_index = input_mc->audio_sc->stream_index;
        response2 = av_interleaved_write_frame(output_mc->avfc, output_packet);
        if (response2 != 0) {
          cout << "Error while writing frame: " << av_err2str(response2) << endl;
          return -1;
        }
      }
      av_packet_unref(output_packet);
      av_packet_free(&output_packet);
    }

    av_frame_unref(input_frame);
  }
  return 0;
}

/**
 * 将MediaContext所占用的内存释放
 */
int free_mc(MediaContext *mc) {
  avformat_free_context(mc->avfc);
  avcodec_free_context(&mc->video_sc->avcc);
  avcodec_free_context(&mc->audio_sc->avcc);
  free(mc);
}

/**
 * 将HH:MM:SS的时间转换成秒数
 */
double parse_time_string_to_seconds(const char *time_str) {  
    int hours, minutes, seconds;  
    // 使用sscanf从字符串中读取小时、分钟和秒  
    if (sscanf(time_str, "%d:%d:%d", &hours, &minutes, &seconds) != 3) {  
        // 如果读取失败，返回错误值或处理错误  
        fprintf(stderr, "Invalid time format: %s\n", time_str);  
        exit(EXIT_FAILURE); // 或者返回某个错误代码，这里选择退出程序  
    }
  
    // 将小时、分钟、秒转换为总秒数  
    double total_seconds = hours * 3600 + minutes * 60 + seconds;  
  
    return total_seconds;  
}

/**
 * 解析输入的参数
 */
int parse_params(int argc, char *argv[], InputParams *input_params) {
  int i;
  for (i = 0; i < argc; i++) {
    if (strcmp(argv[i], "-i") == 0) {
      cout <<  argv[i + 1] << endl;
      input_params->input_filename = argv[i + 1];
      i++;
    } else if (strcmp(argv[i], "-ss") == 0) {
      input_params->start_time = parse_time_string_to_seconds(argv[i + 1]);
      i++;
    } else if (strcmp(argv[i], "-to") == 0) {
      input_params->end_time = parse_time_string_to_seconds(argv[i + 1]);
      i++;
    } else if (strcmp(argv[i], "-f") == 0) {
      input_params->output_format = argv[i + 1];
      i++;
    } else
      input_params->output_filename = argv[i];
  }
  if (!input_params->input_filename) {
    cout << "no input filename" << endl;
    return -1;
  }

  if (!input_params->output_filename) {
    cout << "no output filename" << endl;
    return -1;
  }

  return 0;
}

int main(int argc, char *argv[])
{

  InputParams *input_params = (InputParams *)calloc(1, sizeof(InputParams));
  if (parse_params(argc, argv, input_params)) return -1;

  MediaContext *input_mc = (MediaContext *) calloc(1, sizeof(MediaContext));
  MediaContext *output_mc = (MediaContext *) calloc(1, sizeof(MediaContext));

  input_mc->video_sc = (StreamingContext *) calloc(1, sizeof(StreamingContext));
  input_mc->audio_sc = (StreamingContext *) calloc(1, sizeof(StreamingContext));
  output_mc->video_sc = (StreamingContext *) calloc(1, sizeof(StreamingContext));
  output_mc->audio_sc = (StreamingContext *) calloc(1, sizeof(StreamingContext));

  input_mc->video_sc->stream_index = -1;
  input_mc->audio_sc->stream_index = -1;
  output_mc->video_sc->stream_index = -1;
  output_mc->audio_sc->stream_index = -1;

  output_mc->avfc = avformat_alloc_context();
  
  if (open_media(input_params->input_filename, &input_mc->avfc)) return -1;

  if (set_input_streaming_context(input_mc)) return -1;

  cout << "input_params->output_filename: " << input_params->output_filename << endl;

  avformat_alloc_output_context2(&output_mc->avfc, NULL, NULL, input_params->output_filename);

  if (!output_mc->avfc) {
    cout << "Could not allocate memory for output format" << endl;
    return -1;
  }

  if (input_mc->video_sc->stream_index != -1) {
    AVRational input_framerate = av_guess_frame_rate(input_mc->avfc, input_mc->video_sc->avstream, NULL);
    set_output_video_streaming_context(output_mc, input_mc, input_framerate, "libx264");
  }

  if (!input_mc->audio_sc->stream_index != -1) {
    set_output_audio_streaming_context(output_mc, input_mc, "aac");
  }

  if (output_mc->avfc->oformat->flags & AVFMT_GLOBALHEADER)
    output_mc->avfc->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
  
  if (!(output_mc->avfc->oformat->flags & AVFMT_NOFILE))
    if (avio_open2(&output_mc->avfc->pb, input_params->output_filename, AVIO_FLAG_WRITE, NULL, NULL) < 0) {
      cout << "Could not open the ouput file" << endl;
      return -1;
    }
  
  // AVDictionary *muxer_opts = NULL;

  // if (sp.muxer_opt_key && sp.muxer_opt_value)
  //   av_dict_set(&muxer_opts, sp.muxer_opt_key, sp.muxer_opt_value, 0);
  
  if (avformat_write_header(output_mc->avfc, NULL) < 0) {
    cout << "An error occurred when opening output file" << endl;
    return -1;
  }

  AVPacket *input_packet = av_packet_alloc();

  if (!input_packet) {
    cout << "Failed to allocated memory for AVPacket" << endl;
    return -1;
  }

  int64_t begin_pts = 0;
  int64_t begin_audio_pts = 0;
  int64_t end_pts = 0;

  if (input_mc->video_sc->avstream && input_mc->video_sc->avstream->time_base.num > 0) {
    begin_pts = input_params->start_time / av_q2d(input_mc->video_sc->avstream->time_base);
    end_pts = input_params->end_time / av_q2d(input_mc->video_sc->avstream->time_base);
  }

  if (input_mc->audio_sc->avstream && input_mc->audio_sc->avstream->time_base.num > 0)
    begin_audio_pts = input_params->start_time / av_q2d(input_mc->audio_sc->avstream->time_base);
  
  // 执行seek，查找要截取时间段开始的位置
  int res = av_seek_frame(input_mc->avfc, input_mc->video_sc->stream_index, begin_pts, AVSEEK_FLAG_BACKWARD | AVSEEK_FLAG_FRAME);
  if (res < 0) {
    cout << "Error while seeking: " << av_err2str(res) << endl;
    return -1;
  }

  while (av_read_frame(input_mc->avfc, input_packet) >= 0)
  {
    AVStream * out_stream = nullptr;
    AVStream * in_stream = nullptr;
    int64_t offset_pts = 0;

    if (input_packet->stream_index == input_mc->video_sc->stream_index) {

      if (input_packet->pts > end_pts) {
        av_packet_unref(input_packet);
        break;
      }

      cout << "video_packet->pts: " << input_packet->pts * av_q2d(input_mc->video_sc->avstream->time_base) << endl;
      // write_video_packet_into_file(input_mc, output_mc, input_packet);
      out_stream = output_mc->video_sc->avstream;
      offset_pts = begin_pts;

    } else if (input_packet->stream_index == input_mc->audio_sc->stream_index) {

      out_stream = output_mc->audio_sc->avstream;
      offset_pts = begin_audio_pts;
      // cout << "audio_packet->pts: " << input_packet->pts * av_q2d(input_mc->audio_sc->avstream->time_base) << endl;
      // write_audio_packet_into_file(input_mc, output_mc, input_packet);
    }

    if (input_packet->stream_index == input_mc->video_sc->stream_index || input_packet->stream_index == input_mc->audio_sc->stream_index) {
      in_stream = input_mc->avfc->streams[input_packet->stream_index];
      input_packet->pts = av_rescale_q_rnd(input_packet->pts - offset_pts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_INF | AV_ROUND_PASS_MINMAX));
      input_packet->dts = av_rescale_q_rnd(input_packet->dts - offset_pts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_INF | AV_ROUND_PASS_MINMAX));
      input_packet->duration = av_rescale_q(input_packet->duration, in_stream->time_base, out_stream->time_base);
      input_packet->pos = -1;
    }
    av_interleaved_write_frame(output_mc->avfc, input_packet);
    av_packet_unref(input_packet);
  }

  av_write_trailer(output_mc->avfc);

  // if (muxer_opts != NULL) {
  //   av_dict_free(&muxer_opts);
  //   muxer_opts = NULL;
  // }

  if (input_packet != NULL) {
    av_packet_free(&input_packet);
    input_packet = NULL;
  }

  avformat_close_input(&output_mc->avfc);

  free_mc(output_mc);
  free_mc(input_mc);

  return 0;

}