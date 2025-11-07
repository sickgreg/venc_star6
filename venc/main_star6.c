#include "star6.h"

#include <arpa/inet.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <time.h>
#include <unistd.h>

#define DEFAULT_BITRATE 8192
#define DEFAULT_GOP 60
#define DEFAULT_HOST "127.0.0.1"
#define DEFAULT_PORT 5000
#define DEFAULT_SENSOR "imx335"
#define DEFAULT_FPS 60
#define DEFAULT_MAX_PKT 1400

static volatile sig_atomic_t running = 1;

typedef struct {
    uint8_t version;
    uint8_t payload_type;
    uint16_t sequence;
    uint32_t timestamp;
    uint32_t ssrc_id;
} RTPHeader;

static uint16_t rtp_sequence = 0;
static uint32_t ssrc_seed = 0xdeadbeef;

static void handle_signal(int sig) {
    (void)sig;
    running = 0;
}

static void print_usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s [options]\n"
        "\n"
        "Options:\n"
        "  -S, --sensor <name>      Sensor name (imx335, imx415). Default: %s\n"
        "  -F, --fps <value>        Frame rate (30, 60, 90, 120). Default: %d\n"
        "  -B, --bitrate <kbps>     Target bitrate in Kbit/s. Default: %d\n"
        "  -G, --gop <frames>       GOP size. Default: %d\n"
        "  -H, --host <addr>        Destination IPv4 address. Default: %s\n"
        "  -P, --port <port>        Destination UDP port. Default: %d\n"
        "  -I, --iq <path>          Path to ISP IQ binary. Optional.\n"
        "  -C, --codec <h264|h265>  Encoder codec. Default: h264.\n"
        "      --mirror             Enable horizontal mirror.\n"
        "      --flip               Enable vertical flip.\n"
        "      --width <pixels>     Override output width.\n"
        "      --height <pixels>    Override output height.\n"
        "      --mtu <bytes>        Maximum RTP payload size. Default: %d\n"
        "  -h, --help               Show this help.\n",
        prog, DEFAULT_SENSOR, DEFAULT_FPS, DEFAULT_BITRATE,
        DEFAULT_GOP, DEFAULT_HOST, DEFAULT_PORT, DEFAULT_MAX_PKT);
}

static void transmit_packet(int sock, const uint8_t *data, uint32_t size,
    const struct sockaddr_in *dst) {
    RTPHeader header;
    header.version = 0x80;
    header.payload_type = 96;
    header.sequence = htons(rtp_sequence++);
    header.timestamp = 0;
    header.ssrc_id = htonl(ssrc_seed);

    struct iovec iov[2];
    iov[0].iov_base = &header;
    iov[0].iov_len = sizeof(header);
    iov[1].iov_base = (void *)data;
    iov[1].iov_len = size;

    struct msghdr msg;
    memset(&msg, 0, sizeof(msg));
    msg.msg_iov = iov;
    msg.msg_iovlen = 2;
    msg.msg_name = (void *)dst;
    msg.msg_namelen = sizeof(*dst);

    sendmsg(sock, &msg, 0);
}

static uint8_t tx_buffer[4096];

