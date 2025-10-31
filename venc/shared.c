#include "main.h"

void printHelp() {
  printf(
    "\n\t\tOpenIPC FPV Streamer for HiSilicon/Goke (%s)\n"
    "\n"
    "  Usage:\n"
    "    venc [Arguments]\n"
    "\n"
    "  Arguments:\n"
    "    -v [Version]   - Camera version                  (Default: "
    "target specific)\n"
    "\n"
    "      GK7205v200 / IMX307\n"
    "        200_imx307B  - v200, IMX307, 2-lane MIPI | 720p  | any fps\n"
    "        200_imx307F  - v200, IMX307, 2-lane MIPI | 1080p | 30  fps only\n"
    "\n"
    "      GK7205v300 / IMX307\n"
    "        300_imx307B  - v300, IMX307, 4-lane MIPI | 720p  | any fps\n"
    "        300_imx307F  - v300, IMX307, 4-lane MIPI | 1080p | 30  fps only\n"
    "\n"
    "      GK7205v300 / IMX335\n"
    "        300_imx335F4 - v300, IMX335, 4-lane MIPI | 2592x1520  | 25  fps only\n"
    "        300_imx335F5 - v300, IMX335, 4-lane MIPI | 2592x1944  | 25  fps only\n"
    "        300_imx335B  - v300, IMX335, 4-lane MIPI | 1292x972   | 30 / 60  fps only\n"
    "\n"
    "      Star6E / SSC338Q\n"
    "        star6e_imx335  - 4-lane MIPI | 2592x1944 | 30 fps\n"
    "        star6e_os04a  - 4-lane MIPI | 2688x1520 | 30 fps\n"
    "\n"
    "    -h [IP]        - Sink IP address                 (Default: "
    "127.0.0.1)\n"
    "    -p [Port]      - Sink port                       (Default: 5000)\n"
    "    -r [Rate]      - Max video rate in Kbit/sec.     (Default: 8192)\n"
    "    -n [Size]      - Max payload frame size in bytes (Default: 1400)\n"
    "    -m [Mode]      - Streaming mode                  (Default: "
    "compact)\n"
    "       compact       - Compact UDP stream \n"
    "       rtp           - RTP stream\n"
    "\n"
    "    -s [Size]      - Encoded image size              (Default: "
    "version specific)\n"
    "\n"
    "      Standard resolutions\n"
    "        D1           - 720  x 480\n"
    "        960h         - 960  x 576\n"
    "        720p         - 1280 x 720\n"
    "        1.3MP        - 1280 x 1024\n"
    "        1080p        - 1920 x 1080\n"
    "        4MP          - 2592 x 1520\n"
    "\n"
    "      Custom resolution format\n"
    "        WxH          - Custom resolution W x H pixels\n"
    "\n"
    "    -f [FPS]       - Encoder FPS (25,30,50,60)       (Default: 60)\n"
    "    -g [Value]     - GOP denominator                 (Default: 10)\n"
    "    -c [Codec]     - Encoder mode                    (Default: "
    "264avbr)\n"
    "\n"
    "           --- H264 ---\n"
    "       264avbr       - h264 AVBR\n"
    "       264qvbr       - h264 QVBR\n"
    "       264vbr        - h264 VBR\n"
    "       264cbr        - h264 CBR\n"
    "\n"
    "           --- H265 ---\n"
    "       265avbr       - h265 AVBR\n"
    "       265qvbr       - h265 QVBR\n"
    "       265vbr        - h265 VBR\n"
    "       265cbr        - h265 CBR\n"
    "\n"
    "    -d [Format]    - Data format                       (Default: "
    "stream)\n"
    "      stream         - Produce NALUs in stream mode\n"
    "      frame          - Produce NALUs in packet mode\n"
    "\n"
    "    --no-slices          - Disable slices\n"
    "    --slice-size [size]  - Slices size in lines      (Default: 4)\n"
    "\n"
    "    --low-delay    - Enable low delay mode\n"
    "    --mirror       - Mirror image\n"
    "    --flip         - Flip image\n"
    "    --exp          - Limit exposure\n"
    "\n"
    "    --roi          - Enable ROI\n"
    "    --roi-qp [QP]  - ROI quality points              (Default: 20)\n"
    "\n", __DATE__
  );
}

#ifdef PLATFORM_STAR6E
void sendPacket(uint8_t* pack_data, uint32_t pack_size, int socket_handle,
  struct sockaddr* dst_address, uint32_t max_size) {
  struct RTPHeader* header = (struct RTPHeader*)pack_data;
  if (pack_size <= max_size) {
    sendto(socket_handle, pack_data, pack_size, 0, dst_address,
      sizeof(struct sockaddr_in));
    return;
  }

  uint32_t payload_offset = sizeof(struct RTPHeader);
  uint32_t payload_size = pack_size - payload_offset;
  uint8_t* payload = pack_data + payload_offset;
  uint8_t marker = header->payload_type & 0x80;
  uint16_t sequence = ntohs(header->sequence);
  uint32_t timestamp = ntohl(header->timestamp);
  uint32_t ssrc_id = ntohl(header->ssrc_id);

  uint32_t offset = 0;
  while (offset < payload_size) {
    uint32_t fragment_size = MIN(max_size - sizeof(struct RTPHeader),
      payload_size - offset);
    struct RTPHeader fragment_header = {
      .version = 0x80,
      .payload_type = (header->payload_type & 0x7F) | ((offset + fragment_size >= payload_size) ? marker : 0),
      .sequence = htons(sequence++),
      .timestamp = htonl(timestamp),
      .ssrc_id = htonl(ssrc_id),
    };

    struct iovec vec[2] = {
      {.iov_base = &fragment_header, .iov_len = sizeof(fragment_header)},
      {.iov_base = payload + offset, .iov_len = fragment_size},
    };

    struct msghdr msg = {
      .msg_name = dst_address,
      .msg_namelen = sizeof(struct sockaddr_in),
      .msg_iov = vec,
      .msg_iovlen = 2,
    };

    sendmsg(socket_handle, &msg, 0);
    offset += fragment_size;
  }
}
#endif
