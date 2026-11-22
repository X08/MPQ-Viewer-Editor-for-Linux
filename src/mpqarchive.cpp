#include "mpqarchive.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <cstring>

#include "StormLib.h"

static HANDLE H(void* p) { return static_cast<HANDLE>(p); }

static const char* tc(const QByteArray& b) { return b.constData(); }

QString MpqArchive::errorText(quint32 code)
{
    switch (code) {
    case ERROR_DISK_FULL:
        return QStringLiteral("Archive is full (hash table has no free entries). "
                              "Use Archive > Set Max File Count to enlarge it.");
    case ERROR_ALREADY_EXISTS: return QStringLiteral("File already exists");
    case ERROR_NOT_SUPPORTED: return QStringLiteral("Operation not supported for this archive");
    case ERROR_FILE_CORRUPT: return QStringLiteral("Archive or file is corrupt");
    case ERROR_FILE_NOT_FOUND: return QStringLiteral("File not found");
    case ERROR_ACCESS_DENIED: return QStringLiteral("Access denied (archive may be read-only)");
    case ERROR_UNKNOWN_FILE_KEY: return QStringLiteral("Cannot decrypt file (unknown key; name needed)");
    case ERROR_CHECKSUM_ERROR: return QStringLiteral("Checksum error");
    case ERROR_FILE_INCOMPLETE: return QStringLiteral("File is incomplete (patch or partial archive)");
    case ERROR_BASE_FILE_MISSING: return QStringLiteral("Base file of a patch is missing");
    case ERROR_MARKED_FOR_DELETE: return QStringLiteral("File is marked as deleted");
    case ERROR_INTERNAL_FILE: return QStringLiteral("Internal file; cannot be modified");
    default: break;
    }
    if (code < 10000)
        return QString::fromLocal8Bit(strerror(int(code))) + QStringLiteral(" (%1)").arg(code);
    return QStringLiteral("StormLib error %1").arg(code);
}

static void setErr(QString* err, const QString& what)
{
    if (err)
        *err = what + QStringLiteral(": ") + MpqArchive::errorText(SErrGetLastError());
}

MpqArchive::~MpqArchive() { close(); }

bool MpqArchive::open(const QString& path, bool readOnly, QString* err)
{
    close();
    HANDLE h = nullptr;
    // MPQ_OPEN_FORCE_LISTFILE makes sure names of files we add are persisted
    // even if the archive had no (listfile) to begin with.
    DWORD flags = readOnly ? MPQ_OPEN_READ_ONLY : MPQ_OPEN_FORCE_LISTFILE;
    QByteArray p = QFile::encodeName(path);
    if (!SFileOpenArchive(tc(p), 0, flags, &h)) {
        setErr(err, QStringLiteral("Cannot open archive"));
        return false;
    }
    m_h = h;
    m_readOnly = readOnly;
    m_path = path;
    return true;
}

bool MpqArchive::create(const QString& path, int version, quint32 maxFiles, QString* err)
{
    close();
    DWORD flags = MPQ_CREATE_LISTFILE | MPQ_CREATE_ATTRIBUTES;
    switch (version) {
    case 2: flags |= MPQ_CREATE_ARCHIVE_V2; break;
    case 3: flags |= MPQ_CREATE_ARCHIVE_V3; break;
    case 4: flags |= MPQ_CREATE_ARCHIVE_V4; break;
    default: flags |= MPQ_CREATE_ARCHIVE_V1; break;
    }
    HANDLE h = nullptr;
    QByteArray p = QFile::encodeName(path);
    if (!SFileCreateArchive(tc(p), flags, maxFiles, &h)) {
        setErr(err, QStringLiteral("Cannot create archive"));
        return false;
    }
    m_h = h;
    m_readOnly = false;
    m_path = path;
    return true;
}

void MpqArchive::close()
{
    if (m_h) {
        SFileCloseArchive(H(m_h));
        m_h = nullptr;
    }
    m_path.clear();
    m_readOnly = false;
}

