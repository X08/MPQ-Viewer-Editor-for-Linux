#include "blp.h"

#include <cstring>

namespace {

struct Reader {
    const uchar* d;
    qint64 n;
    bool u32(qint64 off, quint32& v) const
    {
        if (off < 0 || off + 4 > n) return false;
        v = quint32(d[off]) | quint32(d[off + 1]) << 8 | quint32(d[off + 2]) << 16 | quint32(d[off + 3]) << 24;
        return true;
    }
    bool u8(qint64 off, quint8& v) const
    {
        if (off < 0 || off >= n) return false;
        v = d[off];
        return true;
    }
};

inline QRgb rgb565(quint16 c, int a = 255)
{
    int r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
    return qRgba((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2), a);
}

// Decode a BC1 colour block (8 bytes) into 16 pixels; if allowAlpha, c0<=c1 gives 1-bit alpha.
void bc1Block(const uchar* p, QRgb out[16], bool allowAlpha)
{
    quint16 c0 = quint16(p[0] | p[1] << 8), c1 = quint16(p[2] | p[3] << 8);
    QRgb col[4];
    col[0] = rgb565(c0);
    col[1] = rgb565(c1);
    if (c0 > c1 || !allowAlpha) {
        col[2] = qRgb((qRed(col[0]) * 2 + qRed(col[1])) / 3, (qGreen(col[0]) * 2 + qGreen(col[1])) / 3,
                      (qBlue(col[0]) * 2 + qBlue(col[1])) / 3);
        col[3] = qRgb((qRed(col[0]) + qRed(col[1]) * 2) / 3, (qGreen(col[0]) + qGreen(col[1]) * 2) / 3,
                      (qBlue(col[0]) + qBlue(col[1]) * 2) / 3);
    } else {
        col[2] = qRgb((qRed(col[0]) + qRed(col[1])) / 2, (qGreen(col[0]) + qGreen(col[1])) / 2,
                      (qBlue(col[0]) + qBlue(col[1])) / 2);
        col[3] = qRgba(0, 0, 0, 0);
    }
    quint32 bits = quint32(p[4]) | quint32(p[5]) << 8 | quint32(p[6]) << 16 | quint32(p[7]) << 24;
    for (int i = 0; i < 16; ++i)
        out[i] = col[(bits >> (2 * i)) & 3];
}

QImage decodeDxt(const uchar* src, qint64 avail, int w, int h, int kind /*1,3,5*/, QString* err)
{
    const int bw = (w + 3) / 4, bh = (h + 3) / 4;
    const int bs = (kind == 1) ? 8 : 16;
    if (qint64(bw) * bh * bs > avail) {
        if (err) *err = QStringLiteral("BLP texture data is truncated");
        return {};
    }
    QImage img(w, h, QImage::Format_ARGB32);
    for (int by = 0; by < bh; ++by) {
        for (int bx = 0; bx < bw; ++bx) {
            const uchar* p = src + (qint64(by) * bw + bx) * bs;
            QRgb px[16];
            int alpha[16];
            for (int i = 0; i < 16; ++i) alpha[i] = 255;
            if (kind == 1) {
                bc1Block(p, px, true);
            } else {
                bc1Block(p + 8, px, false);
                if (kind == 3) {
                    for (int i = 0; i < 16; ++i) {
                        int v = (p[i / 2] >> ((i & 1) * 4)) & 15;
                        alpha[i] = v * 17;
                    }
                } else {
                    int a0 = p[0], a1 = p[1];
                    int tab[8];
                    tab[0] = a0;
                    tab[1] = a1;
                    if (a0 > a1) {
                        for (int i = 1; i < 7; ++i) tab[i + 1] = ((7 - i) * a0 + i * a1) / 7;
                    } else {
                        for (int i = 1; i < 5; ++i) tab[i + 1] = ((5 - i) * a0 + i * a1) / 5;
                        tab[6] = 0;
                        tab[7] = 255;
                    }
                    quint64 bits = 0;
                    for (int i = 0; i < 6; ++i) bits |= quint64(p[2 + i]) << (8 * i);
                    for (int i = 0; i < 16; ++i) alpha[i] = tab[(bits >> (3 * i)) & 7];
                }
            }
            for (int y = 0; y < 4; ++y) {
                const int yy = by * 4 + y;
                if (yy >= h) break;
                QRgb* line = reinterpret_cast<QRgb*>(img.scanLine(yy));
                for (int x = 0; x < 4; ++x) {
                    const int xx = bx * 4 + x;
                    if (xx >= w) break;
                    QRgb c = px[y * 4 + x];
                    line[xx] = (kind == 1) ? c : qRgba(qRed(c), qGreen(c), qBlue(c), alpha[y * 4 + x]);
                }
            }
        }
    }
    return img;
}

QImage decodePaletted(const uchar* data, qint64 total, qint64 off, int w, int h, const QRgb* pal, int alphaDepth,
                      QString* err)
{
    const qint64 npx = qint64(w) * h;
    qint64 alphaBytes = 0;
    if (alphaDepth == 8) alphaBytes = npx;
    else if (alphaDepth == 4) alphaBytes = (npx + 1) / 2;
    else if (alphaDepth == 1) alphaBytes = (npx + 7) / 8;
    if (off < 0 || off + npx + alphaBytes > total) {
        if (err) *err = QStringLiteral("BLP texture data is truncated");
        return {};
    }
    const uchar* idx = data + off;
    const uchar* al = idx + npx;
    QImage img(w, h, QImage::Format_ARGB32);
    for (int y = 0; y < h; ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(img.scanLine(y));
        for (int x = 0; x < w; ++x) {
            const qint64 i = qint64(y) * w + x;
            int a = 255;
            if (alphaDepth == 8) a = al[i];
            else if (alphaDepth == 4) a = ((al[i / 2] >> ((i & 1) * 4)) & 15) * 17;
            else if (alphaDepth == 1) a = ((al[i / 8] >> (i & 7)) & 1) ? 255 : 0;
            QRgb c = pal[idx[i]];
            line[x] = qRgba(qRed(c), qGreen(c), qBlue(c), a);
        }
    }
    return img;
}

} // namespace

