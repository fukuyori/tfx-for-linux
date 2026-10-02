#include "QuickPreviewOverlay.h"
#include "PreviewPane.h"
#include "UiText.h"

#include <QApplication>
#include <QCollator>
#include <QDirIterator>
#include <QKeyEvent>
#include <QPlainTextEdit>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>

QuickPreviewOverlay::QuickPreviewOverlay(QWidget *parent)
    : QWidget(parent), m_preview(new PreviewPane(this)),
      m_listing(new QPlainTextEdit(this)), m_stack(new QStackedWidget(this)),
      m_timer(new QTimer(this))
{
    setObjectName("quickPreviewOverlay");
    setAutoFillBackground(true);
    setAttribute(Qt::WA_StyledBackground, true);
    m_preview->setShowsFileInfo(false);
    m_listing->setObjectName("previewCode");
    m_listing->setReadOnly(true);
    m_listing->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_stack->addWidget(m_preview);
    m_stack->addWidget(m_listing);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(0);
    layout->addWidget(m_stack, 1);
    connect(m_timer, &QTimer::timeout, this, &QuickPreviewOverlay::readFolderBatch);
    qApp->installEventFilter(this);
    hide();
}

QuickPreviewOverlay::~QuickPreviewOverlay() = default;

void QuickPreviewOverlay::present(const QStringList &paths)
{
    if (paths.isEmpty()) return;
    setGeometry(parentWidget()->rect().adjusted(10, 20, -10, -10));
    show();
    raise();
    previewPaths(paths);
}

void QuickPreviewOverlay::toggleSourceRendered()
{
    if (m_stack->currentWidget() == m_preview) m_preview->toggleSourceRendered();
}

void QuickPreviewOverlay::openCurrentPreviewExternally()
{
    if (m_stack->currentWidget() == m_preview) m_preview->openCurrentPreviewExternally();
}

void QuickPreviewOverlay::dismiss()
{
    if (isHidden()) return;
    m_timer->stop();
    m_iterator.reset();
    m_entries.clear();
    m_paths.clear();
    hide();
    emit dismissed();
}

bool QuickPreviewOverlay::eventFilter(QObject *watched, QEvent *event)
{
    if (!isVisible()) return QWidget::eventFilter(watched, event);
    if (watched == parentWidget() && event->type() == QEvent::Resize)
        setGeometry(parentWidget()->rect().adjusted(10, 20, -10, -10));
    auto *widget = qobject_cast<QWidget *>(watched);
    const QWidget *focus = QApplication::focusWidget();
    // Inline editors, search fields and terminals must keep their own keys,
    // even when ignored key events bubble up to a file view or the window.
    const bool previewFocus = focus && (focus == m_navigationView
        || focus == this || isAncestorOf(focus));
    if (widget && widget->window() == window() && previewFocus
        && !QApplication::activeModalWidget() && !QApplication::activePopupWidget()
        && (event->type() == QEvent::ShortcutOverride || event->type() == QEvent::KeyPress)) {
        auto *key = static_cast<QKeyEvent *>(event);
        const bool close = key->key() == Qt::Key_Escape
            || QKeySequence(key->keyCombination()) == m_shortcut;
        const bool open = (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter)
            && key->modifiers() == Qt::NoModifier;
        if (close || open) {
            event->accept();
            if (event->type() == QEvent::KeyPress && !key->isAutoRepeat()) {
                dismiss();
                if (open) emit openRequested();
            }
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void QuickPreviewOverlay::previewPaths(const QStringList &paths)
{
    if (paths == m_paths) return;
    m_paths = paths;
    m_timer->stop();
    m_iterator.reset();
    m_entries.clear();
    if (paths.isEmpty()) {
        m_stack->setCurrentWidget(m_listing);
        m_listing->setPlainText(UiText::t("Nothing selected.", "選択なし"));
        return;
    }
    if (paths.size() == 1 && !QFileInfo(paths.first()).isDir()) {
        m_stack->setCurrentWidget(m_preview);
        m_preview->previewPath(paths.first());
        return;
    }
    m_stack->setCurrentWidget(m_listing);
    if (paths.size() > 1) {
        QStringList names;
        for (const auto &path : paths) {
            const QFileInfo info(path);
            names << info.fileName() + (info.isDir() ? "/" : "");
        }
        m_listing->setPlainText(names.join('\n'));
        return;
    }
    const QDir dir(paths.first());
    if (!dir.isReadable()) {
        m_listing->setPlainText(UiText::t("Cannot read folder.", "フォルダを読み取れません。"));
        return;
    }
    m_listing->setPlainText(UiText::t("Loading…", "読み込み中…"));
    // Bounded batches keep navigation and dismissal responsive. Do not follow
    // directory symlinks or include hidden entries, matching the Swift preview.
    m_iterator = std::make_unique<QDirIterator>(paths.first(),
        QDir::AllEntries | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    m_timer->start(0);
}

void QuickPreviewOverlay::readFolderBatch()
{
    for (int i = 0; i < 100 && m_entries.size() < 5000 && m_iterator->hasNext(); ++i) {
        m_iterator->next();
        m_entries << m_iterator->fileInfo();
    }
    if (m_entries.size() < 5000 && m_iterator->hasNext()) return;
    const bool truncated = m_entries.size() > 500 || m_iterator->hasNext();
    m_timer->stop();
    m_iterator.reset();
    QCollator collator;
    collator.setNumericMode(true);
    std::sort(m_entries.begin(), m_entries.end(), [&collator](const auto &a, const auto &b) {
        if (a.isDir() != b.isDir()) return a.isDir();
        return collator.compare(a.fileName(), b.fileName()) < 0;
    });
    QStringList names;
    if (truncated) names << UiText::t("… first 500 entries shown", "…先頭の500件を表示");
    for (int i = 0; i < qMin(500, m_entries.size()); ++i) {
        const auto &info = m_entries.at(i);
        names << info.fileName() + (info.isDir() ? "/" : "");
    }
    m_listing->setPlainText(names.isEmpty()
        ? UiText::t("(empty folder)", "(空のフォルダ)") : names.join('\n'));
    m_entries.clear();
}