bool MpqArchive::addListFile(const QString& listfile, QString* err)
{
    if (!m_h)
        return false;
    QByteArray p = QFile::encodeName(listfile);
    DWORD r = SFileAddListFile(H(m_h), tc(p));
    if (r != ERROR_SUCCESS) {
        if (err)
            *err = QStringLiteral("Cannot apply listfile: ") + errorText(r);
        return false;
    }
    return true;
}

bool MpqArchive::loadEntries(QVector<MpqEntry>& out, QString* err)
{
    out.clear();
    if (!m_h)
        return false;

    static const QRegularExpression pseudo(QStringLiteral("^File\\d{8}\\.[A-Za-z0-9_]+$"));

    SFILE_FIND_DATA fd;
    HANDLE hf = SFileFindFirstFile(H(m_h), "*", &fd, nullptr);
    if (!hf) {
        DWORD e = SErrGetLastError();
        if (e == ERROR_NO_MORE_FILES)
            return true; // empty archive
        if (err)
            *err = QStringLiteral("Cannot enumerate files: ") + errorText(e);
        return false;
    }
    out.reserve(int(info().fileCount));
    do {
        MpqEntry e;
        e.path = QString::fromUtf8(fd.cFileName);
        e.size = fd.dwFileSize;
        e.compSize = fd.dwCompSize;
        e.flags = fd.dwFileFlags;
        e.unnamed = pseudo.match(e.path).hasMatch();
        out.push_back(std::move(e));
    } while (SFileFindNextFile(hf, &fd));
    SFileFindClose(hf);
    return true;
}

MpqInfo MpqArchive::info() const
{
    MpqInfo i;
    if (!m_h)
        return i;
    DWORD v = 0, len = 0;
    ULONGLONG v64 = 0;
    TMPQHeader hdr;
    std::memset(&hdr, 0, sizeof(hdr));
    if (SFileGetFileInfo(H(m_h), SFileMpqHeader, &hdr, sizeof(hdr), &len))
        i.formatVersion = int(hdr.wFormatVersion) + 1;
    if (SFileGetFileInfo(H(m_h), SFileMpqMaxFileCount, &v, sizeof(v), &len))
        i.maxFileCount = v;
    if (SFileGetFileInfo(H(m_h), SFileMpqNumberOfFiles, &v, sizeof(v), &len))
        i.fileCount = v;
    if (SFileGetFileInfo(H(m_h), SFileMpqHashTableSize, &v, sizeof(v), &len))
        i.hashTableSize = v;
    if (SFileGetFileInfo(H(m_h), SFileMpqSectorSize, &v, sizeof(v), &len))
        i.sectorSize = v;
    if (SFileGetFileInfo(H(m_h), SFileMpqArchiveSize64, &v64, sizeof(v64), &len))
        i.archiveSize = v64;
    if (SFileGetFileInfo(H(m_h), SFileMpqSignatures, &v, sizeof(v), &len))
        i.signatures = v;
    i.readOnly = m_readOnly;
    return i;
}

bool MpqArchive::readFile(const QString& name, QByteArray& out, qint64 maxBytes, bool* truncated, QString* err)
{
    out.clear();
    if (truncated)
        *truncated = false;
    if (!m_h)
        return false;
    QByteArray n = name.toUtf8();
    HANDLE hf = nullptr;
    if (!SFileOpenFileEx(H(m_h), n.constData(), SFILE_OPEN_FROM_MPQ, &hf)) {
        setErr(err, QStringLiteral("Cannot open '%1'").arg(name));
        return false;
    }
    DWORD hi = 0;
    DWORD lo = SFileGetFileSize(hf, &hi);
    if (lo == SFILE_INVALID_SIZE) {
        setErr(err, QStringLiteral("Cannot get size of '%1'").arg(name));
        SFileCloseFile(hf);
        return false;
    }
    qint64 total = (qint64(hi) << 32) | lo;
    qint64 want = qMin(total, maxBytes);
    out.resize(int(want));
    qint64 done = 0;
    bool ok = true;
    while (done < want) {
        DWORD got = 0;
        DWORD chunk = DWORD(qMin<qint64>(want - done, 1 << 20));
        if (!SFileReadFile(hf, out.data() + done, chunk, &got, nullptr) && got == 0) {
            setErr(err, QStringLiteral("Cannot read '%1'").arg(name));
            ok = false;
            break;
        }
        if (got == 0)
            break;
        done += got;
    }
    SFileCloseFile(hf);
    if (!ok) {
        out.clear();
        return false;
    }
    out.resize(int(done));
    if (truncated)
        *truncated = total > want;
    return true;
}

