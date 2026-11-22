#include "mainwindow.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QDirIterator>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QProgressDialog>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSettings>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTreeView>
#include <QUrl>
#include <QVBoxLayout>

#include "blp.h"

namespace {
constexpr quint32 COMP_ZLIB = 0x02, COMP_BZIP2 = 0x10, COMP_LZMA = 0x12;
constexpr qint64 MAX_TEXT_PREVIEW = 2 * 1024 * 1024;
constexpr qint64 MAX_IMAGE_PREVIEW = 32 * 1024 * 1024;
constexpr qint64 MAX_HEX_PREVIEW = 64 * 1024;

QString extOf(const QString& path)
{
    const int dot = path.lastIndexOf(QLatin1Char('.'));
    const int sep = path.lastIndexOf(QLatin1Char('\\'));
    return dot > sep ? path.mid(dot + 1).toLower() : QString();
}

bool isImageExt(const QString& e)
{
    return e == QLatin1String("blp") || e == QLatin1String("png") || e == QLatin1String("jpg") ||
           e == QLatin1String("jpeg") || e == QLatin1String("bmp") || e == QLatin1String("gif");
}

bool looksLikeText(const QString& ext, const QByteArray& d)
{
    static const QSet<QString> known = {"txt", "lua", "xml", "toc", "ini", "cfg", "lst", "wtf", "json", "html", "htm",
                                        "css", "js", "slk", "j", "ai", "wts", "csv", "md", "xsd", "fdf", "sig", "bat"};
    if (known.contains(ext))
        return true;
    const QByteArray s = d.left(8192);
    if (s.isEmpty())
        return false;
    int ctl = 0;
    for (uchar c : s) {
        if (c == 0)
            return false;
        if (c < 0x20 && c != '\t' && c != '\n' && c != '\r')
            ++ctl;
    }
    if (ctl * 50 > s.size())
        return false;
    return !QString::fromUtf8(s).contains(QChar::ReplacementCharacter);
}

QString hexDump(const QByteArray& d, qint64 limit)
{
    const int n = int(qMin<qint64>(d.size(), limit));
    QString out;
    out.reserve(n * 4);
    for (int off = 0; off < n; off += 16) {
        out += QStringLiteral("%1  ").arg(off, 8, 16, QLatin1Char('0'));
        QString asc;
        for (int i = 0; i < 16; ++i) {
            if (off + i < n) {
                const uchar c = uchar(d[off + i]);
                out += QStringLiteral("%1 ").arg(c, 2, 16, QLatin1Char('0'));
                asc += (c >= 0x20 && c < 0x7f) ? QChar(c) : QLatin1Char('.');
            } else {
                out += QStringLiteral("   ");
            }
            if (i == 7)
                out += QLatin1Char(' ');
        }
        out += QStringLiteral(" |") + asc + QStringLiteral("|\n");
    }
    return out;
}
} // namespace

// ---------------------------------------------------------------------------

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
{
    setAcceptDrops(true);
    buildUi();
    buildMenus();

    QSettings s;
    m_listfile = s.value(QStringLiteral("listfile")).toString();
    const quint32 comp = s.value(QStringLiteral("compression"), COMP_ZLIB).toUInt();
    for (QAction* a : m_compGroup->actions())
        if (a->data().toUInt() == comp)
            a->setChecked(true);
    if (!restoreGeometry(s.value(QStringLiteral("geometry")).toByteArray()))
        resize(1100, 700);

    updateUi();
    updateStatus();
}

