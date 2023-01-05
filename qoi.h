#ifndef QOI_FORMAT_CODEC_QOI_H_
#define QOI_FORMAT_CODEC_QOI_H_

#include "utils.h"

constexpr uint8_t QOI_OP_INDEX_TAG = 0x00;
constexpr uint8_t QOI_OP_DIFF_TAG = 0x40;
constexpr uint8_t QOI_OP_LUMA_TAG = 0x80;
constexpr uint8_t QOI_OP_RUN_TAG = 0xc0;
constexpr uint8_t QOI_OP_RGB_TAG = 0xfe;
constexpr uint8_t QOI_OP_RGBA_TAG = 0xff;
constexpr uint8_t QOI_PADDING[8] = {0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u};
constexpr uint8_t QOI_MASK_2 = 0xc0;

/**
 * @brief encode the raw pixel data of an image to qoi format.
 *
 * @param[in] width image width in pixels
 * @param[in] height image height in pixels
 * @param[in] channels number of color channels, 3 = RGB, 4 = RGBA
 * @param[in] colorspace image color space, 0 = sRGB with linear alpha, 1 = all channels linear
 *
 * @return bool true if it is a valid qoi format image, false otherwise
 */
bool QoiEncode(uint32_t width, uint32_t height, uint8_t channels, uint8_t colorspace = 0);

/**
 * @brief decode the qoi format of an image to raw pixel data
 *
 * @param[out] width image width in pixels
 * @param[out] height image height in pixels
 * @param[out] channels number of color channels, 3 = RGB, 4 = RGBA
 * @param[out] colorspace image color space, 0 = sRGB with linear alpha, 1 = all channels linear
 *
 * @return bool true if it is a valid qoi format image, false otherwise
 */
bool QoiDecode(uint32_t &width, uint32_t &height, uint8_t &channels, uint8_t &colorspace);


bool QoiEncode(uint32_t width, uint32_t height, uint8_t channels, uint8_t colorspace) {

    // qoi-header part

    // write magic bytes "qoif"
    QoiWriteChar('q');
    QoiWriteChar('o');
    QoiWriteChar('i');
    QoiWriteChar('f');
    // write image width
    QoiWriteU32(width);
    // write image height
    QoiWriteU32(height);
    // write channel number
    QoiWriteU8(channels);
    // write color space specifier
    QoiWriteU8(colorspace);

    /* qoi-data part */
    int run = 0;
    int px_num = width * height;

    uint8_t history[64][4];
    memset(history, 0, sizeof(history));

    uint8_t r, g, b, a;
    a = 255u;
    uint8_t pre_r, pre_g, pre_b, pre_a;
    pre_r = 0u;
    pre_g = 0u;
    pre_b = 0u;
    pre_a = 255u;

    for (int i = 0; i < px_num; ++i) {
        r = QoiReadU8();
        g = QoiReadU8();
        b = QoiReadU8();
        if (channels == 4) a = QoiReadU8();

        if (r == pre_r && g == pre_g && b == pre_b) {//运行QOI_OP_RUN
            ++run;
            if (run == 62 || i == px_num - 1) {
                QoiWriteU8(QOI_OP_RUN_TAG + run - 1);//run最大或到最后一像素，输出
                run = 0;
            }
        } else {
            if (run > 0) {
                QoiWriteU8(QOI_OP_RUN_TAG + run - 1);//表明run结束，输出
                run = 0;
            }
            int8_t index = (r * 3 + g * 5 + b * 7 + a * 11) % 64;
            if (history[index][0] == r && history[index][1] == g &&
                history[index][2] == b) {
                QoiWriteU8(QOI_OP_INDEX_TAG + index);//若index中可找到，运行QOI_OP_INDEX
            } else {
                history[index][0] = r;
                history[index][1] = g;
                history[index][2] = b;//将信息记录到相应位置中

                int8_t diff_r = r - pre_r;
                int8_t diff_g = g - pre_g;
                int8_t diff_b = b - pre_b;

                if (diff_r >= -2 && diff_r <= 1 && diff_g >= -2 &&
                    diff_g <= 1 && diff_b >= -2 && diff_b <= 1) {//运行QOI_OP_DIFF
                    QoiWriteU8(QOI_OP_DIFF_TAG + ((diff_r + 2) << 4) +
                               ((diff_g + 2) << 2) + (diff_b + 2));
                } else {
                    int8_t diff_rg = diff_r - diff_g;
                    int8_t diff_bg = diff_b - diff_g;
                    if (diff_g >= -32 && diff_g <= 31 && diff_rg >= -8 &&
                        diff_rg <= 7 && diff_bg >= -8 && diff_bg <= 7) {//运行QOI_OP_LUMA
                        QoiWriteU8(QOI_OP_LUMA_TAG + (diff_g + 32));
                        QoiWriteU8(((diff_rg + 8) << 4) + (diff_bg + 8));
                    } else {//运行QOI_OP_RGB
                        QoiWriteU8(QOI_OP_RGB_TAG);
                        QoiWriteU8(r);
                        QoiWriteU8(g);
                        QoiWriteU8(b);
                    }
                }
            }
        }
        pre_r = r;
        pre_g = g;
        pre_b = b;
        pre_a = a;
    }

    // qoi-padding part
    for (int i = 0; i < sizeof(QOI_PADDING) / sizeof(QOI_PADDING[0]); ++i) {
        QoiWriteU8(QOI_PADDING[i]);
    }

    return true;
}

