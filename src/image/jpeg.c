#include "../include/image/jpeg.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MARKER_SOI  0xD8
#define MARKER_APP_ED  0xED

static int read_file(const char *path, uint8_t **buf, size_t *len) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    *len = ftell(f);
    fseek(f, 0, SEEK_SET);
    *buf = malloc(*len);
    if (!*buf) { fclose(f); return -2; }
    if (fread(*buf, 1, *len, f) != *len) {
        free(*buf); fclose(f); return -3;
    }
    fclose(f);
    return 0;
}

static int write_file(const char *path, const uint8_t *buf, size_t len) {
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    if (fwrite(buf, 1, len, f) != len) {
        fclose(f); return -2;
    }
    fclose(f);
    return 0;
}

int embed_message_jpeg_no_lib(const char *input_path,
                              const char *output_path,
                              const uint8_t *message,
                              size_t message_len)
{
    uint8_t *inbuf = NULL;
    size_t inlen = 0;
    int rc = read_file(input_path, &inbuf, &inlen);
    if (rc != 0) return rc;

    if (inlen < 2 || inbuf[0] != 0xFF || inbuf[1] != MARKER_SOI) {
        free(inbuf);
        return -4;
    }

    size_t seg_data_len = 4 + message_len;
    size_t seg_total_len = 2 + 2 + seg_data_len;
    uint8_t *seg = malloc(seg_total_len);
    if (!seg) { free(inbuf); return -5; }

    seg[0] = 0xFF;
    seg[1] = MARKER_APP_ED;
    uint16_t be_len = (uint16_t)(seg_data_len + 2);
    seg[2] = (uint8_t)(be_len >> 8);
    seg[3] = (uint8_t)(be_len & 0xFF);
    uint32_t le_msglen = (uint32_t)message_len;
    seg[4] = (uint8_t)(le_msglen & 0xFF);
    seg[5] = (uint8_t)((le_msglen >> 8) & 0xFF);
    seg[6] = (uint8_t)((le_msglen >> 16) & 0xFF);
    seg[7] = (uint8_t)((le_msglen >> 24) & 0xFF);
    memcpy(seg + 8, message, message_len);

    size_t outlen = 2 + seg_total_len + (inlen - 2);
    uint8_t *outbuf = malloc(outlen);
    if (!outbuf) { free(inbuf); free(seg); return -6; }

    memcpy(outbuf, inbuf, 2);
    memcpy(outbuf + 2, seg, seg_total_len);
    memcpy(outbuf + 2 + seg_total_len, inbuf + 2, inlen - 2);

    rc = write_file(output_path, outbuf, outlen);

    free(inbuf);
    free(seg);
    free(outbuf);
    return rc;
}

int extract_message_jpeg_no_lib(const char *input_path,
                                uint8_t **out_message,
                                size_t *out_len)
{
    uint8_t *buf = NULL;
    size_t buf_len = 0;
    int rc = read_file(input_path, &buf, &buf_len);
    if (rc != 0) return rc;

    size_t pos = 2;
    while (pos + 4 < buf_len) {
        if (buf[pos] == 0xFF && buf[pos+1] == MARKER_APP_ED) {
            uint16_t seg_len = (buf[pos+2] << 8) | buf[pos+3];
            if (pos + 2 + seg_len > buf_len || seg_len < 6) {
                free(buf);
                return -7;
            }
            uint32_t msg_len =
              (uint32_t)buf[pos+4]
            | ((uint32_t)buf[pos+5] << 8)
            | ((uint32_t)buf[pos+6] << 16)
            | ((uint32_t)buf[pos+7] << 24);
            if (msg_len + 4 != seg_len - 2) {
                free(buf);
                return -8;
            }
            *out_message = malloc(msg_len);
            if (!*out_message) { free(buf); return -9; }
            memcpy(*out_message, buf + pos + 8, msg_len);
            *out_len = msg_len;
            free(buf);
            return 0;
        }
        uint16_t skip_len = (buf[pos+2] << 8) | buf[pos+3];
        pos += 2 + skip_len;
    }

    free(buf);
    return -10; 
}