void MainWindow::buildUi()
{
    m_model = new MpqModel(this);
    m_proxy = new MpqProxy(this);
    m_proxy->setSourceModel(m_model);

    m_filter = new QLineEdit;
    m_filter->setPlaceholderText(QStringLiteral("Filter by path…"));
    m_filter->setClearButtonEnabled(true);

    m_tree = new QTreeView;
    m_tree->setModel(m_proxy);
    m_tree->setSortingEnabled(true);
    m_tree->sortByColumn(MpqModel::ColName, Qt::AscendingOrder);
    m_tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_tree->setUniformRowHeights(true);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tree->setAlternatingRowColors(true);
    m_tree->header()->setStretchLastSection(true);
    m_tree->setColumnWidth(MpqModel::ColName, 320);
    m_tree->setColumnWidth(MpqModel::ColSize, 80);
    m_tree->setColumnWidth(MpqModel::ColPacked, 80);
    m_tree->setColumnWidth(MpqModel::ColRatio, 55);

    auto* left = new QWidget;
    auto* lv = new QVBoxLayout(left);
    lv->setContentsMargins(0, 0, 0, 0);
    lv->addWidget(m_filter);
    lv->addWidget(m_tree);

    m_previewTitle = new QLabel;
    m_previewTitle->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_previewTitle->setWordWrap(true);
    m_previewMode = new QComboBox;
    m_previewMode->addItems({QStringLiteral("Auto"), QStringLiteral("Text"), QStringLiteral("Hex")});

    m_placeholder = new QLabel;
    m_placeholder->setAlignment(Qt::AlignCenter);
    m_placeholder->setWordWrap(true);

    m_text = new QPlainTextEdit;
    m_text->setReadOnly(true);
    m_text->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_text->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

    m_image = new QLabel;
    m_image->setAlignment(Qt::AlignCenter);
    m_imageScroll = new QScrollArea;
    m_imageScroll->setWidget(m_image);
    m_imageScroll->setWidgetResizable(true);
    m_imageScroll->setBackgroundRole(QPalette::Dark);

    m_stack = new QStackedWidget;
    m_stack->addWidget(m_placeholder);
    m_stack->addWidget(m_text);
    m_stack->addWidget(m_imageScroll);

    auto* right = new QWidget;
    auto* rv = new QVBoxLayout(right);
    rv->setContentsMargins(0, 0, 0, 0);
    auto* head = new QHBoxLayout;
    head->addWidget(m_previewTitle, 1);
    head->addWidget(m_previewMode);
    rv->addLayout(head);
    rv->addWidget(m_stack, 1);

    auto* split = new QSplitter;
    split->addWidget(left);
    split->addWidget(right);
    split->setStretchFactor(0, 3);
    split->setStretchFactor(1, 2);
    setCentralWidget(split);

    m_statusLeft = new QLabel;
    m_statusRight = new QLabel;
    statusBar()->addWidget(m_statusLeft, 1);
    statusBar()->addPermanentWidget(m_statusRight);

    connect(m_filter, &QLineEdit::textChanged, this, [this](const QString& t) {
        m_proxy->setFilterFixedString(t);
        if (!t.isEmpty())
            m_tree->expandAll();
    });
    connect(m_tree->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this] { onSelectionChanged(); });
    connect(m_tree, &QTreeView::customContextMenuRequested, this, &MainWindow::showContextMenu);
    connect(m_tree, &QTreeView::doubleClicked, this, [this](const QModelIndex& i) {
        if (!m_model->nodeFor(m_proxy->mapToSource(i))->isDir)
            extractSelected();
    });
    connect(m_previewMode, &QComboBox::currentIndexChanged, this, [this] { renderPreview(); });

    clearPreview(QString());
}

void MainWindow::buildMenus()
{
    auto mk = [this](const QString& text, auto slot, const QKeySequence& key = {}) {
        auto* a = new QAction(text, this);
        if (!key.isEmpty())
            a->setShortcut(key);
        connect(a, &QAction::triggered, this, slot);
        return a;
    };

    aNew = mk(QStringLiteral("&New Archive…"), [this] { newArchive(); }, QKeySequence::New);
    aOpen = mk(QStringLiteral("&Open…"), [this] { openDialog(false); }, QKeySequence::Open);
    aOpenRO = mk(QStringLiteral("Open &Read-Only…"), [this] { openDialog(true); }, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O));
    aClose = mk(QStringLiteral("&Close Archive"), [this] { closeArchive(); }, QKeySequence::Close);
    aQuit = mk(QStringLiteral("&Quit"), [this] { close(); }, QKeySequence::Quit);

    aListfile = mk(QStringLiteral("Load &Listfile…"), [this] { loadListfile(); });
    aForget = mk(QStringLiteral("Forget Saved Listfile"), [this] { forgetListfile(); });
    aAddFiles = mk(QStringLiteral("Add &Files…"), [this] { addFilesDialog(); }, QKeySequence(Qt::CTRL | Qt::Key_I));
    aAddFolder = mk(QStringLiteral("Add Fol&der…"), [this] { addFolderDialog(); });
    aExtract = mk(QStringLiteral("&Extract Selected…"), [this] { extractSelected(); }, QKeySequence(Qt::CTRL | Qt::Key_E));
    aExtractAll = mk(QStringLiteral("Extract &All…"), [this] { extractAll(); }, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_E));
    aRename = mk(QStringLiteral("&Rename / Move…"), [this] { renameSelected(); }, QKeySequence(Qt::Key_F2));
    aDelete = mk(QStringLiteral("&Delete"), [this] { deleteSelected(); }, QKeySequence::Delete);
    aCompact = mk(QStringLiteral("&Compact Archive"), [this] { compactArchive(); });
    aMaxFiles = mk(QStringLiteral("Set &Max File Count…"), [this] { setMaxFiles(); });
    aInfo = mk(QStringLiteral("Archive &Info…"), [this] { showInfo(); }, QKeySequence(Qt::ALT | Qt::Key_Return));

    // Delete / F2 must not fire while typing in the filter box.
    aDelete->setShortcutContext(Qt::WidgetShortcut);
    aRename->setShortcutContext(Qt::WidgetShortcut);
    m_tree->addAction(aDelete);
    m_tree->addAction(aRename);

    QMenu* mf = menuBar()->addMenu(QStringLiteral("&File"));
    mf->addActions({aNew, aOpen, aOpenRO, aClose});
    mf->addSeparator();
    mf->addAction(aQuit);

    QMenu* ma = menuBar()->addMenu(QStringLiteral("&Archive"));
    ma->addActions({aAddFiles, aAddFolder});
    ma->addSeparator();
    ma->addActions({aExtract, aExtractAll});
    ma->addSeparator();
    ma->addActions({aRename, aDelete});
    ma->addSeparator();
    ma->addActions({aListfile, aForget});
    ma->addSeparator();
    ma->addActions({aCompact, aMaxFiles, aInfo});

    QMenu* mo = menuBar()->addMenu(QStringLiteral("&Options"));
    QMenu* mc = mo->addMenu(QStringLiteral("Compression for added files"));
    m_compGroup = new QActionGroup(this);
    struct { const char* label; quint32 v; } comps[] = {
        {"None (store)", 0},
        {"Zlib (most compatible)", COMP_ZLIB},
        {"BZip2 (Warcraft III and later)", COMP_BZIP2},
        {"LZMA (StarCraft II and later only)", COMP_LZMA},
    };
    for (const auto& c : comps) {
        auto* a = mc->addAction(QString::fromLatin1(c.label));
        a->setCheckable(true);
        a->setData(c.v);
        m_compGroup->addAction(a);
        if (c.v == COMP_ZLIB)
            a->setChecked(true);
        connect(a, &QAction::triggered, this, [c] { QSettings().setValue(QStringLiteral("compression"), c.v); });
    }

    QMenu* mh = menuBar()->addMenu(QStringLiteral("&Help"));
    mh->addAction(mk(QStringLiteral("&About"), [this] { about(); }));
}

