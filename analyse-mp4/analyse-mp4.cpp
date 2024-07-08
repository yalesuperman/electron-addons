extern "C" {
  #include <libavformat/avformat.h>
  #include <libavcodec/avcodec.h>
}

#include <string>
#include <iostream>
#include <nlohmann/json.hpp>
#include <sstream>

#ifndef AV_RB16
// 获取2个字节的数据所表示的数字的大小
#define AV_RB16(x) ((((const uint8_t*)(x))[0] << 8) | ((const uint8_t*)(x))[1])
#endif

using json = nlohmann::json;

struct AVCodecData {
  AVCodec *avcodec;
  AVCodecContext *avcodec_context;
};

class AnalyseMp4
{
private:
  /** 需要进行分析的文件 */
  std::string input_filename_;
  AVFormatContext *avformat_context_;
  json analyseData;
  int8_t video_stream_index;
  /** 打开输入文件*/
  int open_media();
  /** 解码流数据*/
  int handle_streams();
  int h264_mp4toannexb(AVPacket *input_packet);
  int h264_extradata_to_annexb(AVStream *avstream, AVPacket *sps_pkt, AVPacket *pps_pkt, int padding);
  int add_nalu_to_analysedata(uint8_t nal_type, int32_t nal_size, const uint8_t *nal_data, bool add_start_code = false);
public:
  AnalyseMp4(std::string input_filename);
  std::string getAnalyseData();
  ~AnalyseMp4();
  int analyse();
};

int AnalyseMp4::open_media() {
  avformat_context_ = avformat_alloc_context();
  if (!avformat_context_) {
    std::cout << "分配avformat内存失败" << std::endl;;
    return -1;
  }
  std::cout << input_filename_.c_str() << std::endl;
  int res = avformat_open_input(&avformat_context_, input_filename_.c_str(), NULL, NULL);
  if (res != 0) {
    std::cout << "Could not open the file: " << av_err2str(res) << std::endl;
    return -1;
  }
  if (avformat_find_stream_info(avformat_context_, NULL) < 0) {
    std::cout << "无法获得流信息" << std::endl;
    return -1;
  }
  return 0;
}

int AnalyseMp4::handle_streams() {
  for (int i = 0; i < avformat_context_->nb_streams; i++) {
    // 查找视频流的下标，查找到结束循环
    if (avformat_context_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
      video_stream_index = i;
      break;
    }
  }

  AVPacket *input_packet = av_packet_alloc();
  if (!input_packet) {
    std::cout << "为AvPacket分配内存失败" << std::endl;
    return -1;
  }

  AVPacket sps_pkt;
  AVPacket pps_pkt;

  // 从视频stream->codecpar->extradata中获取sp和pps信息
  h264_extradata_to_annexb(avformat_context_->streams[video_stream_index], &sps_pkt, &pps_pkt, AV_INPUT_BUFFER_PADDING_SIZE);
  analyseData["profile_idc"] = avformat_context_->streams[video_stream_index]->codecpar->profile;
  add_nalu_to_analysedata(7, sps_pkt.size, sps_pkt.data);
  add_nalu_to_analysedata(8, pps_pkt.size, pps_pkt.data);

  while (av_read_frame(avformat_context_, input_packet) >= 0)
  {
    if (input_packet->stream_index == video_stream_index) h264_mp4toannexb(input_packet);
  }

  if (input_packet != NULL) {
    av_packet_free(&input_packet);
    input_packet = NULL;
  }

  avformat_close_input(&avformat_context_);

  avformat_free_context(avformat_context_);
  avformat_context_ = NULL;
  return 0;

}

int AnalyseMp4::h264_mp4toannexb(AVPacket *input_packet) {
  AVPacket *out_packet = NULL;

  uint8_t nal_type;
  int32_t nal_size;
  uint32_t cumul_size = 0;
  const uint8_t *buf;
  const uint8_t *buf_end;
  int buf_size;
  int ret = 0, i;

  buf = input_packet->data;
  buf_size = input_packet->size;
  buf_end = input_packet->data + input_packet->size;

  do
  {
    ret = AVERROR(EINVAL);
    // 每个视频帧的前4个字节表示的是视频帧的大小，如果buf中的数据没有4个字节就不需要继续处理了
    if (buf + 4 > buf_end) {
      return -1;
    }
    // 计算视频帧的大小
    for (nal_size = 0, i = 0; i < 4; i++)
      nal_size = (nal_size << 8) | buf[i];
    
    buf += 4; // 跳过表示视频帧大小的4字节
    nal_type = *buf & 0x1f; // 视频帧的第一个字节里面有nal type
    if (nal_size > buf_end - buf || nal_size < 0) {
      return -1;
    }

    std::stringstream ss;
    ss << std::hex << reinterpret_cast<void*>((uint8_t*)buf);
    std::string address_str = ss.str();
    
    add_nalu_to_analysedata(nal_type, nal_size, buf, true);

    buf += nal_size;
    cumul_size += nal_size + 4;
    
  } while (cumul_size < buf_size);

  return 0;
}

