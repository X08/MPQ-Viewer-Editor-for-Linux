#pragma once
#include <QAbstractItemModel>
#include <QHash>
#include <QSortFilterProxyModel>
#include <memory>
#include <vector>

#include "mpqarchive.h"

struct MpqNode {
    QString name;
    QString path;            // full MPQ path (backslashes); for dirs, without trailing slash
    bool isDir = false;
    bool unnamed = false;
    MpqNode* parent = nullptr;
    int row = 0;
    quint64 size = 0;        // for dirs: sum of children
    quint64 comp = 0;
    quint32 flags = 0;
    quint32 fileCount = 0;   // for dirs: number of files below
    std::vector<std::unique_ptr<MpqNode>> kids;
    QHash<QString, MpqNode*> lookup;  // lowercase name -> child (build-time helper)
};

class MpqModel : public QAbstractItemModel {
    Q_OBJECT
public:
    enum Column { ColName, ColSize, ColPacked, ColRatio, ColFlags, ColCount };
    enum Role { PathRole = Qt::UserRole + 1, IsDirRole, SortRole, UnnamedRole };

    explicit MpqModel(QObject* parent = nullptr);

    void setEntries(const QVector<MpqEntry>& entries);
    void clear();

    MpqNode* nodeFor(const QModelIndex& idx) const;
    QModelIndex indexForPath(const QString& path) const;  // exact, case-insensitive

    // All file paths at or below the node
    QStringList filesUnder(const QModelIndex& idx) const;

    QModelIndex index(int row, int col, const QModelIndex& parent = {}) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& = {}) const override { return ColCount; }
    QVariant data(const QModelIndex& idx, int role) const override;
    QVariant headerData(int section, Qt::Orientation o, int role) const override;

    static QString flagsText(quint32 f);
    static QString sizeText(quint64 b);

private:
    void collect(const MpqNode* n, QStringList& out) const;
    void finalize(MpqNode* n);
    std::unique_ptr<MpqNode> m_root;
};

// Directories first, then by the column's natural order; recursive name filter.
class MpqProxy : public QSortFilterProxyModel {
    Q_OBJECT
public:
    explicit MpqProxy(QObject* parent = nullptr);
protected:
    bool lessThan(const QModelIndex& l, const QModelIndex& r) const override;
};