// --- opening / closing ------------------------------------------------------

bool MainWindow::openArchive(const QString& path, bool readOnly)
{
    if (!closeArchive())
        return false;
    QString err;
    bool ro = readOnly;
    bool ok = m_ar.open(path, ro, &err);
    if (!ok && !ro) {
        QString err2;
        if (m_ar.open(path, true, &err2)) {
            ok = true;
            ro = true;
            statusBar()->showMessage(QStringLiteral("Opened read-only (file is not writable)"), 8000);
        }
    }
    if (!ok) {
        QMessageBox::critical(this, QStringLiteral("Open failed"), err);
        updateUi();
        updateStatus();
        return false;
    }
    if (!m_listfile.isEmpty() && QFileInfo::exists(m_listfile)) {
        QString lerr;
        if (!m_ar.addListFile(m_listfile, &lerr))
            statusBar()->showMessage(lerr, 8000);
    }
    reload();
    setWindowTitle(QStringLiteral("%1%2").arg(QFileInfo(path).fileName(), ro ? QStringLiteral(" [read-only]") : QString()));
    updateUi();
    updateStatus();
    return true;
}

void MainWindow::openDialog(bool readOnly)
{
    QSettings s;
    const QString f = QFileDialog::getOpenFileName(
        this, QStringLiteral("Open MPQ archive"), s.value(QStringLiteral("lastDir")).toString(),
        QStringLiteral("MPQ archives (*.mpq *.MPQ *.w3x *.w3m *.scx *.scm *.sc2map *.sc2mod *.SC2Replay *.exe);;All files (*)"));
    if (f.isEmpty())
        return;
    s.setValue(QStringLiteral("lastDir"), QFileInfo(f).absolutePath());
    openArchive(f, readOnly);
}

void MainWindow::newArchive()
{
    QDialog d(this);
    d.setWindowTitle(QStringLiteral("New MPQ Archive"));
    auto* form = new QFormLayout(&d);
    auto* ver = new QComboBox;
    ver->addItem(QStringLiteral("v1 — Diablo II, StarCraft, Warcraft III, WoW 1.x"), 1);
    ver->addItem(QStringLiteral("v2 — WoW 2.x–3.x (Burning Crusade, Wrath)"), 2);
    ver->addItem(QStringLiteral("v3 — WoW 4.x, early StarCraft II"), 3);
    ver->addItem(QStringLiteral("v4 — StarCraft II, Diablo III"), 4);
    auto* maxf = new QSpinBox;
    maxf->setRange(16, 1000000);
    maxf->setValue(4096);
    form->addRow(QStringLiteral("Format:"), ver);
    form->addRow(QStringLiteral("Max files:"), maxf);
    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    form->addRow(bb);
    connect(bb, &QDialogButtonBox::accepted, &d, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &d, &QDialog::reject);
    if (d.exec() != QDialog::Accepted)
        return;

    QSettings s;
    QString f = QFileDialog::getSaveFileName(this, QStringLiteral("Create MPQ archive"),
                                             s.value(QStringLiteral("lastDir")).toString(),
                                             QStringLiteral("MPQ archives (*.mpq)"));
    if (f.isEmpty())
        return;
    if (QFileInfo(f).suffix().isEmpty())
        f += QStringLiteral(".mpq");
    s.setValue(QStringLiteral("lastDir"), QFileInfo(f).absolutePath());

    if (!closeArchive())
        return;
    QString err;
    if (!m_ar.create(f, ver->currentData().toInt(), quint32(maxf->value()), &err)) {
        QMessageBox::critical(this, QStringLiteral("Create failed"), err);
        return;
    }
    if (!m_ar.flush(&err))
        QMessageBox::warning(this, QStringLiteral("Warning"), err);
    reload();
    setWindowTitle(QFileInfo(f).fileName());
    updateUi();
    updateStatus();
}

