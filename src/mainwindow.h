#pragma once
#include <QMainWindow>
#include <QSet>

#include "mpqarchive.h"
#include "mpqmodel.h"

class QAction;
class QActionGroup;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QScrollArea;
class QStackedWidget;
class QTreeView;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    bool openArchive(const QString& path, bool readOnly = false);

protected:
    void dragEnterEvent(QDragEnterEvent* e) override;
    void dropEvent(QDropEvent* e) override;
    void closeEvent(QCloseEvent* e) override;

private:
    void buildUi();
    void buildMenus();

    // actions
    void newArchive();
    void openDialog(bool readOnly);
    bool closeArchive();
    void loadListfile();
    void forgetListfile();
    void extractSelected();
    void extractAll();
    void addFilesDialog();
    void addFolderDialog();
    void deleteSelected();
    void renameSelected();
    void compactArchive();
    void setMaxFiles();
    void showInfo();
    void about();
    void showContextMenu(const QPoint& pos);

    // helpers
    void reload();
    void updateUi();
    void updateStatus();
    void onSelectionChanged();
    QModelIndexList selectedSourceIndexes() const;
    QStringList selectedFiles() const;
    QString targetPrefix() const;
    bool extractFiles(const QStringList& files, const QString& destDir);
    void addPaths(const QStringList& localPaths, const QString& prefix);
    bool finishEdit(const QString& what);
    quint32 compressionMask() const;
    bool requireWritable();
    void showPreview();
    void renderPreview();
    void clearPreview(const QString& message);
    QSet<QString> expandedPaths() const;
    void restoreExpanded(const QSet<QString>& paths);
    static QString safeRelPath(const QString& mpqPath);

    MpqArchive m_ar;
    MpqModel* m_model;
    MpqProxy* m_proxy;
    QTreeView* m_tree;
    QLineEdit* m_filter;

    QStackedWidget* m_stack;
    QLabel* m_placeholder;
    QPlainTextEdit* m_text;
    QScrollArea* m_imageScroll;
    QLabel* m_image;
    QLabel* m_previewTitle;
    QComboBox* m_previewMode;

    QLabel* m_statusLeft;
    QLabel* m_statusRight;

    QString m_listfile;
    QString m_previewName;
    QByteArray m_previewData;
    bool m_previewTruncated = false;
    qint64 m_previewTotal = 0;

    QAction *aNew, *aOpen, *aOpenRO, *aClose, *aQuit;
    QAction *aListfile, *aForget, *aExtract, *aExtractAll;
    QAction *aAddFiles, *aAddFolder, *aDelete, *aRename;
    QAction *aCompact, *aMaxFiles, *aInfo;
    QActionGroup* m_compGroup;
};
