// Core regression test: archive wrapper, model, BLP decoder.
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QTemporaryDir>
#include <cstdio>
#include "../src/blp.h"
#include "../src/mpqarchive.h"
#include "../src/mpqmodel.h"

static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); ++fails; } } while (0)

static void write(const QString& p, const QByteArray& d) { QDir().mkpath(QFileInfo(p).absolutePath()); QFile f(p); f.open(QIODevice::WriteOnly); f.write(d); }
static QByteArray read(const QString& p) { QFile f(p); f.open(QIODevice::ReadOnly); return f.readAll(); }

int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);
    QTemporaryDir td; QString t = td.path(), err;

    QByteArray big; for (int i = 0; i < 200000; ++i) big += QByteArray("line ") + QByteArray::number(i) + "\n";
    write(t + "/src/a.txt", "hello world");
    write(t + "/src/sub/b.lua", big);
    write(t + "/src/sub/deep/c.bin", QByteArray(1000, '\0') + "xyz");

    for (int ver = 1; ver <= 4; ++ver) {
        MpqArchive a;
        QString path = t + QString("/v%1.mpq").arg(ver);
        CHECK(a.create(path, ver, 64, &err));
        CHECK(a.addFile(t + "/src/a.txt", "Interface\\a.txt", 0x02, &err));
        CHECK(a.addFile(t + "/src/sub/b.lua", "Interface\\sub\\b.lua", ver >= 3 ? 0x12 : 0x10, &err));
        CHECK(a.addFile(t + "/src/sub/deep/c.bin", "Data\\c.bin", 0, &err));
        CHECK(a.flush(&err));
        CHECK(a.info().formatVersion == ver);
        a.close();

        MpqArchive b;
        CHECK(b.open(path, false, &err));
        QVector<MpqEntry> es; CHECK(b.loadEntries(es, &err));
        QStringList names; for (auto& e : es) names << e.path;
        names.sort();
        CHECK(names.contains("Interface\\a.txt") && names.contains("Interface\\sub\\b.lua") && names.contains("Data\\c.bin"));
        CHECK(names.contains("(listfile)"));
        QByteArray out; bool tr = true;
        CHECK(b.readFile("Interface\\sub\\b.lua", out, 1 << 30, &tr, &err)); CHECK(out == big && !tr);
        CHECK(b.readFile("Interface\\sub\\b.lua", out, 100, &tr, &err)); CHECK(out.size() == 100 && tr);

        // edit: rename, delete, replace
        CHECK(b.renameFile("Interface\\a.txt", "Interface\\renamed\\a2.txt", &err));
        CHECK(b.removeFile("Data\\c.bin", &err));
        write(t + "/src/a.txt", "REPLACED");
        CHECK(b.addFile(t + "/src/a.txt", "Interface\\renamed\\a2.txt", 0x02, &err));
        CHECK(b.flush(&err));
        b.close();

        MpqArchive c;
        CHECK(c.open(path, true, &err));
        CHECK(c.loadEntries(es, &err));
        names.clear(); for (auto& e : es) names << e.path;
        CHECK(names.contains("Interface\\renamed\\a2.txt"));
        CHECK(!names.contains("Interface\\a.txt") && !names.contains("Data\\c.bin"));
        CHECK(c.readFile("Interface\\renamed\\a2.txt", out, 1000, &tr, &err)); CHECK(out == "REPLACED");
        CHECK(c.extractFile("Interface\\sub\\b.lua", t + QString("/out%1/x/b.lua").arg(ver), &err));
        CHECK(read(t + QString("/out%1/x/b.lua").arg(ver)) == big);
        CHECK(!c.addFile(t + "/src/a.txt", "q", 0, &err));   // read-only must refuse
        std::printf("v%d ok (%d entries)\n", ver, int(es.size()));

        if (ver == 1) {
            MpqModel m; m.setEntries(es);
            CHECK(m.indexForPath("interface\\SUB\\B.LUA").isValid());
            CHECK(m.filesUnder(m.indexForPath("Interface")).size() == 2);
            CHECK(m.filesUnder(QModelIndex()).size() == es.size());
        }
        c.close();
    }

    // compact + max file count growth
    {
        MpqArchive a; CHECK(a.open(t + "/v2.mpq", false, &err));
        CHECK(a.setMaxFileCount(1000, &err)); CHECK(a.info().maxFileCount >= 1000);
        CHECK(a.compact(&err)); a.close();
        CHECK(a.open(t + "/v2.mpq", true, &err));
        QByteArray out; bool tr;
        CHECK(a.readFile("Interface\\sub\\b.lua", out, 1 << 30, &tr, &err)); CHECK(out == big);
    }

    // full archive reports a useful error
    {
        MpqArchive a; CHECK(a.create(t + "/tiny.mpq", 1, 16, &err));
        int ok = 0; QString last;
        for (int i = 0; i < 40; ++i) if (a.addFile(t + "/src/a.txt", QString("f%1.txt").arg(i), 0, &err)) ++ok; else last = err;
        std::printf("tiny: added %d, last error: %s\n", ok, qPrintable(last));
        CHECK(ok < 40 && !last.isEmpty());
    }

    // BLP: build a 4x4 BLP2 DXT1 (solid red) and a paletted BLP2 with 8-bit alpha
    {
        QByteArray h(1172, '\0');
        auto p32 = [&](int o, quint32 v) { for (int i = 0; i < 4; ++i) h[o + i] = char(v >> (8 * i)); };
        h.replace(0, 4, "BLP2"); p32(4, 1); h[8] = 2; h[9] = 0; h[10] = 0; h[11] = 0; p32(12, 4); p32(16, 4);
        p32(20, 1172); p32(84, 8);
        QByteArray blk(8, '\0'); blk[0] = char(0x00); blk[1] = char(0xF8); /*c0=0xF800 red*/ blk[2] = char(0x00); blk[3] = char(0xF8);
        QString e; QImage im = decodeBlp(h + blk, &e);
        CHECK(!im.isNull() && im.width() == 4 && qRed(im.pixel(2, 2)) == 255 && qGreen(im.pixel(2, 2)) == 0);

        QByteArray h2(1172, '\0'); h2.replace(0, 4, "BLP2");
        auto q32 = [&](int o, quint32 v) { for (int i = 0; i < 4; ++i) h2[o + i] = char(v >> (8 * i)); };
        q32(4, 1); h2[8] = 1; h2[9] = 8; q32(12, 2); q32(16, 2); q32(20, 1172); q32(84, 8);
        q32(148 + 4 * 5, 0x0000FF00); // palette[5] = BGRA -> green
        QByteArray px(4, char(5)); QByteArray al(4, char(128));
        im = decodeBlp(h2 + px + al, &e);
        CHECK(!im.isNull() && qGreen(im.pixel(1, 1)) == 255 && qAlpha(im.pixel(1, 1)) == 128);

        CHECK(decodeBlp(QByteArray("garbage garbage garbage garbage"), &e).isNull());
        CHECK(decodeBlp(h + QByteArray(2, 0), &e).isNull());   // truncated
    }
    std::printf(fails ? "\n%d FAILURES\n" : "\nALL PASSED\n", fails);
    return fails ? 1 : 0;
}