bool MainWindow::closeArchive()
{
    if (m_ar.isOpen())
        m_ar.close();
    m_model->clear();
    clearPreview(QString());
    setWindowTitle(QString());
    updateUi();
    updateStatus();
    return true;
}

void MainWindow::closeEvent(QCloseEvent* e)
{
    QSettings().setValue(QStringLiteral("geometry"), saveGeometry());
    closeArchive();
    e->accept();
}

// --- listing -----------------------------------------------------------------

QSet<QString> MainWindow::expandedPaths() const
{
    QSet<QString> out;
    std::function<void(const QModelIndex&)> walk = [&](const QModelIndex& src) {
        const int n = m_model->rowCount(src);
        for (int i = 0; i < n; ++i) {
            const QModelIndex c = m_model->index(i, 0, src);
            const MpqNode* node = m_model->nodeFor(c);
            if (!node->isDir)
                continue;
            if (m_tree->isExpanded(m_proxy->mapFromSource(c))) {
                out.insert(node->path.toLower());
                walk(c);
            }
        }
    };
    walk(QModelIndex());
    return out;
}

void MainWindow::restoreExpanded(const QSet<QString>& paths)
{
    QStringList sorted(paths.begin(), paths.end());
    std::sort(sorted.begin(), sorted.end(), [](const QString& a, const QString& b) { return a.size() < b.size(); });
    for (const QString& p : sorted) {
        const QModelIndex src = m_model->indexForPath(p);
        if (src.isValid())
            m_tree->expand(m_proxy->mapFromSource(src));
    }
}

void MainWindow::reload()
{
    const bool first = m_model->rowCount() == 0;
    const QSet<QString> expanded = expandedPaths();
    QVector<MpqEntry> entries;
    QString err;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const bool ok = m_ar.loadEntries(entries, &err);
    m_model->setEntries(entries);
    QApplication::restoreOverrideCursor();
    if (!ok)
        QMessageBox::warning(this, QStringLiteral("Listing failed"), err);
    if (first && m_model->rowCount() <= 8) {
        for (int i = 0; i < m_proxy->rowCount(); ++i)
            m_tree->expand(m_proxy->index(i, 0));
    } else {
        restoreExpanded(expanded);
    }
    clearPreview(QString());
    updateStatus();
}

void MainWindow::loadListfile()
{
    if (!m_ar.isOpen())
        return;
    const QString f = QFileDialog::getOpenFileName(this, QStringLiteral("Select listfile"), QString(),
                                                   QStringLiteral("Listfiles (*.txt *.lst *.csv);;All files (*)"));
    if (f.isEmpty())
        return;
    QString err;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const bool ok = m_ar.addListFile(f, &err);
    QApplication::restoreOverrideCursor();
    if (!ok) {
        QMessageBox::warning(this, QStringLiteral("Listfile"), err);
        return;
    }
    m_listfile = f;
    QSettings().setValue(QStringLiteral("listfile"), f);
    reload();
    statusBar()->showMessage(QStringLiteral("Listfile applied; it will be applied automatically to archives you open next."), 8000);
}

void MainWindow::forgetListfile()
{
    m_listfile.clear();
    QSettings().remove(QStringLiteral("listfile"));
    statusBar()->showMessage(QStringLiteral("Saved listfile cleared (names already resolved stay resolved until you reopen)."), 8000);
}

// --- selection helpers ---------------------------------------------------------

QModelIndexList MainWindow::selectedSourceIndexes() const
{
    QModelIndexList out;
    const QModelIndexList rows = m_tree->selectionModel()->selectedRows(0);
    for (const QModelIndex& i : rows)
        out << m_proxy->mapToSource(i);
    return out;
}

QStringList MainWindow::selectedFiles() const
{
    QStringList out;
    QSet<QString> seen;
    for (const QModelIndex& i : selectedSourceIndexes()) {
        for (const QString& p : m_model->filesUnder(i)) {
            if (!seen.contains(p)) {
                seen.insert(p);
                out << p;
            }
        }
    }
    return out;
}

QString MainWindow::targetPrefix() const
{
    const QModelIndexList sel = selectedSourceIndexes();
    if (sel.size() != 1)
        return QString();
    const MpqNode* n = m_model->nodeFor(sel.first());
    if (n->isDir)
        return n->path + QLatin1Char('\\');
    const MpqNode* p = n->parent;
    return (p && !p->path.isEmpty()) ? p->path + QLatin1Char('\\') : QString();
}

