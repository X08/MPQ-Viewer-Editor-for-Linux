#include "mpqmodel.h"

#include <QCollator>
#include <QColor>
#include <algorithm>

// MPQ flag bits (kept local so this file does not need StormLib.h)
static constexpr quint32 F_IMPLODE = 0x00000100;
static constexpr quint32 F_COMPRESS = 0x00000200;
static constexpr quint32 F_ENCRYPTED = 0x00010000;
static constexpr quint32 F_FIXKEY = 0x00020000;
static constexpr quint32 F_PATCH = 0x00100000;
static constexpr quint32 F_SINGLE = 0x01000000;
static constexpr quint32 F_DELMARK = 0x02000000;
static constexpr quint32 F_CRC = 0x04000000;

MpqModel::MpqModel(QObject* parent)
    : QAbstractItemModel(parent), m_root(std::make_unique<MpqNode>())
{
    m_root->isDir = true;
}

void MpqModel::clear()
{
    beginResetModel();
    m_root = std::make_unique<MpqNode>();
    m_root->isDir = true;
    endResetModel();
}

void MpqModel::setEntries(const QVector<MpqEntry>& entries)
{
    beginResetModel();
    m_root = std::make_unique<MpqNode>();
    m_root->isDir = true;

    for (const MpqEntry& e : entries) {
        const QStringList parts = e.path.split(QLatin1Char('\\'), Qt::SkipEmptyParts);
        if (parts.isEmpty())
            continue;
        MpqNode* cur = m_root.get();
        QString built;
        for (int i = 0; i < parts.size(); ++i) {
            const bool last = (i == parts.size() - 1);
            built += (i ? QStringLiteral("\\") : QString()) + parts[i];
            // The suffix lets a file and a folder share a name; it cannot collide
            // because the last character always differs ('f' vs 'd').
            const QString key = parts[i].toLower() + (last ? QStringLiteral("|f") : QStringLiteral("|d"));
            MpqNode* child = cur->lookup.value(key, nullptr);
            if (!child) {
                auto n = std::make_unique<MpqNode>();
                n->name = parts[i];
                n->path = built;
                n->isDir = !last;
                n->parent = cur;
                child = n.get();
                cur->lookup.insert(key, child);
                cur->kids.push_back(std::move(n));
            }
            if (last) {
                child->size = e.size;
                child->comp = e.compSize;
                child->flags = e.flags;
                child->unnamed = e.unnamed;
                child->fileCount = 1;
            }
            cur = child;
        }
    }
    finalize(m_root.get());
    endResetModel();
}

void MpqModel::finalize(MpqNode* n)
{
    // Natural, case-insensitive order; directories first.
    QCollator coll;
    coll.setNumericMode(true);
    coll.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(n->kids.begin(), n->kids.end(), [&](const auto& a, const auto& b) {
        if (a->isDir != b->isDir)
            return a->isDir;
        return coll.compare(a->name, b->name) < 0;
    });
    quint64 size = 0, comp = 0;
    quint32 files = 0;
    for (int i = 0; i < int(n->kids.size()); ++i) {
        MpqNode* k = n->kids[i].get();
        k->row = i;
        if (k->isDir)
            finalize(k);
        size += k->size;
        comp += k->comp;
        files += k->fileCount;
    }
    n->lookup.clear();
    if (n->isDir) {
        n->size = size;
        n->comp = comp;
        n->fileCount = files;
    }
}

MpqNode* MpqModel::nodeFor(const QModelIndex& idx) const
{
    return idx.isValid() ? static_cast<MpqNode*>(idx.internalPointer()) : m_root.get();
}

QModelIndex MpqModel::indexForPath(const QString& path) const
{
    const QStringList parts = path.split(QLatin1Char('\\'), Qt::SkipEmptyParts);
    QModelIndex idx;
    for (const QString& p : parts) {
        const MpqNode* n = nodeFor(idx);
        bool found = false;
        for (int i = 0; i < int(n->kids.size()); ++i) {
            if (n->kids[i]->name.compare(p, Qt::CaseInsensitive) == 0) {
                idx = index(i, 0, idx);
                found = true;
                break;
            }
        }
        if (!found)
            return {};
    }
    return idx;
}

void MpqModel::collect(const MpqNode* n, QStringList& out) const
{
    if (!n->isDir) {
        out << n->path;
        return;
    }
    for (const auto& k : n->kids)
        collect(k.get(), out);
}

QStringList MpqModel::filesUnder(const QModelIndex& idx) const
{
    QStringList out;
    collect(nodeFor(idx), out);
    return out;
}

QModelIndex MpqModel::index(int row, int col, const QModelIndex& parent) const
{
    const MpqNode* p = nodeFor(parent);
    if (row < 0 || row >= int(p->kids.size()) || col < 0 || col >= ColCount)
        return {};
    return createIndex(row, col, p->kids[row].get());
}

