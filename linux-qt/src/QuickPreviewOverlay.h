#pragma once

#include <QKeySequence>
#include <QWidget>
#include <QPointer>
#include <QFileInfo>
#include <memory>

class PreviewPane;
class QPlainTextEdit;
class QStackedWidget;
class QDirIterator;
class QTimer;

// Temporary preview; the file list keeps keyboard focus for selection navigation.
class QuickPreviewOverlay : public QWidget
{
    Q_OBJECT
public:
    explicit QuickPreviewOverlay(QWidget *parent);
    ~QuickPreviewOverlay() override;
    PreviewPane *previewPane() const { return m_preview; }
    void setNavigationView(QWidget *view) { m_navigationView = view; }
    void toggleSourceRendered();
    void openCurrentPreviewExternally();
    void setShortcut(const QKeySequence &key) { m_shortcut = key; }
    void present(const QStringList &paths);
    void previewPaths(const QStringList &paths);
    void dismiss();
signals:
    void dismissed();
    void openRequested();
protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
private:
    void readFolderBatch();
    QPointer<QWidget> m_navigationView;
    PreviewPane *m_preview;
    QPlainTextEdit *m_listing;
    QStackedWidget *m_stack;
    QTimer *m_timer;
    std::unique_ptr<QDirIterator> m_iterator;
    QList<QFileInfo> m_entries;
    QStringList m_paths;
    QKeySequence m_shortcut{Qt::Key_Space};
};