bool QoiDecode(uint32_t &width, uint32_t &height, uint8_t &channels, uint8_t &colorspace) {

    char c1 = QoiReadChar();
    char c2 = QoiReadChar();
    char c3 = QoiReadChar();
    char c4 = QoiReadChar();
    if (c1 != 'q' || c2 != 'o' || c3 != 'i' || c4 != 'f') {
        return false;
    }

    // read image width
    width = QoiReadU32();
    // read image height
    height = QoiReadU32();
    // read channel number
    channels = QoiReadU8();
    // read color space specifier
    colorspace = QoiReadU8();

    int run = 0;
    int px_num = width * height;

    uint8_t history[64][4];
    memset(history, 0, sizeof(history));

    uint8_t r, g, b, a;
    a = 255u;

    for (int i = 0; i < px_num; ++i) {

        if (run == 0) {
            uint8_t read_1 = QoiReadU8();//先读入一个字节
            if ((read_1 >> 6) == 3 && read_1 != QOI_OP_RGB_TAG) {//压缩方式为QOI_OP_RUN
                run = read_1 - QOI_OP_RUN_TAG + 1;
                --run;
            } else {
                if ((read_1 >> 6) == 0) {//压缩方式为QOI_OP_INDEX
                    r = history[read_1 - QOI_OP_INDEX_TAG][0];
                    g = history[read_1 - QOI_OP_INDEX_TAG][1];
                    b = history[read_1 - QOI_OP_INDEX_TAG][2];
                } else {
                    if (read_1 == QOI_OP_RGB_TAG) {//压缩方式为QOI_OP_RGB
                        r = QoiReadU8();
                        g = QoiReadU8();
                        b = QoiReadU8();
                    } else {
                        if ((read_1 >> 6) == 1) {//压缩方式为QOI_OP_DIFF
                            r += (((read_1 >> 4) & 3) - 2);
                            g += (((read_1 >> 2) & 3) - 2);
                            b += ((read_1 & 3) - 2);
                        } else {
                            if ((read_1 >> 6) == 2) {//压缩方式为QOI_OP_LUMA
                                uint8_t read_2 = QoiReadU8();
                                int8_t diff_g = (read_1 & 63) - 32;
                                g += diff_g;
                                r += (diff_g + (read_2 >> 4) - 8);
                                b += (diff_g + (read_2 & 15) - 8);
                            }
                        }
                    }
                    int8_t index = (r * 3 + g * 5 + b * 7 + a * 11) % 64;
                    history[index][0] = r;
                    history[index][1] = g;
                    history[index][2] = b;//将信息记录到相应位置中
                }
            }
        } else --run;//run不为0，重复输出上一轮的信息
        QoiWriteU8(r);
        QoiWriteU8(g);
        QoiWriteU8(b);
        if (channels == 4) QoiWriteU8(a);
    }

    bool valid = true;
    for (int i = 0; i < sizeof(QOI_PADDING) / sizeof(QOI_PADDING[0]); ++i) {
        if (QoiReadU8() != QOI_PADDING[i]) valid = false;
    }

    return valid;
}

#endif // QOI_FORMAT_CODEC_QOI_H_
