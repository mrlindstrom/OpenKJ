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

#ifndef QRCODEGEN_H
#define QRCODEGEN_H

#include <QByteArray>
#include <QString>
#include <vector>

// Minimal QR code encoder (ISO/IEC 18004), byte mode, error correction level M.
// Only what OpenKJ needs: encode a short string (a URL) into a module matrix.
class QrCode
{
public:
    // Returns an invalid QrCode (size() == 0) if the text is empty or too long to encode.
    static QrCode encodeText(const QString &text);

    [[nodiscard]] bool isValid() const { return m_size > 0; }
    // Width/height of the code in modules, not counting the quiet zone.
    [[nodiscard]] int size() const { return m_size; }
    // True when the module at the given position is dark. Out of range positions are light.
    [[nodiscard]] bool module(int x, int y) const
    {
        if (x < 0 || y < 0 || x >= m_size || y >= m_size)
            return false;
        return m_modules[static_cast<size_t>(y) * m_size + x];
    }

private:
    int m_size{0};
    std::vector<bool> m_modules;
    std::vector<bool> m_isFunction;

    void setModule(int x, int y, bool dark) { m_modules[static_cast<size_t>(y) * m_size + x] = dark; }
    void setFunctionModule(int x, int y, bool dark);
    [[nodiscard]] bool isFunction(int x, int y) const { return m_isFunction[static_cast<size_t>(y) * m_size + x]; }

    void drawFunctionPatterns(int version);
    void drawFinderPattern(int cx, int cy);
    void drawAlignmentPattern(int cx, int cy);
    void drawFormatBits(int mask);
    void drawVersionBits(int version);
    void drawCodewords(const QByteArray &codewords);
    void applyMask(int mask);
    [[nodiscard]] long penaltyScore() const;
};

#endif // QRCODEGEN_H