void MainWindow::onSelectionChanged()
{
    updateUi();
    // Summary: count files/bytes, skipping nodes already covered by a selected ancestor.
    const QModelIndexList sel = selectedSourceIndexes();
    QSet<const MpqNode*> chosen;
    for (const QModelIndex& i : sel)
        chosen.insert(m_model->nodeFor(i));
    quint64 bytes = 0;
    quint32 files = 0;
    for (const MpqNode* n : chosen) {
        bool covered = false;
        for (const MpqNode* p = n->parent; p; p = p->parent)
            if (chosen.contains(p)) { covered = true; break; }
        if (!covered) {
            bytes += n->size;
            files += n->fileCount;
        }
    }
    m_statusRight->setText(sel.isEmpty() ? QString()
                                         : QStringLiteral("%1 file(s) selected, %2").arg(files).arg(MpqModel::sizeText(bytes)));
    showPreview();
}

void MainWindow::updateUi()
{
    const bool open = m_ar.isOpen();
    const bool rw = open && !m_ar.isReadOnly();
    const bool sel = !selectedSourceIndexes().isEmpty();
    const bool one = selectedSourceIndexes().size() == 1;
    aClose->setEnabled(open);
    aListfile->setEnabled(open);
    aExtract->setEnabled(open && sel);
    aExtractAll->setEnabled(open);
    aAddFiles->setEnabled(rw);
    aAddFolder->setEnabled(rw);
    aDelete->setEnabled(rw && sel);
    aRename->setEnabled(rw && one);
    aCompact->setEnabled(rw);
    aMaxFiles->setEnabled(rw);
    aInfo->setEnabled(open);
    m_filter->setEnabled(open);
}

void MainWindow::updateStatus()
{
    if (!m_ar.isOpen()) {
        m_statusLeft->setText(QStringLiteral("No archive open — use File > Open, or drop an .mpq onto this window."));
        return;
    }
    const MpqInfo i = m_ar.info();
    m_statusLeft->setText(QStringLiteral("%1 — MPQ v%2 — %3 / %4 files%5")
                              .arg(m_ar.path())
                              .arg(i.formatVersion)
                              .arg(i.fileCount)
                              .arg(i.maxFileCount)
                              .arg(i.readOnly ? QStringLiteral(" — read-only") : QString()));
}

// --- preview ---------------------------------------------------------------------

void MainWindow::clearPreview(const QString& message)
{
    m_previewName.clear();
    m_previewData.clear();
    m_previewTitle->clear();
    m_placeholder->setText(message.isEmpty() ? QStringLiteral("Select a file to preview it") : message);
    m_stack->setCurrentWidget(m_placeholder);
}

void MainWindow::showPreview()
{
    const QModelIndexList sel = selectedSourceIndexes();
    if (sel.size() != 1 || m_model->nodeFor(sel.first())->isDir) {
        clearPreview(QString());
        return;
    }
    const MpqNode* n = m_model->nodeFor(sel.first());
    m_previewName = n->path;
    m_previewTotal = qint64(n->size);
    const qint64 limit = isImageExt(extOf(n->path)) ? MAX_IMAGE_PREVIEW : MAX_TEXT_PREVIEW;
    QString err;
    if (!m_ar.readFile(n->path, m_previewData, limit, &m_previewTruncated, &err)) {
        clearPreview(err);
        m_previewTitle->setText(n->path);
        return;
    }
    renderPreview();
}

void MainWindow::renderPreview()
{
    if (m_previewName.isEmpty())
        return;
    QString title = QStringLiteral("%1 — %2").arg(m_previewName, MpqModel::sizeText(quint64(m_previewTotal)));
    const QString ext = extOf(m_previewName);
    const int mode = m_previewMode->currentIndex();  // 0 auto, 1 text, 2 hex

    if (mode == 0 && isImageExt(ext) && !m_previewTruncated) {
        QImage img;
        QString err;
        if (ext == QLatin1String("blp"))
            img = decodeBlp(m_previewData, &err);
        else if (!img.loadFromData(m_previewData))
            err = QStringLiteral("Unsupported image data");
        if (!img.isNull()) {
            m_image->setPixmap(QPixmap::fromImage(img));
            m_stack->setCurrentWidget(m_imageScroll);
            m_previewTitle->setText(title + QStringLiteral(" — %1×%2").arg(img.width()).arg(img.height()));
            return;
        }
        title += QStringLiteral(" — %1").arg(err);
    }

    const bool text = (mode == 1) || (mode == 0 && looksLikeText(ext, m_previewData));
    if (text) {
        QString s = QString::fromUtf8(m_previewData);
        if (s.contains(QChar::ReplacementCharacter))
            s = QString::fromLatin1(m_previewData);
        m_text->setPlainText(s);
        if (m_previewTruncated)
            title += QStringLiteral(" — showing first %1").arg(MpqModel::sizeText(quint64(m_previewData.size())));
    } else {
        m_text->setPlainText(hexDump(m_previewData, MAX_HEX_PREVIEW));
        if (m_previewTotal > MAX_HEX_PREVIEW)
            title += QStringLiteral(" — showing first %1").arg(MpqModel::sizeText(MAX_HEX_PREVIEW));
    }
    m_stack->setCurrentWidget(m_text);
    m_previewTitle->setText(title);
}