/**
 * AVCC格式
 * bits
 *  8   version (always 0x01)
 *  8   avc profile (sps[0][1])
 *  8   avc compatibility (sps[0][2])
 *  8   avc level (sps[0][3])
 *  6   reserved (all bits on)
 *  2   NALULengthSizeMinusOne
 *  3   reserved (all bits on)
 *  5   number of SPS NALUs (usually 1)
 *  repeated once per SPS
 *  16  SPS size
 *  variable SPS NALU data
 *  8   number of PPS NALUs (usually 1)
 *  repeated once per PPS
 *  16  PPS size
 *  variable PPS NALU data
*/

int AnalyseMp4::h264_extradata_to_annexb(AVStream *avstream, AVPacket *sps_pkt, AVPacket *pps_pkt, int padding) {
  uint16_t uint_size = 0;
  uint64_t total_nalu_size = 0;
  uint8_t unit_nb = 0;
  uint8_t sps_done = 0;
  uint8_t sps_seen = 0;
  uint8_t pps_seen = 0;
  uint8_t *packet = NULL;

  uint8_t *codec_extradata = avstream->codecpar->extradata;
  int codec_extradata_size = avstream->codecpar->extradata_size;

  const uint8_t *extradata = codec_extradata + 4;
  static const uint8_t nalu_header[4] = {0, 0, 0, 1};
  extradata++; // 跳过一个字节，这个字节没用

  unit_nb = *extradata++ & 0x1f; // 取SPS个数，理论上可以有多个

  if (!unit_nb) {
    av_log(NULL, AV_LOG_ERROR, "No SPS、PPS\n");
    return -1;
  } else sps_seen = 1;

  while (unit_nb--)
  {
    // 每一个sps或者pps的数据结构都是[16位 sps/pps长度][sps/pps NALU data]
    int err;
    uint_size = AV_RB16(extradata);
    total_nalu_size += uint_size + 4; // 加上4字节的H264 header，即0001
    if (total_nalu_size > INT_MAX - padding) {
      av_log(NULL, AV_LOG_ERROR, "Too big extradata size, corrupted stream or invalid MP4/AVCC bitstream\n");
      av_free(packet);
      return AVERROR(EINVAL);
    }
    // 此时extradata指向的还是表示长度那两位之前的位置，所以需要+2
    if (extradata + 2 + uint_size > codec_extradata + codec_extradata_size) {
      av_log(NULL, AV_LOG_ERROR, "Packet header is not contained in global extradata, corrupted stream or invalid MP4/AVCC bitstream\n");
      av_free(packet);
      return AVERROR(EINVAL);
    }

    // 分配存放SPS的空间
    if ((err = av_reallocp(&packet, total_nalu_size)) < 0) return err;
    memcpy(packet + total_nalu_size - uint_size - 4, nalu_header, 4);
    memcpy(packet + total_nalu_size - uint_size, extradata + 2, uint_size);
    extradata += 2 + uint_size;
    if (!unit_nb && !sps_done++) {
      // 表示SPS已经读取完成
      sps_pkt->data = packet;
      sps_pkt->size = total_nalu_size;
      // SPS读取完后继续处理的就是PPS的数据
      unit_nb = *extradata++; // 获取SPS的数量
      if (unit_nb) {
        packet = NULL;
        total_nalu_size = 0;
        pps_seen = 1;
      }
    } else if (!unit_nb) {
      // PPS读取完
      pps_pkt->data = packet;
      pps_pkt->size = total_nalu_size;
    }
  }

  if (!sps_seen) av_log(NULL, AV_LOG_WARNING, "Warning: SPS NALU missing or invalid. The resulting stream may not play.\n");
  
  if (!pps_seen) av_log(NULL, AV_LOG_WARNING, "Warning: PPS NALU missing or invalid. The resulting stream may not play.\n");

  return 0;
}

int AnalyseMp4::add_nalu_to_analysedata(uint8_t nal_type, int32_t nal_size, const uint8_t *nal_data, bool add_start_code) {
  json packet;
  json packetData;
  packet["nal_type"] = nal_type;
  if (add_start_code) {
    // 如果nalu没有起始码，先添加起始码
    packetData.push_back("00");
    packetData.push_back("00");
    packetData.push_back("00");
    packetData.push_back("01");
  }
  packet["nal_size"] = add_start_code ? nal_size + 4 : nal_size;
  for (int i = 0; i < nal_size; i++) {
    packetData.push_back(nal_data[i]);
  }
  packet["data"] = packetData;
  analyseData["data"].push_back(packet);
}

int AnalyseMp4::analyse() {
  if (open_media()) return -1;
  if (handle_streams()) return -1;
  return 0;
}

std::string AnalyseMp4::getAnalyseData() {
  return analyseData.dump();
}

AnalyseMp4::AnalyseMp4(std::string input_filename)
{
  input_filename_ = input_filename;
  json data;
  analyseData["data"] = data;
}

AnalyseMp4::~AnalyseMp4()
{
  analyseData.clear();
}
