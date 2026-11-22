#pragma once
#include <QByteArray>
#include <QString>
#include <QVector>
#include <functional>

// Thin C++ wrapper around StormLib. All names use the MPQ convention
// (backslash-separated, case-insensitive).

struct MpqEntry {
    QString path;
    quint32 size = 0;
    quint32 compSize = 0;
    quint32 flags = 0;
    bool unnamed = false;   // StormLib invented a "File0000xxxx.ext" name
};

struct MpqInfo {
    int formatVersion = 0;      // 1..4
    quint32 maxFileCount = 0;
    quint32 fileCount = 0;
    quint32 hashTableSize = 0;
    quint32 sectorSize = 0;
    quint64 archiveSize = 0;
    quint32 signatures = 0;
    bool readOnly = false;
};

class MpqArchive {
public:
    MpqArchive() = default;
    ~MpqArchive();
    MpqArchive(const MpqArchive&) = delete;
    MpqArchive& operator=(const MpqArchive&) = delete;

    bool open(const QString& path, bool readOnly, QString* err);
    bool create(const QString& path, int version, quint32 maxFiles, QString* err);
    void close();

    bool isOpen() const { return m_h != nullptr; }
    bool isReadOnly() const { return m_readOnly; }
    const QString& path() const { return m_path; }

    bool addListFile(const QString& listfile, QString* err);
    bool loadEntries(QVector<MpqEntry>& out, QString* err);
    MpqInfo info() const;

    // Reads up to maxBytes; *truncated is set if the file is larger.
    bool readFile(const QString& name, QByteArray& out, qint64 maxBytes, bool* truncated, QString* err);
    bool extractFile(const QString& name, const QString& dest, QString* err);

    // compression: 0 = store, otherwise an MPQ_COMPRESSION_* mask
    bool addFile(const QString& src, const QString& archivedName, quint32 compression, QString* err);
    bool removeFile(const QString& name, QString* err);
    bool renameFile(const QString& from, const QString& to, QString* err);
    bool hasFile(const QString& name) const;
    bool setMaxFileCount(quint32 n, QString* err);
    bool flush(QString* err);
    bool compact(QString* err);

    static QString errorText(quint32 code);

private:
    void* m_h = nullptr;
    bool m_readOnly = false;
    QString m_path;
};