// --- extraction ---------------------------------------------------------------------

QString MainWindow::safeRelPath(const QString& mpqPath)
{
    static const QRegularExpression sep(QStringLiteral("[\\\\/]"));
    QStringList out;
    for (QString p : mpqPath.split(sep, Qt::SkipEmptyParts)) {
        if (p == QLatin1String(".") || p == QLatin1String(".."))
            continue;
        p.replace(QLatin1Char(':'), QLatin1Char('_'));
        out << p;
    }
    return out.join(QLatin1Char('/'));
}

bool MainWindow::extractFiles(const QStringList& files, const QString& destDir)
{
    if (files.isEmpty())
        return true;
    const QString root = QDir::cleanPath(QDir(destDir).absolutePath());
    QProgressDialog prog(QStringLiteral("Extracting…"), QStringLiteral("Cancel"), 0, int(files.size()), this);
    prog.setWindowModality(Qt::WindowModal);
    prog.setMinimumDuration(300);

    enum { Ask, All, None } policy = Ask;
    int ok = 0, skipped = 0, failed = 0;
    QStringList errors;
    for (int i = 0; i < files.size(); ++i) {
        prog.setValue(i);
        if (prog.wasCanceled())
            break;
        const QString rel = safeRelPath(files[i]);
        const QString dest = QDir::cleanPath(root + QLatin1Char('/') + rel);
        if (rel.isEmpty() || !dest.startsWith(root + QLatin1Char('/'))) {
            ++failed;
            errors << QStringLiteral("Refusing unsafe path: %1").arg(files[i]);
            continue;
        }
        if (QFileInfo::exists(dest)) {
            if (policy == Ask) {
                QMessageBox box(QMessageBox::Question, QStringLiteral("File exists"),
                                QStringLiteral("%1 already exists.\nOverwrite?").arg(dest),
                                QMessageBox::Yes | QMessageBox::YesToAll | QMessageBox::No | QMessageBox::NoToAll, this);
                box.setEscapeButton(QMessageBox::No);
                const int r = box.exec();
                if (r == QMessageBox::YesToAll) policy = All;
                else if (r == QMessageBox::NoToAll) policy = None;
                else if (r == QMessageBox::No) { ++skipped; continue; }
            }
            if (policy == None) { ++skipped; continue; }
        }
        QString err;
        if (m_ar.extractFile(files[i], dest, &err)) {
            ++ok;
        } else {
            ++failed;
            errors << err;
        }
    }
    prog.setValue(int(files.size()));
    statusBar()->showMessage(QStringLiteral("Extracted %1 file(s) to %2%3%4")
                                 .arg(ok).arg(root)
                                 .arg(skipped ? QStringLiteral(", skipped %1").arg(skipped) : QString())
                                 .arg(failed ? QStringLiteral(", %1 failed").arg(failed) : QString()), 10000);
    if (failed) {
        errors = errors.mid(0, 12);
        QMessageBox::warning(this, QStringLiteral("Extraction problems"),
                             QStringLiteral("%1 file(s) could not be extracted:\n\n%2").arg(failed).arg(errors.join(QLatin1Char('\n'))));
    }
    return failed == 0;
}

void MainWindow::extractSelected()
{
    const QStringList files = selectedFiles();
    if (files.isEmpty())
        return;
    QSettings s;
    const QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("Extract %1 file(s) to…").arg(files.size()),
                                                          s.value(QStringLiteral("lastExtractDir")).toString());
    if (dir.isEmpty())
        return;
    s.setValue(QStringLiteral("lastExtractDir"), dir);
    extractFiles(files, dir);
}

void MainWindow::extractAll()
{
    if (!m_ar.isOpen())
        return;
    const QStringList files = m_model->filesUnder(QModelIndex());
    QSettings s;
    const QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("Extract all %1 files to…").arg(files.size()),
                                                          s.value(QStringLiteral("lastExtractDir")).toString());
    if (dir.isEmpty())
        return;
    s.setValue(QStringLiteral("lastExtractDir"), dir);
    extractFiles(files, dir);
}

// --- editing ---------------------------------------------------------------------------

bool MainWindow::requireWritable()
{
    if (!m_ar.isOpen())
        return false;
    if (m_ar.isReadOnly()) {
        QMessageBox::information(this, QStringLiteral("Read-only"),
                                 QStringLiteral("This archive is open read-only. Reopen it with File > Open to edit."));
        return false;
    }
    return true;
}

quint32 MainWindow::compressionMask() const
{
    QAction* a = m_compGroup->checkedAction();
    return a ? a->data().toUInt() : COMP_ZLIB;
}

bool MainWindow::finishEdit(const QString& what)
{
    QString err;
    const bool ok = m_ar.flush(&err);
    if (!ok)
        QMessageBox::critical(this, QStringLiteral("Write failed"), err);
    reload();
    statusBar()->showMessage(what, 8000);
    return ok;
}