QImage decodeBlp(const QByteArray& bytes, QString* err)
{
    Reader r{reinterpret_cast<const uchar*>(bytes.constData()), bytes.size()};
    if (bytes.size() < 28) {
        if (err) *err = QStringLiteral("Not a BLP file (too small)");
        return {};
    }
    const bool blp1 = std::memcmp(bytes.constData(), "BLP1", 4) == 0;
    const bool blp2 = std::memcmp(bytes.constData(), "BLP2", 4) == 0;
    if (!blp1 && !blp2) {
        if (err) *err = QStringLiteral("Not a BLP1/BLP2 file");
        return {};
    }

    quint32 w = 0, h = 0, off0 = 0, size0 = 0;
    QRgb pal[256];

    auto sane = [&](quint32 W, quint32 H) { return W > 0 && H > 0 && W <= 16384 && H <= 16384; };

    if (blp1) {
        quint32 comp, alphaBits, extra, hasMips;
        r.u32(4, comp); r.u32(8, alphaBits); r.u32(12, w); r.u32(16, h); r.u32(20, extra); r.u32(24, hasMips);
        if (!r.u32(28, off0) || !r.u32(28 + 64, size0) || !sane(w, h)) {
            if (err) *err = QStringLiteral("Invalid BLP1 header");
            return {};
        }
        if (comp == 0) {
            if (err) *err = QStringLiteral("BLP1 with embedded JPEG is not supported in the preview");
            return {};
        }
        for (int i = 0; i < 256; ++i) {
            quint32 c;
            if (!r.u32(156 + i * 4, c)) {
                if (err) *err = QStringLiteral("Invalid BLP1 palette");
                return {};
            }
            pal[i] = qRgb((c >> 16) & 255, (c >> 8) & 255, c & 255);
        }
        return decodePaletted(r.d, r.n, off0, int(w), int(h), pal, int(alphaBits), err);
    }

    // BLP2
    quint32 type;
    quint8 enc, alphaDepth, alphaEnc, mips;
    if (!r.u32(4, type) || !r.u8(8, enc) || !r.u8(9, alphaDepth) || !r.u8(10, alphaEnc) || !r.u8(11, mips) ||
        !r.u32(12, w) || !r.u32(16, h) || !r.u32(20, off0) || !r.u32(20 + 64, size0) || !sane(w, h)) {
        if (err) *err = QStringLiteral("Invalid BLP2 header");
        return {};
    }
    if (type != 1) {
        if (err) *err = QStringLiteral("Unsupported BLP2 content type");
        return {};
    }
    if (off0 > quint32(r.n)) {
        if (err) *err = QStringLiteral("BLP2 mip offset out of range");
        return {};
    }
    const uchar* mip = r.d + off0;
    const qint64 avail = r.n - off0;
    if (enc == 1) {
        for (int i = 0; i < 256; ++i) {
            quint32 c;
            r.u32(148 + i * 4, c);
            pal[i] = qRgb((c >> 16) & 255, (c >> 8) & 255, c & 255);
        }
        return decodePaletted(r.d, r.n, off0, int(w), int(h), pal, alphaDepth, err);
    }
    if (enc == 2) {
        int kind = (alphaEnc == 0) ? 1 : (alphaEnc == 1) ? 3 : (alphaEnc == 7) ? 5 : 0;
        if (!kind) {
            if (err) *err = QStringLiteral("Unsupported BLP2 DXT variant");
            return {};
        }
        return decodeDxt(mip, avail, int(w), int(h), kind, err);
    }
    if (enc == 3) {
        const qint64 need = qint64(w) * h * 4;
        if (need > avail) {
            if (err) *err = QStringLiteral("BLP2 texture data is truncated");
            return {};
        }
        QImage img(int(w), int(h), QImage::Format_ARGB32);
        for (quint32 y = 0; y < h; ++y) {
            QRgb* line = reinterpret_cast<QRgb*>(img.scanLine(int(y)));
            for (quint32 x = 0; x < w; ++x) {
                const uchar* p = mip + (qint64(y) * w + x) * 4;  // stored B,G,R,A
                line[x] = qRgba(p[2], p[1], p[0], p[3]);
            }
        }
        return img;
    }
    if (err) *err = QStringLiteral("Unsupported BLP2 encoding %1").arg(enc);
    return {};
}
