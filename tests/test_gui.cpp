// GUI smoke test (offscreen): builds an archive, opens it in MainWindow, screenshots previews.
#include <QApplication>
#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QPainter>
#include <QTreeView>
#include <QLineEdit>
#include <QMimeData>
#include <QDropEvent>
#include <QUrl>
#include <cstdio>
#include "../src/mainwindow.h"
#include "../src/mpqarchive.h"

static void wr(const QString& p, const QByteArray& d) { QFile f(p); f.open(QIODevice::WriteOnly); f.write(d); }

static QByteArray makeBlpDxt5(int w, int h)
{
    QByteArray hd(1172, '\0');
    auto p32 = [&](int o, quint32 v) { for (int i = 0; i < 4; ++i) hd[o + i] = char(v >> (8 * i)); };
    hd.replace(0, 4, "BLP2"); p32(4, 1); hd[8] = 2; hd[9] = 8; hd[10] = 7; p32(12, w); p32(16, h); p32(20, 1172);
    QByteArray data;
    for (int by = 0; by < h / 4; ++by) for (int bx = 0; bx < w / 4; ++bx) {
        QByteArray b(16, '\0');
        b[0] = char(255); b[1] = char(0);                        // alpha endpoints, all index 0 -> 255
        quint16 c0 = quint16(((bx * 31 / (w / 4)) << 11) | ((by * 63 / (h / 4)) << 5) | 20), c1 = 0x001F;
        b[8] = char(c0 & 255); b[9] = char(c0 >> 8); b[10] = char(c1 & 255); b[11] = char(c1 >> 8);
        data += b;
    }
    p32(84, data.size());
    return hd + data;
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QString dir = argv[1]; QDir().mkpath(dir);
    QString err, path = dir + "/demo.mpq";
    {
        MpqArchive a; a.create(path, 2, 256, &err);
        wr(dir + "/t.lua", "local function hello()\n  print(\"Hello from inside an MPQ\")\nend\nhello()\n");
        wr(dir + "/t.blp", makeBlpDxt5(128, 64));
        QImage png(64, 64, QImage::Format_ARGB32); png.fill(Qt::darkCyan); { QPainter p(&png); p.setBrush(Qt::yellow); p.drawEllipse(8, 8, 48, 48); }
        png.save(dir + "/t.png");
        QByteArray bin; for (int i = 0; i < 600; ++i) bin += char(i * 7);
        wr(dir + "/t.bin", bin);
        a.addFile(dir + "/t.lua", "Interface\\AddOns\\Demo\\Demo.lua", 0x02, &err);
        a.addFile(dir + "/t.blp", "Interface\\Icons\\gradient.blp", 0x02, &err);
        a.addFile(dir + "/t.png", "Interface\\Icons\\circle.png", 0x02, &err);
        a.addFile(dir + "/t.bin", "Data\\blob.bin", 0, &err);
        a.addFile(dir + "/t.lua", "Interface\\AddOns\\Demo\\Other.lua", 0x02, &err);
        a.close();
    }
    struct TW : MainWindow { using MainWindow::dropEvent; };
    TW w; w.resize(1100, 640); w.show();
    if (!w.openArchive(path)) { std::printf("open failed\n"); return 1; }
    auto* tv = w.findChild<QTreeView*>();
    tv->expandAll();
    auto pick = [&](const QString& name, const QString& shot) {
        std::function<QModelIndex(const QModelIndex&)> find = [&](const QModelIndex& p) -> QModelIndex {
            for (int i = 0; i < tv->model()->rowCount(p); ++i) {
                QModelIndex c = tv->model()->index(i, 0, p);
                if (c.data(Qt::DisplayRole).toString() == name) return c;
                QModelIndex r = find(c); if (r.isValid()) return r;
            }
            return {};
        };
        QModelIndex i = find({});
        if (!i.isValid()) { std::printf("not found %s\n", qPrintable(name)); return; }
        tv->setCurrentIndex(i);
        qApp->processEvents();
        w.grab().save(dir + "/" + shot + ".png");
        std::printf("shot %s\n", qPrintable(shot));
    };
    pick("Demo.lua", "shot_text");
    pick("gradient.blp", "shot_blp");
    pick("circle.png", "shot_png");
    pick("blob.bin", "shot_hex");
    w.findChild<QLineEdit*>()->setText("other");
    qApp->processEvents();
    w.grab().save(dir + "/shot_filter.png");
    // Drop a folder onto the window -> files must be added under the selected folder and persist.
    QDir().mkpath(dir + "/dropme/inner");
    wr(dir + "/dropme/x.txt", "x"); wr(dir + "/dropme/inner/y.txt", "yy");
    w.findChild<QLineEdit*>()->clear();
    qApp->processEvents();
    auto* mime = new QMimeData; mime->setUrls({QUrl::fromLocalFile(dir + "/dropme")});
    QDropEvent ev(QPointF(10, 10), Qt::CopyAction, mime, Qt::LeftButton, Qt::NoModifier);
    w.dropEvent(&ev);
    qApp->processEvents();
    delete mime;
    w.grab().save(dir + "/shot_after_drop.png");
    w.openArchive(path);   // reopen from disk to prove it persisted
    MpqArchive chk; chk.open(path, true, &err);
    QVector<MpqEntry> es; chk.loadEntries(es, &err); for (auto& e : es) std::printf("entry: %s\n", qPrintable(e.path));
    return 0;
}