void MainWindow::addPaths(const QStringList& localPaths, const QString& prefix)
{
    if (!requireWritable())
        return;
    struct Item { QString src, name; };
    QVector<Item> items;
    for (const QString& p : localPaths) {
        const QFileInfo fi(p);
        if (fi.isDir()) {
            const QDir base(fi.absoluteFilePath());
            QDirIterator it(base.absolutePath(), QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                const QString f = it.next();
                QString rel = base.relativeFilePath(f);
                rel.replace(QLatin1Char('/'), QLatin1Char('\\'));
                items.push_back({f, prefix + fi.fileName() + QLatin1Char('\\') + rel});
            }
        } else if (fi.isFile()) {
            items.push_back({fi.absoluteFilePath(), prefix + fi.fileName()});
        }
    }
    if (items.isEmpty())
        return;

    int conflicts = 0;
    for (const Item& it : items)
        if (m_ar.hasFile(it.name))
            ++conflicts;
    if (conflicts &&
        QMessageBox::question(this, QStringLiteral("Replace files?"),
                              QStringLiteral("%1 of %2 file(s) already exist in the archive and will be replaced. Continue?")
                                  .arg(conflicts).arg(items.size())) != QMessageBox::Yes)
        return;

    QProgressDialog prog(QStringLiteral("Adding files…"), QStringLiteral("Cancel"), 0, int(items.size()), this);
    prog.setWindowModality(Qt::WindowModal);
    prog.setMinimumDuration(300);
    int added = 0;
    QStringList errors;
    for (int i = 0; i < items.size(); ++i) {
        prog.setValue(i);
        if (prog.wasCanceled())
            break;
        QString err;
        if (m_ar.addFile(items[i].src, items[i].name, compressionMask(), &err))
            ++added;
        else
            errors << err;
        if (errors.size() >= 5)
            break;
    }
    prog.setValue(int(items.size()));
    finishEdit(QStringLiteral("Added %1 file(s)").arg(added));
    if (!errors.isEmpty())
        QMessageBox::warning(this, QStringLiteral("Some files were not added"), errors.join(QLatin1Char('\n')));
}

void MainWindow::addFilesDialog()
{
    if (!requireWritable())
        return;
    QSettings s;
    const QStringList f = QFileDialog::getOpenFileNames(this, QStringLiteral("Add files"), s.value(QStringLiteral("lastAddDir")).toString());
    if (f.isEmpty())
        return;
    s.setValue(QStringLiteral("lastAddDir"), QFileInfo(f.first()).absolutePath());
    addPaths(f, targetPrefix());
}

void MainWindow::addFolderDialog()
{
    if (!requireWritable())
        return;
    QSettings s;
    const QString d = QFileDialog::getExistingDirectory(this, QStringLiteral("Add folder"), s.value(QStringLiteral("lastAddDir")).toString());
    if (d.isEmpty())
        return;
    s.setValue(QStringLiteral("lastAddDir"), QFileInfo(d).absolutePath());
    addPaths({d}, targetPrefix());
}