QModelIndex MpqModel::parent(const QModelIndex& child) const
{
    if (!child.isValid())
        return {};
    const MpqNode* n = static_cast<MpqNode*>(child.internalPointer());
    const MpqNode* p = n->parent;
    if (!p || p == m_root.get())
        return {};
    return createIndex(p->row, 0, const_cast<MpqNode*>(p));
}

int MpqModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid() && parent.column() != 0)
        return 0;
    return int(nodeFor(parent)->kids.size());
}

QString MpqModel::sizeText(quint64 b)
{
    if (b < 1024)
        return QStringLiteral("%1 B").arg(b);
    static const char* u[] = {"KiB", "MiB", "GiB", "TiB"};
    double v = double(b);
    int i = -1;
    do { v /= 1024.0; ++i; } while (v >= 1024.0 && i < 3);
    return QStringLiteral("%1 %2").arg(v, 0, 'f', v < 10 ? 2 : 1).arg(QLatin1String(u[i]));
}

QString MpqModel::flagsText(quint32 f)
{
    QStringList s;
    if (f & F_COMPRESS) s << QStringLiteral("compressed");
    if (f & F_IMPLODE) s << QStringLiteral("imploded");
    if (f & F_ENCRYPTED) s << ((f & F_FIXKEY) ? QStringLiteral("encrypted+fixkey") : QStringLiteral("encrypted"));
    if (f & F_SINGLE) s << QStringLiteral("single-unit");
    if (f & F_CRC) s << QStringLiteral("crc");
    if (f & F_PATCH) s << QStringLiteral("patch");
    if (f & F_DELMARK) s << QStringLiteral("deleted");
    return s.join(QStringLiteral(", "));
}

QVariant MpqModel::data(const QModelIndex& idx, int role) const
{
    if (!idx.isValid())
        return {};
    const MpqNode* n = static_cast<MpqNode*>(idx.internalPointer());
    switch (role) {
    case PathRole: return n->path;
    case IsDirRole: return n->isDir;
    case UnnamedRole: return n->unnamed;
    case SortRole:
        switch (idx.column()) {
        case ColName: return n->name;
        case ColSize: return qulonglong(n->size);
        case ColPacked: return qulonglong(n->comp);
        case ColRatio: return n->size ? double(n->comp) / double(n->size) : 0.0;
        default: return qulonglong(n->flags);
        }
    case Qt::ToolTipRole:
        return n->path + (n->unnamed ? QStringLiteral("\n(name not in listfile)") : QString());
    case Qt::ForegroundRole:
        if (n->unnamed)
            return QColor(Qt::gray);
        return {};
    case Qt::TextAlignmentRole:
        if (idx.column() >= ColSize && idx.column() <= ColRatio)
            return int(Qt::AlignRight | Qt::AlignVCenter);
        return {};
    case Qt::DisplayRole:
        switch (idx.column()) {
        case ColName:
            return n->isDir ? QStringLiteral("%1  (%2)").arg(n->name).arg(n->fileCount) : n->name;
        case ColSize: return sizeText(n->size);
        case ColPacked: return sizeText(n->comp);
        case ColRatio:
            if (!n->size) return QString();
            return QStringLiteral("%1%").arg(100.0 * double(n->comp) / double(n->size), 0, 'f', 0);
        case ColFlags: return n->isDir ? QString() : flagsText(n->flags);
        }
        return {};
    default: return {};
    }
}

QVariant MpqModel::headerData(int section, Qt::Orientation o, int role) const
{
    if (o != Qt::Horizontal || role != Qt::DisplayRole)
        return {};
    switch (section) {
    case ColName: return QStringLiteral("Name");
    case ColSize: return QStringLiteral("Size");
    case ColPacked: return QStringLiteral("Packed");
    case ColRatio: return QStringLiteral("Ratio");
    case ColFlags: return QStringLiteral("Flags");
    }
    return {};
}

// ---------------------------------------------------------------------------

MpqProxy::MpqProxy(QObject* parent) : QSortFilterProxyModel(parent)
{
    setSortRole(MpqModel::SortRole);
    setFilterCaseSensitivity(Qt::CaseInsensitive);
    setFilterRole(MpqModel::PathRole);
    setRecursiveFilteringEnabled(true);
    setSortCaseSensitivity(Qt::CaseInsensitive);
}

bool MpqProxy::lessThan(const QModelIndex& l, const QModelIndex& r) const
{
    const bool ld = l.data(MpqModel::IsDirRole).toBool();
    const bool rd = r.data(MpqModel::IsDirRole).toBool();
    if (ld != rd)
        return sortOrder() == Qt::AscendingOrder ? ld : !ld;  // dirs stay on top
    const QVariant a = l.data(MpqModel::SortRole), b = r.data(MpqModel::SortRole);
    if (a.metaType().id() == QMetaType::QString) {
        QCollator c;
        c.setNumericMode(true);
        c.setCaseSensitivity(Qt::CaseInsensitive);
        return c.compare(a.toString(), b.toString()) < 0;
    }
    return a.toDouble() < b.toDouble();
}