static void send_packet(int sock, hal_vidcodec codec, uint8_t *packet,
    uint32_t size, const struct sockaddr_in *dst, uint32_t max_payload) {
    const uint32_t prefix = 4;
    if (size <= prefix)
        return;

    uint8_t *payload = packet + prefix;
    uint32_t payload_size = size - prefix;

    if (payload_size <= max_payload) {
        transmit_packet(sock, payload, payload_size, dst);
        return;
    }

    if (codec == HAL_VIDCODEC_H265) {
        if (payload_size <= 2 || max_payload <= 3)
            return;

        uint8_t nal_header0 = payload[0];
        uint8_t nal_header1 = payload[1];
        uint8_t nal_type = (nal_header0 >> 1) & 0x3F;

        const uint8_t *nal_payload = payload + 2;
        uint32_t nal_payload_size = payload_size - 2;
        bool start = true;

        while (nal_payload_size && running) {
            uint32_t chunk = nal_payload_size;
            if (chunk > max_payload - 3)
                chunk = max_payload - 3;

            tx_buffer[0] = (uint8_t)((nal_header0 & 0x81) | (49 << 1));
            tx_buffer[1] = nal_header1;
            tx_buffer[2] = nal_type;
            if (start)
                tx_buffer[2] |= 0x80;
            if (chunk == nal_payload_size)
                tx_buffer[2] |= 0x40;

            memcpy(tx_buffer + 3, nal_payload, chunk);
            transmit_packet(sock, tx_buffer, chunk + 3, dst);

            nal_payload += chunk;
            nal_payload_size -= chunk;
            start = false;
        }
        return;
    }

    if (payload_size <= 1 || max_payload <= 2)
        return;

    uint8_t nal_header = payload[0];
    uint8_t nal_type = nal_header & 0x1F;
    uint8_t fu_indicator = (nal_header & 0xE0) | 28;
    uint8_t fu_header = nal_type;

    const uint8_t *nal_payload = payload + 1;
    uint32_t nal_payload_size = payload_size - 1;
    bool start = true;

    while (nal_payload_size && running) {
        uint32_t chunk = nal_payload_size;
        if (chunk > max_payload - 2)
            chunk = max_payload - 2;

        tx_buffer[0] = fu_indicator;
        tx_buffer[1] = fu_header;
        if (start)
            tx_buffer[1] |= 0x80;
        if (chunk == nal_payload_size)
            tx_buffer[1] |= 0x40;

        memcpy(tx_buffer + 2, nal_payload, chunk);
        transmit_packet(sock, tx_buffer, chunk + 2, dst);

        nal_payload += chunk;
        nal_payload_size -= chunk;
        start = false;
    }
}