void MainWindow::deleteSelected()
{
    if (!requireWritable())
        return;
    const QStringList files = selectedFiles();
    if (files.isEmpty())
        return;
    if (QMessageBox::warning(this, QStringLiteral("Delete"),
                             QStringLiteral("Delete %1 file(s) from the archive?").arg(files.size()),
                             QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes)
        return;
    QProgressDialog prog(QStringLiteral("Deleting…"), QStringLiteral("Cancel"), 0, int(files.size()), this);
    prog.setWindowModality(Qt::WindowModal);
    prog.setMinimumDuration(300);
    int done = 0;
    QStringList errors;
    for (int i = 0; i < files.size(); ++i) {
        prog.setValue(i);
        if (prog.wasCanceled())
            break;
        QString err;
        if (m_ar.removeFile(files[i], &err))
            ++done;
        else
            errors << err;
        if (errors.size() >= 5)
            break;
    }
    prog.setValue(int(files.size()));
    finishEdit(QStringLiteral("Deleted %1 file(s)").arg(done));
    if (!errors.isEmpty())
        QMessageBox::warning(this, QStringLiteral("Some files were not deleted"), errors.join(QLatin1Char('\n')));
}

void MainWindow::renameSelected()
{
    if (!requireWritable())
        return;
    const QModelIndexList sel = selectedSourceIndexes();
    if (sel.size() != 1)
        return;
    const MpqNode* n = m_model->nodeFor(sel.first());
    const QString oldPath = n->path;
    bool ok = false;
    QString np = QInputDialog::getText(this, n->isDir ? QStringLiteral("Rename / move folder") : QStringLiteral("Rename / move file"),
                                       QStringLiteral("New path (use \\ between folders):"), QLineEdit::Normal, oldPath, &ok);
    np = np.trimmed();
    np.replace(QLatin1Char('/'), QLatin1Char('\\'));
    if (!ok || np.isEmpty() || np == oldPath)
        return;

    QStringList files = m_model->filesUnder(sel.first());
    int done = 0;
    QStringList errors;
    for (const QString& f : files) {
        const QString target = n->isDir ? np + f.mid(oldPath.size()) : np;
        QString err;
        if (m_ar.renameFile(f, target, &err))
            ++done;
        else
            errors << err;
        if (errors.size() >= 5)
            break;
    }
    finishEdit(QStringLiteral("Renamed %1 file(s)").arg(done));
    if (!errors.isEmpty())
        QMessageBox::warning(this, QStringLiteral("Rename problems"), errors.join(QLatin1Char('\n')));
}

void MainWindow::compactArchive()
{
    if (!requireWritable())
        return;
    if (QMessageBox::question(this, QStringLiteral("Compact archive"),
                              QStringLiteral("Rewrite the archive to reclaim unused space? This can take a while for large archives.")) != QMessageBox::Yes)
        return;
    QString err;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const bool ok = m_ar.compact(&err);
    QApplication::restoreOverrideCursor();
    if (!ok)
        QMessageBox::critical(this, QStringLiteral("Compact failed"), err);
    reload();
    if (ok)
        statusBar()->showMessage(QStringLiteral("Archive compacted"), 8000);
}

void MainWindow::setMaxFiles()
{
    if (!requireWritable())
        return;
    const MpqInfo i = m_ar.info();
    bool ok = false;
    const int v = QInputDialog::getInt(this, QStringLiteral("Max file count"),
                                       QStringLiteral("New maximum (can only grow; currently %1, %2 used):").arg(i.maxFileCount).arg(i.fileCount),
                                       int(i.maxFileCount), int(i.maxFileCount), 4000000, 1, &ok);
    if (!ok || quint32(v) == i.maxFileCount)
        return;
    QString err;
    if (!m_ar.setMaxFileCount(quint32(v), &err)) {
        QMessageBox::critical(this, QStringLiteral("Failed"), err);
        return;
    }
    finishEdit(QStringLiteral("Max file count is now %1").arg(v));
}

// --- misc ----------------------------------------------------------------------------------

void MainWindow::showInfo()
{
    if (!m_ar.isOpen())
        return;
    const MpqInfo i = m_ar.info();
    QMessageBox::information(
        this, QStringLiteral("Archive info"),
        QStringLiteral("%1\n\nFormat: MPQ v%2\nFiles: %3 (max %4)\nHash table entries: %5\nSector size: %6 bytes\n"
                       "Archive size: %7\nSignatures: %8\nMode: %9")
            .arg(m_ar.path()).arg(i.formatVersion).arg(i.fileCount).arg(i.maxFileCount).arg(i.hashTableSize)
            .arg(i.sectorSize).arg(MpqModel::sizeText(i.archiveSize))
            .arg(i.signatures ? QStringLiteral("yes (0x%1)").arg(i.signatures, 0, 16) : QStringLiteral("none"))
            .arg(i.readOnly ? QStringLiteral("read-only") : QStringLiteral("read/write")));
}

void MainWindow::about()
{
    QMessageBox::about(this, QStringLiteral("About MPQ Viewer by X08"),
                       QStringLiteral("<b>MPQ Viewer</b> %1 by X08<br>A Linux GUI for browsing and editing Blizzard MPQ archives.<br><br>"
                                      "Source: <a href=\"https://github.com/X08/MPQ-Viewer-Editor-for-Linux\">github.com/X08/MPQ-Viewer-Editor-for-Linux</a><br><br>"
                                      "Built on <a href=\"https://github.com/ladislav-zezula/StormLib\">StormLib</a> (MIT) and Qt 6.")
                           .arg(QStringLiteral(APP_VERSION)));
}

void MainWindow::showContextMenu(const QPoint& pos)
{
    QMenu m(this);
    m.addAction(aExtract);
    m.addSeparator();
    m.addAction(aAddFiles);
    m.addAction(aAddFolder);
    m.addAction(aRename);
    m.addAction(aDelete);
    m.addSeparator();
    QAction* copy = m.addAction(QStringLiteral("Copy path"));
    const QModelIndex idx = m_tree->indexAt(pos);
    copy->setEnabled(idx.isValid());
    if (m.exec(m_tree->viewport()->mapToGlobal(pos)) == copy && idx.isValid())
        QGuiApplication::clipboard()->setText(m_model->nodeFor(m_proxy->mapToSource(idx))->path);
}

void MainWindow::dragEnterEvent(QDragEnterEvent* e)
{
    if (!e->mimeData()->hasUrls())
        return;
    for (const QUrl& u : e->mimeData()->urls())
        if (!u.isLocalFile())
            return;
    e->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent* e)
{
    QStringList paths;
    for (const QUrl& u : e->mimeData()->urls())
        if (u.isLocalFile())
            paths << u.toLocalFile();
    if (paths.isEmpty())
        return;
    e->acceptProposedAction();
    if (!m_ar.isOpen())
        openArchive(paths.first());
    else
        addPaths(paths, targetPrefix());
}