bool MpqArchive::extractFile(const QString& name, const QString& dest, QString* err)
{
    if (!m_h)
        return false;
    QDir().mkpath(QFileInfo(dest).absolutePath());
    QByteArray n = name.toUtf8();
    QByteArray d = QFile::encodeName(dest);
    if (!SFileExtractFile(H(m_h), n.constData(), d.constData(), SFILE_OPEN_FROM_MPQ)) {
        setErr(err, QStringLiteral("Cannot extract '%1'").arg(name));
        return false;
    }
    return true;
}

bool MpqArchive::addFile(const QString& src, const QString& archivedName, quint32 compression, QString* err)
{
    if (!m_h)
        return false;
    QByteArray s = QFile::encodeName(src);
    QByteArray n = archivedName.toUtf8();
    DWORD flags = MPQ_FILE_REPLACEEXISTING;
    DWORD comp = 0;
    if (compression) {
        flags |= MPQ_FILE_COMPRESS;
        comp = compression;
    }
    if (!SFileAddFileEx(H(m_h), s.constData(), n.constData(), flags, comp, comp ? MPQ_COMPRESSION_NEXT_SAME : 0)) {
        setErr(err, QStringLiteral("Cannot add '%1'").arg(archivedName));
        return false;
    }
    return true;
}

bool MpqArchive::removeFile(const QString& name, QString* err)
{
    if (!m_h)
        return false;
    QByteArray n = name.toUtf8();
    if (!SFileRemoveFile(H(m_h), n.constData(), SFILE_OPEN_FROM_MPQ)) {
        setErr(err, QStringLiteral("Cannot delete '%1'").arg(name));
        return false;
    }
    return true;
}

bool MpqArchive::renameFile(const QString& from, const QString& to, QString* err)
{
    if (!m_h)
        return false;
    QByteArray a = from.toUtf8(), b = to.toUtf8();
    if (!SFileRenameFile(H(m_h), a.constData(), b.constData())) {
        setErr(err, QStringLiteral("Cannot rename '%1'").arg(from));
        return false;
    }
    return true;
}

bool MpqArchive::hasFile(const QString& name) const
{
    if (!m_h)
        return false;
    QByteArray n = name.toUtf8();
    return SFileHasFile(H(m_h), n.constData());
}

bool MpqArchive::setMaxFileCount(quint32 n, QString* err)
{
    if (!m_h)
        return false;
    if (!SFileSetMaxFileCount(H(m_h), n)) {
        setErr(err, QStringLiteral("Cannot change max file count"));
        return false;
    }
    return true;
}

bool MpqArchive::flush(QString* err)
{
    if (!m_h || m_readOnly)
        return true;
    if (!SFileFlushArchive(H(m_h))) {
        setErr(err, QStringLiteral("Cannot write archive tables"));
        return false;
    }
    return true;
}

bool MpqArchive::compact(QString* err)
{
    if (!m_h)
        return false;
    if (!SFileCompactArchive(H(m_h), nullptr, false)) {
        setErr(err, QStringLiteral("Cannot compact archive"));
        return false;
    }
    return true;
}
