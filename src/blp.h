#pragma once
#include <QByteArray>
#include <QImage>
#include <QString>

// Decodes the first mip level of a Blizzard BLP texture.
// Supports BLP1 (paletted) and BLP2 (paletted, DXT1/3/5, raw BGRA).
// BLP1 with embedded JPEG content is not supported.
QImage decodeBlp(const QByteArray& data, QString* err);