int main(int argc, char **argv) {
    const char *sensor = DEFAULT_SENSOR;
    unsigned int fps = DEFAULT_FPS;
    unsigned int bitrate = DEFAULT_BITRATE;
    unsigned int gop = DEFAULT_GOP;
    const char *host = DEFAULT_HOST;
    uint16_t port = DEFAULT_PORT;
    const char *iq_path = NULL;
    hal_vidcodec codec = HAL_VIDCODEC_H264;
    bool mirror = false;
    bool flip = false;
    unsigned int width = 0;
    unsigned int height = 0;
    uint32_t max_payload = DEFAULT_MAX_PKT;

    static struct option long_opts[] = {
        {"sensor", required_argument, NULL, 'S'},
        {"fps", required_argument, NULL, 'F'},
        {"bitrate", required_argument, NULL, 'B'},
        {"gop", required_argument, NULL, 'G'},
        {"host", required_argument, NULL, 'H'},
        {"port", required_argument, NULL, 'P'},
        {"iq", required_argument, NULL, 'I'},
        {"codec", required_argument, NULL, 'C'},
        {"mirror", no_argument, NULL, 1},
        {"flip", no_argument, NULL, 2},
        {"width", required_argument, NULL, 3},
        {"height", required_argument, NULL, 4},
        {"mtu", required_argument, NULL, 5},
        {"help", no_argument, NULL, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "hS:F:B:G:H:P:I:C:", long_opts, NULL)) != -1) {
        switch (opt) {
            case 'S':
                sensor = optarg;
                break;
            case 'F':
                fps = (unsigned int)strtoul(optarg, NULL, 10);
                break;
            case 'B':
                bitrate = (unsigned int)strtoul(optarg, NULL, 10);
                break;
            case 'G':
                gop = (unsigned int)strtoul(optarg, NULL, 10);
                break;
            case 'H':
                host = optarg;
                break;
            case 'P':
                port = (uint16_t)strtoul(optarg, NULL, 10);
                break;
            case 'I':
                iq_path = optarg;
                break;
            case 'C':
                if (!strcasecmp(optarg, "h265"))
                    codec = HAL_VIDCODEC_H265;
                else
                    codec = HAL_VIDCODEC_H264;
                break;
            case 1:
                mirror = true;
                break;
            case 2:
                flip = true;
                break;
            case 3:
                width = (unsigned int)strtoul(optarg, NULL, 10);
                break;
            case 4:
                height = (unsigned int)strtoul(optarg, NULL, 10);
                break;
            case 5:
                max_payload = (uint32_t)strtoul(optarg, NULL, 10);
                break;
            case 'h':
            default:
                print_usage(argv[0]);
                return EXIT_FAILURE;
        }
    }

    if (!(fps == 30 || fps == 60 || fps == 90 || fps == 120)) {
        fprintf(stderr, "Unsupported FPS value %u.\n", fps);
        return EXIT_FAILURE;
    }

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    star6_context context;
    if (star6_context_init(&context)) {
        fprintf(stderr, "Failed to initialize Sigmastar runtime.\n");
        return EXIT_FAILURE;
    }

    if (star6_pipeline_start(&context, sensor, fps, iq_path, codec,
            bitrate, gop, mirror, flip, width, height)) {
        fprintf(stderr, "Failed to start capture pipeline.\n");
        star6_context_deinit(&context);
        return EXIT_FAILURE;
    }

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        star6_context_deinit(&context);
        return EXIT_FAILURE;
    }

    struct sockaddr_in dst;
    memset(&dst, 0, sizeof(dst));
    dst.sin_family = AF_INET;
    dst.sin_port = htons(port);
    if (inet_aton(host, &dst.sin_addr) == 0) {
        fprintf(stderr, "Invalid host address %s\n", host);
        close(sock);
        star6_context_deinit(&context);
        return EXIT_FAILURE;
    }

    fprintf(stderr, "Streaming %ux%u @ %u fps to %s:%u (%s)\n",
        context.target_width, context.target_height, context.framerate,
        host, port, codec == HAL_VIDCODEC_H265 ? "H.265" : "H.264");

    uint32_t frame_count = 0;
    uint64_t byte_count = 0;
    struct timespec last_log;
    clock_gettime(CLOCK_MONOTONIC, &last_log);

    while (running) {
        i6_venc_stat stat;
        if (star6_query_stream(&context, &stat) || !stat.curPacks) {
            usleep(5000);
            continue;
        }

        i6_venc_strm stream;
        memset(&stream, 0, sizeof(stream));
        stream.packet = calloc(stat.curPacks, sizeof(i6_venc_pack));
        if (!stream.packet)
            break;
        stream.count = stat.curPacks;

        if (star6_acquire_stream(&context, &stream, 40)) {
            free(stream.packet);
            stream.packet = NULL;
            continue;
        }

        for (unsigned int i = 0; i < stream.count && running; ++i) {
            i6_venc_pack *pack = &stream.packet[i];
            if (!pack->data || pack->length <= pack->offset)
                continue;
            uint8_t *payload = pack->data + pack->offset;
            uint32_t payload_len = pack->length - pack->offset;
            send_packet(sock, codec, payload, payload_len, &dst, max_payload);
            byte_count += payload_len;
        }

        frame_count += stat.curPacks;

        star6_release_stream(&context, &stream);
        free(stream.packet);
        stream.packet = NULL;

        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        double elapsed = (now.tv_sec - last_log.tv_sec) +
            (now.tv_nsec - last_log.tv_nsec) / 1e9;
        if (elapsed >= 1.0) {
            double mbit = (double)byte_count * 8.0 / 1e6;
            fprintf(stderr, "Rate: %.2f Mbps, frames: %u\n", mbit / elapsed, frame_count);
            byte_count = 0;
            frame_count = 0;
            last_log = now;
        }
    }

    close(sock);
    star6_context_deinit(&context);
    fprintf(stderr, "Shutdown complete.\n");
    return EXIT_SUCCESS;
}

