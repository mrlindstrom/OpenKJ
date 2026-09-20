/*
 * Copyright (c) 2013-2021 Thomas Isaac Lightburn
 *
 *
 * This file is part of OpenKJ.
 *
 * OpenKJ is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "qrcodegen.h"
#include <algorithm>
#include <array>

namespace {

struct QrBlockInfo {
    int eccPerBlock;
    int group1Blocks;
    int group1DataLen;
    int group2Blocks;
    int group2DataLen;
    int totalDataCodewords;
};

// Table data: ECC level M block structure per version (ISO/IEC 18004)
// {eccCodewordsPerBlock, group1Blocks, group1DataLen, group2Blocks, group2DataLen, totalDataCodewords}
static const QrBlockInfo BLOCK_INFO[41] = {
    {0, 0, 0, 0, 0, 0},
    {10, 1, 16, 0, 0, 16}, // version 1
    {16, 1, 28, 0, 0, 28}, // version 2
    {26, 1, 44, 0, 0, 44}, // version 3
    {18, 2, 32, 0, 0, 64}, // version 4
    {24, 2, 43, 0, 0, 86}, // version 5
    {16, 4, 27, 0, 0, 108}, // version 6
    {18, 4, 31, 0, 0, 124}, // version 7
    {22, 2, 38, 2, 39, 154}, // version 8
    {22, 3, 36, 2, 37, 182}, // version 9
    {26, 4, 43, 1, 44, 216}, // version 10
    {30, 1, 50, 4, 51, 254}, // version 11
    {22, 6, 36, 2, 37, 290}, // version 12
    {22, 8, 37, 1, 38, 334}, // version 13
    {24, 4, 40, 5, 41, 365}, // version 14
    {24, 5, 41, 5, 42, 415}, // version 15
    {28, 7, 45, 3, 46, 453}, // version 16
    {28, 10, 46, 1, 47, 507}, // version 17
    {26, 9, 43, 4, 44, 563}, // version 18
    {26, 3, 44, 11, 45, 627}, // version 19
    {26, 3, 41, 13, 42, 669}, // version 20
    {26, 17, 42, 0, 0, 714}, // version 21
    {28, 17, 46, 0, 0, 782}, // version 22
    {28, 4, 47, 14, 48, 860}, // version 23
    {28, 6, 45, 14, 46, 914}, // version 24
    {28, 8, 47, 13, 48, 1000}, // version 25
    {28, 19, 46, 4, 47, 1062}, // version 26
    {28, 22, 45, 3, 46, 1128}, // version 27
    {28, 3, 45, 23, 46, 1193}, // version 28
    {28, 21, 45, 7, 46, 1267}, // version 29
    {28, 19, 47, 10, 48, 1373}, // version 30
    {28, 2, 46, 29, 47, 1455}, // version 31
    {28, 10, 46, 23, 47, 1541}, // version 32
    {28, 14, 46, 21, 47, 1631}, // version 33
    {28, 14, 46, 23, 47, 1725}, // version 34
    {28, 12, 47, 26, 48, 1812}, // version 35
    {28, 6, 47, 34, 48, 1914}, // version 36
    {28, 29, 46, 14, 47, 1992}, // version 37
    {28, 13, 46, 32, 47, 2102}, // version 38
    {28, 40, 47, 7, 48, 2216}, // version 39
    {28, 18, 47, 31, 48, 2334}, // version 40
};

static const int ALIGN_POS[41][8] = {
    {-1, -1, -1, -1, -1, -1, -1, -1},
    {-1, -1, -1, -1, -1, -1, -1, -1},
    {6, 18, -1, -1, -1, -1, -1, -1},
    {6, 22, -1, -1, -1, -1, -1, -1},
    {6, 26, -1, -1, -1, -1, -1, -1},
    {6, 30, -1, -1, -1, -1, -1, -1},
    {6, 34, -1, -1, -1, -1, -1, -1},
    {6, 22, 38, -1, -1, -1, -1, -1},
    {6, 24, 42, -1, -1, -1, -1, -1},
    {6, 26, 46, -1, -1, -1, -1, -1},
    {6, 28, 50, -1, -1, -1, -1, -1},
    {6, 30, 54, -1, -1, -1, -1, -1},
    {6, 32, 58, -1, -1, -1, -1, -1},
    {6, 34, 62, -1, -1, -1, -1, -1},
    {6, 26, 46, 66, -1, -1, -1, -1},
    {6, 26, 48, 70, -1, -1, -1, -1},
    {6, 26, 50, 74, -1, -1, -1, -1},
    {6, 30, 54, 78, -1, -1, -1, -1},
    {6, 30, 56, 82, -1, -1, -1, -1},
    {6, 30, 58, 86, -1, -1, -1, -1},
    {6, 34, 62, 90, -1, -1, -1, -1},
    {6, 28, 50, 72, 94, -1, -1, -1},
    {6, 26, 50, 74, 98, -1, -1, -1},
    {6, 30, 54, 78, 102, -1, -1, -1},
    {6, 28, 54, 80, 106, -1, -1, -1},
    {6, 32, 58, 84, 110, -1, -1, -1},
    {6, 30, 58, 86, 114, -1, -1, -1},
    {6, 34, 62, 90, 118, -1, -1, -1},
    {6, 26, 50, 74, 98, 122, -1, -1},
    {6, 30, 54, 78, 102, 126, -1, -1},
    {6, 26, 52, 78, 104, 130, -1, -1},
    {6, 30, 56, 82, 108, 134, -1, -1},
    {6, 34, 60, 86, 112, 138, -1, -1},
    {6, 30, 58, 86, 114, 142, -1, -1},
    {6, 34, 62, 90, 118, 146, -1, -1},
    {6, 30, 54, 78, 102, 126, 150, -1},
    {6, 24, 50, 76, 102, 128, 154, -1},
    {6, 28, 54, 80, 106, 132, 158, -1},
    {6, 32, 58, 84, 110, 136, 162, -1},
    {6, 26, 54, 82, 110, 138, 166, -1},
    {6, 30, 58, 86, 114, 142, 170, -1},
};

// --- GF(256) arithmetic, primitive polynomial 0x11D ---------------------------------

struct GaloisField {
    std::array<unsigned char, 256> exp{};
    std::array<unsigned char, 256> log{};
    GaloisField()
    {
        int x = 1;
        for (int i = 0; i < 255; i++) {
            exp[i] = static_cast<unsigned char>(x);
            log[x] = static_cast<unsigned char>(i);
            x <<= 1;
            if (x & 0x100)
                x ^= 0x11D;
        }
        exp[255] = exp[0];
    }
    [[nodiscard]] unsigned char mul(unsigned char a, unsigned char b) const
    {
        if (a == 0 || b == 0)
            return 0;
        return exp[(log[a] + log[b]) % 255];
    }
};

const GaloisField &gf()
{
    static const GaloisField instance;
    return instance;
}

// Generator polynomial for the given number of error correction codewords.
std::vector<unsigned char> rsGenerator(int degree)
{
    std::vector<unsigned char> result(degree, 0);
    result[degree - 1] = 1;
    unsigned char root = 1;
    for (int i = 0; i < degree; i++) {
        for (int j = 0; j < degree; j++) {
            result[j] = gf().mul(result[j], root);
            if (j + 1 < degree)
                result[j] ^= result[j + 1];
        }
        root = gf().mul(root, 2);
    }
    return result;
}

// Error correction codewords for one block of data.
QByteArray rsRemainder(const QByteArray &data, int eccLen)
{
    const auto generator = rsGenerator(eccLen);
    std::vector<unsigned char> result(eccLen, 0);
    for (char byte : data) {
        unsigned char factor = static_cast<unsigned char>(byte) ^ result[0];
        result.erase(result.begin());
        result.push_back(0);
        for (int i = 0; i < eccLen; i++)
            result[i] ^= gf().mul(generator[i], factor);
    }
    QByteArray ecc;
    ecc.reserve(eccLen);
    for (int i = 0; i < eccLen; i++)
        ecc.append(static_cast<char>(result[i]));
    return ecc;
}

// Bose-Chaudhuri-Hocquenghem remainder used by the format and version information.
int bchRemainder(int data, int generator, int generatorBits)
{
    int result = data;
    for (int i = generatorBits - 1; i >= 0; i--) {
        if (result & (1 << (i + generatorBits)))
            result ^= generator << i;
    }
    return result;
}

bool getBit(int value, int index)
{
    return ((value >> index) & 1) != 0;
}

// Number of modules a version can hold, minus the function patterns.
int numRawDataModules(int version)
{
    int size = version * 4 + 17;
    int result = size * size;
    result -= 8 * 8 * 3;                      // finder patterns plus separators
    result -= 15 * 2 + 1;                     // format information and dark module
    result -= (size - 16) * 2;                // timing patterns
    if (version >= 2) {
        int numAlign = version / 7 + 2;
        result -= (numAlign - 1) * (numAlign - 1) * 25;
        result -= (numAlign - 2) * 2 * 20;
        if (version >= 7)
            result -= 6 * 3 * 2;              // version information
    }
    return result;
}

} // namespace

void QrCode::setFunctionModule(int x, int y, bool dark)
{
    setModule(x, y, dark);
    m_isFunction[static_cast<size_t>(y) * m_size + x] = true;
}

void QrCode::drawFinderPattern(int cx, int cy)
{
    for (int dy = -4; dy <= 4; dy++) {
        for (int dx = -4; dx <= 4; dx++) {
            int x = cx + dx;
            int y = cy + dy;
            if (x < 0 || y < 0 || x >= m_size || y >= m_size)
                continue;
            int dist = std::max(std::abs(dx), std::abs(dy));
            setFunctionModule(x, y, dist != 2 && dist != 4);
        }
    }
}

void QrCode::drawAlignmentPattern(int cx, int cy)
{
    for (int dy = -2; dy <= 2; dy++) {
        for (int dx = -2; dx <= 2; dx++)
            setFunctionModule(cx + dx, cy + dy, std::max(std::abs(dx), std::abs(dy)) != 1);
    }
}

void QrCode::drawFunctionPatterns(int version)
{
    // Timing patterns
    for (int i = 0; i < m_size; i++) {
        setFunctionModule(6, i, i % 2 == 0);
        setFunctionModule(i, 6, i % 2 == 0);
    }

    drawFinderPattern(3, 3);
    drawFinderPattern(m_size - 4, 3);
    drawFinderPattern(3, m_size - 4);

    // Alignment patterns, skipping the three that would sit on a finder pattern
    std::vector<int> positions;
    for (int i = 0; i < 8 && ALIGN_POS[version][i] >= 0; i++)
        positions.push_back(ALIGN_POS[version][i]);
    const auto count = static_cast<int>(positions.size());
    for (int i = 0; i < count; i++) {
        for (int j = 0; j < count; j++) {
            if ((i == 0 && j == 0) || (i == 0 && j == count - 1) || (i == count - 1 && j == 0))
                continue;
            drawAlignmentPattern(positions[i], positions[j]);
        }
    }

    // Reserve the format and version areas; the real bits are drawn once the mask is known.
    drawFormatBits(0);
    drawVersionBits(version);
}

void QrCode::drawFormatBits(int mask)
{
    // Error correction level M is 0b00 in the format information.
    const int data = (0 << 3) | mask;
    const int bits = ((data << 10) | bchRemainder(data << 10, 0x537, 10)) ^ 0x5412;

    for (int i = 0; i <= 5; i++)
        setFunctionModule(8, i, getBit(bits, i));
    setFunctionModule(8, 7, getBit(bits, 6));
    setFunctionModule(8, 8, getBit(bits, 7));
    setFunctionModule(7, 8, getBit(bits, 8));
    for (int i = 9; i < 15; i++)
        setFunctionModule(14 - i, 8, getBit(bits, i));

    for (int i = 0; i < 8; i++)
        setFunctionModule(m_size - 1 - i, 8, getBit(bits, i));
    for (int i = 8; i < 15; i++)
        setFunctionModule(8, m_size - 15 + i, getBit(bits, i));
    setFunctionModule(8, m_size - 8, true); // always dark
}

void QrCode::drawVersionBits(int version)
{
    if (version < 7)
        return;
    const int bits = (version << 12) | bchRemainder(version << 12, 0x1F25, 12);
    for (int i = 0; i < 18; i++) {
        const bool bit = getBit(bits, i);
        const int a = m_size - 11 + i % 3;
        const int b = i / 3;
        setFunctionModule(a, b, bit);
        setFunctionModule(b, a, bit);
    }
}

void QrCode::drawCodewords(const QByteArray &codewords)
{
    int i = 0; // bit index into codewords
    for (int right = m_size - 1; right >= 1; right -= 2) {
        if (right == 6)
            right = 5; // skip the vertical timing pattern column
        for (int vert = 0; vert < m_size; vert++) {
            for (int j = 0; j < 2; j++) {
                const int x = right - j;
                const bool upward = ((right + 1) & 2) == 0;
                const int y = upward ? m_size - 1 - vert : vert;
                if (!isFunction(x, y) && i < codewords.size() * 8) {
                    setModule(x, y, getBit(static_cast<unsigned char>(codewords[i >> 3]), 7 - (i & 7)));
                    i++;
                }
                // Any remaining modules stay light, as the standard requires.
            }
        }
    }
}

void QrCode::applyMask(int mask)
{
    for (int y = 0; y < m_size; y++) {
        for (int x = 0; x < m_size; x++) {
            if (isFunction(x, y))
                continue;
            bool invert;
            switch (mask) {
                case 0: invert = (x + y) % 2 == 0; break;
                case 1: invert = y % 2 == 0; break;
                case 2: invert = x % 3 == 0; break;
                case 3: invert = (x + y) % 3 == 0; break;
                case 4: invert = (x / 3 + y / 2) % 2 == 0; break;
                case 5: invert = x * y % 2 + x * y % 3 == 0; break;
                case 6: invert = (x * y % 2 + x * y % 3) % 2 == 0; break;
                default: invert = ((x + y) % 2 + x * y % 3) % 2 == 0; break;
            }
            if (invert)
                setModule(x, y, !module(x, y));
        }
    }
}

long QrCode::penaltyScore() const
{
    const int N1 = 3, N2 = 3, N3 = 40, N4 = 10;
    long result = 0;

    auto finderLike = [](const std::vector<bool> &line, int pos) {
        static const bool pattern[7] = {true, false, true, true, true, false, true};
        for (int i = 0; i < 7; i++) {
            if (line[pos + i] != pattern[i])
                return false;
        }
        return true;
    };

    // Rules 1 and 3, applied to every row and column
    for (int pass = 0; pass < 2; pass++) {
        for (int a = 0; a < m_size; a++) {
            std::vector<bool> line(m_size);
            for (int b = 0; b < m_size; b++)
                line[b] = pass == 0 ? module(b, a) : module(a, b);

            int runLength = 1;
            for (int b = 1; b < m_size; b++) {
                if (line[b] == line[b - 1]) {
                    runLength++;
                    if (runLength == 5)
                        result += N1;
                    else if (runLength > 5)
                        result++;
                } else {
                    runLength = 1;
                }
            }

            for (int b = 0; b + 7 <= m_size; b++) {
                if (!finderLike(line, b))
                    continue;
                // The pattern only counts with four light modules on one side
                bool before = true, after = true;
                for (int k = 1; k <= 4; k++) {
                    if (b - k >= 0 && line[b - k])
                        before = false;
                    if (b + 6 + k < m_size && line[b + 6 + k])
                        after = false;
                }
                if (before || after)
                    result += N3;
            }
        }
    }

    // Rule 2: blocks of the same colour
    for (int y = 0; y + 1 < m_size; y++) {
        for (int x = 0; x + 1 < m_size; x++) {
            const bool colour = module(x, y);
            if (colour == module(x + 1, y) && colour == module(x, y + 1) && colour == module(x + 1, y + 1))
                result += N2;
        }
    }

    // Rule 4: balance of dark and light modules
    int dark = 0;
    for (int y = 0; y < m_size; y++) {
        for (int x = 0; x < m_size; x++) {
            if (module(x, y))
                dark++;
        }
    }
    const int total = m_size * m_size;
    const int percent = dark * 100 / total;
    result += static_cast<long>(std::abs(percent - 50) / 5) * N4;
    return result;
}

QrCode QrCode::encodeText(const QString &text)
{
    QrCode code;
    const QByteArray data = text.toUtf8();
    if (data.isEmpty())
        return code;

    // Smallest version that fits the data in byte mode at error correction level M
    int version = 0;
    for (int v = 1; v <= 40; v++) {
        const int countBits = v < 10 ? 8 : 16;
        const int neededBits = 4 + countBits + data.size() * 8;
        if (neededBits <= BLOCK_INFO[v].totalDataCodewords * 8) {
            version = v;
            break;
        }
    }
    if (version == 0)
        return code; // too much data for a QR code

    const QrBlockInfo &info = BLOCK_INFO[version];

    // Build the bit stream: mode indicator, character count, data, terminator, padding
    std::vector<bool> bits;
    bits.reserve(static_cast<size_t>(info.totalDataCodewords) * 8);
    auto appendBits = [&bits](int value, int count) {
        for (int i = count - 1; i >= 0; i--)
            bits.push_back(getBit(value, i));
    };
    appendBits(0x4, 4); // byte mode
    appendBits(data.size(), version < 10 ? 8 : 16);
    for (char byte : data)
        appendBits(static_cast<unsigned char>(byte), 8);

    const size_t capacityBits = static_cast<size_t>(info.totalDataCodewords) * 8;
    for (int i = 0; i < 4 && bits.size() < capacityBits; i++)
        bits.push_back(false);
    while (bits.size() % 8 != 0)
        bits.push_back(false);
    for (unsigned char pad = 0xEC; bits.size() < capacityBits; pad ^= 0xEC ^ 0x11)
        appendBits(pad, 8);

    QByteArray dataCodewords;
    dataCodewords.reserve(info.totalDataCodewords);
    for (size_t i = 0; i < bits.size(); i += 8) {
        int byte = 0;
        for (int j = 0; j < 8; j++)
            byte = (byte << 1) | (bits[i + j] ? 1 : 0);
        dataCodewords.append(static_cast<char>(byte));
    }

    // Split into blocks, add error correction, then interleave
    std::vector<QByteArray> dataBlocks;
    std::vector<QByteArray> eccBlocks;
    int offset = 0;
    for (int group = 0; group < 2; group++) {
        const int blocks = group == 0 ? info.group1Blocks : info.group2Blocks;
        const int length = group == 0 ? info.group1DataLen : info.group2DataLen;
        for (int b = 0; b < blocks; b++) {
            QByteArray block = dataCodewords.mid(offset, length);
            offset += length;
            eccBlocks.push_back(rsRemainder(block, info.eccPerBlock));
            dataBlocks.push_back(block);
        }
    }

    QByteArray codewords;
    const int maxDataLen = std::max(info.group1DataLen, info.group2Blocks > 0 ? info.group2DataLen : 0);
    for (int i = 0; i < maxDataLen; i++) {
        for (const auto &block : dataBlocks) {
            if (i < block.size())
                codewords.append(block[i]);
        }
    }
    for (int i = 0; i < info.eccPerBlock; i++) {
        for (const auto &block : eccBlocks)
            codewords.append(block[i]);
    }

    // Draw the matrix
    code.m_size = version * 4 + 17;
    code.m_modules.assign(static_cast<size_t>(code.m_size) * code.m_size, false);
    code.m_isFunction.assign(static_cast<size_t>(code.m_size) * code.m_size, false);
    code.drawFunctionPatterns(version);
    code.drawCodewords(codewords);

    // Pick the mask with the lowest penalty score
    int bestMask = 0;
    long bestPenalty = -1;
    for (int mask = 0; mask < 8; mask++) {
        code.applyMask(mask);
        code.drawFormatBits(mask);
        const long penalty = code.penaltyScore();
        if (bestPenalty < 0 || penalty < bestPenalty) {
            bestPenalty = penalty;
            bestMask = mask;
        }
        code.applyMask(mask); // undo
    }
    code.applyMask(bestMask);
    code.drawFormatBits(bestMask);

    // Sanity check: the codewords must have filled the matrix
    if (numRawDataModules(version) / 8 != codewords.size()) {
        return {};
    }
    return code;
}
