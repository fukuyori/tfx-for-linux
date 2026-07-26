#include "FilePane.h"
#include "FilePaneClipboard.h"
#include "UiText.h"
#include "core/FileTypeInfo.h"
#include "platform/Platform.h"

#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QPointer>
#include <QProcess>
#include <QPushButton>
#include <QThread>
#include <QTimer>

#include <atomic>
#include <memory>

using namespace tfx::platform;

void FilePane::showFileContextMenu(const QPoint &point)
{
    const QModelIndex clicked = m_view->indexAt(point);
    // Row-based hit test: the clicked cell may be a synthesised column that
    // dropped out of the stored selection, and selectRow() would collapse the
    // multi-selection.
    if (clicked.isValid()
        && !m_view->selectionModel()->rowIntersectsSelection(clicked.row(), clicked.parent())) {
        m_view->selectRow(clicked.row());
    }

    const QFileInfo info = currentFileInfo();
    const bool hasSelection = info.exists();
    const bool isDirectory = hasSelection && info.isDir();
    const bool isZip = hasSelection && info.suffix().compare("zip", Qt::CaseInsensitive) == 0;

    QMenu menu(this);

    // Group 1 — Open
    menu.addAction(UiText::t("Open", "開く"), this, &FilePane::openSelected)->setEnabled(hasSelection);
    if (hasSelection && !isDirectory) {
        auto *openWith = menu.addMenu(UiText::t("Open With", "このアプリケーションで開く"));
        openWith->addAction(UiText::t("Default Application", "既定のアプリケーション"), this, &FilePane::openSelected);
        const QString suffix = info.suffix().toLower();
        QStringList configuredPrograms;
        if (!suffix.isEmpty() && m_openWithApplications.contains(suffix)) {
            configuredPrograms << m_openWithApplications.value(suffix);
        }
        if (m_openWithApplications.contains("*")) {
            configuredPrograms << m_openWithApplications.value("*");
        }
        configuredPrograms.removeDuplicates();
        for (const QString &program : configuredPrograms) {
            if (program.trimmed().isEmpty()) {
                continue;
            }
            openWith->addAction(program, this, [this, program]() {
                openWithConfiguredApplication(program);
            });
        }
        if (!configuredPrograms.isEmpty()) {
            openWith->addSeparator();
        }
        openWith->addAction(UiText::t("Other...", "その他..."), this, &FilePane::openWithCustomApplication);
    }

    // Group 2 — User commands (flat), right after Open
    addUserCommandActions(&menu, hasSelection);

    // Group 3 — Edit / manipulate
    menu.addSeparator();
    menu.addAction(UiText::t("Rename", "名前を変更"), this, &FilePane::renameSelected)->setEnabled(hasSelection);
    menu.addAction(UiText::t("Create Link", "リンクを作成"), this, &FilePane::createLinkForSelection)->setEnabled(hasSelection);
    menu.addAction(UiText::t("Compress to Zip", "zip に圧縮"), this, &FilePane::compressSelectedItemsToZip)->setEnabled(hasSelection);
    if (isZip) {
        menu.addAction(UiText::t("Extract Zip", "zip を展開"), this, &FilePane::extractSelectedZip);
    }

    // Group 3 — Clipboard
    menu.addSeparator();
    menu.addAction(UiText::t("Copy Items", "項目をコピー"), this, &FilePane::copySelected)->setEnabled(hasSelection);
    menu.addAction(UiText::t("Cut Items", "項目をカット"), this, &FilePane::cutSelected)->setEnabled(hasSelection);
    menu.addAction(UiText::t("Paste Here", "ここにペースト"), this, &FilePane::pasteIntoCurrentDirectory)
        ->setEnabled(tfx::filepane::clipboardCanPaste(QApplication::clipboard()->mimeData()));
    menu.addAction(UiText::t("Paste as Plain Text", "プレーンテキストとしてペースト"), this, &FilePane::pasteClipboardAsPlainText)
        ->setEnabled(QApplication::clipboard()->mimeData()->hasText());

    // Group 4 — Tags
    menu.addSeparator();
    auto *tagsMenu = menu.addMenu(UiText::t("Tags", "タグ"));
    tagsMenu->addAction(UiText::t("Add Custom Tag...", "カスタムタグを追加..."))->setEnabled(false);
    tagsMenu->addSeparator();
    tagsMenu->addAction("Red")->setEnabled(false);
    tagsMenu->addAction("Orange")->setEnabled(false);
    tagsMenu->addAction("Yellow")->setEnabled(false);
    tagsMenu->addAction("Green")->setEnabled(false);
    tagsMenu->addAction("Blue")->setEnabled(false);
    tagsMenu->addAction("Purple")->setEnabled(false);
    tagsMenu->addAction("Gray")->setEnabled(false);

    // Group 5 — Location
    menu.addSeparator();
    menu.addAction(UiText::t("Reveal in File Manager", "ファイルマネージャで表示"), this, &FilePane::revealSelectionInFileManager)->setEnabled(hasSelection);
    menu.addAction(UiText::t("Copy Path", "パスをコピー"), this, &FilePane::copySelectedPaths)->setEnabled(hasSelection);

    // Group 6 — Folder actions
    if (isDirectory) {
        menu.addSeparator();
        menu.addAction(UiText::t("Pin Folder", "フォルダをピン留め"), this, [this, info]() {
            emit pinFolderRequested(info.absoluteFilePath());
        });
        menu.addAction(UiText::t("Open Terminal Here", "ここでターミナルを開く"), this, &FilePane::openTerminalHere);
    }

    // Group 7 — Destructive
    menu.addSeparator();
    menu.addAction(UiText::t("Move to Trash", "ゴミ箱へ移動"), this, &FilePane::moveSelectedToTrash)->setEnabled(hasSelection);

    // Group 8 — Properties (single selection only, not inside archives)
    menu.addSeparator();
    menu.addAction(UiText::t("Properties", "プロパティ"), this, &FilePane::showProperties)
        ->setEnabled(m_zipPath.isEmpty() && selectedLocalPaths().size() <= 1);

    menu.exec(m_view->viewport()->mapToGlobal(point));
}

void FilePane::showEmptyAreaContextMenu(const QPoint &point)
{
    QMenu menu(this);
    menu.addAction(UiText::t("New Folder", "新規フォルダ"), this, &FilePane::createFolder);
    menu.addAction(UiText::t("New File", "新規ファイル"), this, &FilePane::createFile);
    menu.addSeparator();
    menu.addAction(UiText::t("Paste Here", "ここにペースト"), this, &FilePane::pasteIntoCurrentDirectory)
        ->setEnabled(tfx::filepane::clipboardCanPaste(QApplication::clipboard()->mimeData()));
    menu.addAction(UiText::t("Paste as Plain Text", "プレーンテキストとしてペースト"), this, &FilePane::pasteClipboardAsPlainText)
        ->setEnabled(QApplication::clipboard()->mimeData()->hasText());
    menu.addSeparator();
    menu.addAction(UiText::t("Select All", "すべて選択"), this, &FilePane::selectAllVisibleItems)
        ->setEnabled(m_proxyModel->rowCount(m_view->rootIndex()) > 0);
    menu.addSeparator();
    menu.addAction(UiText::t("Reveal in File Manager", "ファイルマネージャで表示"), this, [this]() {
        revealInFileManager(m_currentPath);
    });
    menu.addAction(UiText::t("Copy Current Path", "現在のパスをコピー"), this, [this]() {
        QApplication::clipboard()->setText(m_currentPath);
    });
    menu.addSeparator();
    menu.addAction(UiText::t("Pin Folder", "フォルダをピン留め"), this, [this]() {
        emit pinFolderRequested(m_currentPath);
    });
    menu.addAction(UiText::t("Open Terminal Here", "ここでターミナルを開く"), this, [this]() {
        emit openTerminalHereRequested(m_currentPath);
    });
    menu.addSeparator();
    menu.addAction(UiText::t("Properties", "プロパティ"), this, &FilePane::showProperties)
        ->setEnabled(m_zipPath.isEmpty());

    addUserCommandActions(&menu, false);

    menu.exec(m_view->viewport()->mapToGlobal(point));
}

void FilePane::revealSelectionInFileManager()
{
    const QFileInfo info = currentFileInfo();
    if (!info.exists()) {
        return;
    }
    revealInFileManager(info.absoluteFilePath());
}

void FilePane::createLinkForSelection()
{
    const QFileInfo info = currentFileInfo();
    if (!info.exists()) {
        return;
    }
    const QString linkPath = uniqueChildPath(info.fileName() + placeholderText(" link", " のリンク"));
    if (QFile::link(info.absoluteFilePath(), linkPath)) {
        QTimer::singleShot(0, this, [this, linkPath]() {
            setCurrentIndexForPath(linkPath);
        });
    } else {
        emit statusMessageRequested(UiText::t("Could not create link.", "リンクを作成できませんでした。"));
    }
}

void FilePane::openWithConfiguredApplication(const QString &program)
{
    const QFileInfo info = currentFileInfo();
    if (!info.exists() || info.isDir() || program.trimmed().isEmpty()) {
        return;
    }
    if (!QProcess::startDetached(program, {info.absoluteFilePath()})) {
        emit statusMessageRequested(UiText::t("Could not launch: %1", "起動できませんでした: %1").arg(program));
    }
}

void FilePane::openWithCustomApplication()
{
    const QFileInfo info = currentFileInfo();
    if (!info.exists()) {
        return;
    }

    // Prefer the desktop's native "Open With" application chooser.
    if (openWithChooser(info.absoluteFilePath())) {
        return;
    }

    // Fallback: ask for a launch command when no native chooser is available.
    bool ok = false;
    const QString command = QInputDialog::getText(this,
        UiText::t("Open With", "このアプリケーションで開く"),
        UiText::t("Application command:", "アプリケーションのコマンド:"),
        QLineEdit::Normal, QString(), &ok).trimmed();
    if (!ok || command.isEmpty()) {
        return;
    }
    if (!QProcess::startDetached(command, {info.absoluteFilePath()})) {
        emit statusMessageRequested(UiText::t("Could not launch: %1", "起動できませんでした: %1").arg(command));
    }
}

bool FilePane::launchExecutable(const QFileInfo &info)
{
    const bool started = QProcess::startDetached(
        info.absoluteFilePath(), {}, info.absolutePath());
    if (!started) {
        emit statusMessageRequested(
            UiText::t("Could not run: %1", "実行できませんでした: %1").arg(info.fileName()));
    }
    return started;
}

bool FilePane::tryRunExecutable(const QFileInfo &info)
{
    if (!info.isFile() || !info.isExecutable()) {
        return false;
    }

    // Peek at the leading bytes to decide binary vs. text script.
    QFile file(info.absoluteFilePath());
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    const QByteArray head = file.read(512);
    file.close();
    if (head.isEmpty()) {
        return false;
    }

    const bool isElf = head.startsWith("\x7f""ELF");
    const bool hasNullByte = head.contains('\0');
    // Treat ELF and any other binary blob as a program to run directly.
    if (isElf || hasNullByte) {
        launchExecutable(info);
        return true;
    }

    // Otherwise it is an executable text file (script): let the user choose.
    QMessageBox box(this);
    box.setWindowTitle(UiText::t("Executable Script", "実行可能なスクリプト"));
    box.setIcon(QMessageBox::Question);
    box.setText(UiText::t("\"%1\" is an executable script.", "\"%1\" は実行可能なスクリプトです。")
                    .arg(info.fileName()));
    box.setInformativeText(
        UiText::t("Do you want to run it, or open it for editing?",
                  "実行しますか、それとも編集用に開きますか？"));
    QPushButton *runButton =
        box.addButton(UiText::t("Run", "実行"), QMessageBox::AcceptRole);
    QPushButton *editButton =
        box.addButton(UiText::t("Edit", "編集"), QMessageBox::RejectRole);
    box.addButton(QMessageBox::Cancel);
    box.setDefaultButton(editButton);
    box.exec();

    QAbstractButton *clicked = box.clickedButton();
    if (clicked == runButton) {
        launchExecutable(info);
        return true;
    }
    if (clicked == editButton) {
        tfx::platform::openPath(info.absoluteFilePath());
        return true;
    }
    // Cancel: consume the open action without doing anything.
    return true;
}

void FilePane::openTerminalHere()
{
    const QFileInfo info = currentFileInfo();
    if (!info.exists()) {
        return;
    }
    emit openTerminalHereRequested(info.isDir() ? info.absoluteFilePath() : info.absolutePath());
}

void FilePane::selectAllVisibleItems()
{
    m_view->selectAll();
    updateStatusLine();
}

// Properties dialog (Windows "Properties" / macOS "Get Info"): metadata for
// the selected item, or for the current folder when nothing is selected.
// Folder sizes are tallied on a worker thread so a large or slow tree never
// blocks the UI; the tally stops when the dialog closes.
void FilePane::showProperties()
{
    if (!m_zipPath.isEmpty()) {
        emit statusMessageRequested(UiText::t("Properties are unavailable inside archives.",
                                              "アーカイブ内ではプロパティを表示できません。"));
        return;
    }
    if (selectedLocalPaths().size() > 1) {
        emit statusMessageRequested(UiText::t("Select a single item to show properties.",
                                              "プロパティは1項目のみ表示できます。"));
        return;
    }
    const QFileInfo current = currentFileInfo();
    const QFileInfo info = current.exists() || current.isSymLink()
        ? current
        : QFileInfo(m_currentPath);

    auto *dialog = new QDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(UiText::t("Properties", "プロパティ"));
    dialog->setMinimumWidth(420);

    auto *layout = new QFormLayout(dialog);
    layout->setLabelAlignment(Qt::AlignRight);
    const auto addRow = [dialog, layout](const QString &label, const QString &value) {
        auto *field = new QLabel(value, dialog);
        field->setTextInteractionFlags(Qt::TextSelectableByMouse);
        field->setWordWrap(true);
        layout->addRow(label, field);
        return field;
    };

    const QLocale locale;
    addRow(UiText::t("Name", "名前"),
           info.fileName().isEmpty() ? info.absoluteFilePath() : info.fileName());
    addRow(UiText::t("Type", "種類"), tfx::core::englishTypeName(info));
    addRow(UiText::t("Location", "場所"), info.absolutePath());
    if (info.isSymLink()) {
        addRow(UiText::t("Link Target", "リンク先"), info.symLinkTarget());
    }

    QLabel *sizeField = addRow(UiText::t("Size", "サイズ"), QString());
    if (info.isDir() && !info.isSymLink()) {
        sizeField->setText(UiText::t("Calculating...", "計算中..."));
        auto cancelled = std::make_shared<std::atomic_bool>(false);
        QObject::connect(dialog, &QObject::destroyed, [cancelled]() { *cancelled = true; });
        const QString root = info.absoluteFilePath();
        QPointer<QLabel> field(sizeField);
        QThread *worker = QThread::create([root, cancelled, field]() {
            qint64 bytes = 0;
            qint64 files = 0;
            qint64 dirs = 0;
            QDirIterator it(root,
                            QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot,
                            QDirIterator::Subdirectories);
            while (it.hasNext() && !*cancelled) {
                it.next();
                const QFileInfo entry = it.fileInfo();
                if (entry.isDir()) {
                    ++dirs;
                } else {
                    ++files;
                    bytes += entry.size();
                }
            }
            if (*cancelled) {
                return;
            }
            const QString text = UiText::t("%1 (%2 files, %3 folders)",
                                           "%1(ファイル %2、フォルダ %3)")
                                     .arg(tfx::core::sizeString(bytes))
                                     .arg(files)
                                     .arg(dirs);
            QMetaObject::invokeMethod(qApp, [field, text]() {
                if (field) {
                    field->setText(text);
                }
            }, Qt::QueuedConnection);
        });
        QObject::connect(worker, &QThread::finished, worker, &QObject::deleteLater);
        worker->start();
    } else {
        sizeField->setText(QString("%1 (%2)")
                               .arg(tfx::core::sizeString(info.size()),
                                    UiText::t("%1 bytes", "%1 バイト").arg(locale.toString(info.size()))));
    }

    const QFile::Permissions permissions = info.permissions();
    const auto octalDigit = [permissions](QFile::Permission read, QFile::Permission write,
                                          QFile::Permission execute) {
        return (permissions.testFlag(read) ? 4 : 0) + (permissions.testFlag(write) ? 2 : 0)
            + (permissions.testFlag(execute) ? 1 : 0);
    };
    const QString octal = QString::number(octalDigit(QFile::ReadOwner, QFile::WriteOwner, QFile::ExeOwner) * 100
                                          + octalDigit(QFile::ReadGroup, QFile::WriteGroup, QFile::ExeGroup) * 10
                                          + octalDigit(QFile::ReadOther, QFile::WriteOther, QFile::ExeOther));
    addRow(UiText::t("Permissions", "パーミッション"),
           QString("%1 (%2)").arg(tfx::core::modeString(info), octal));
    addRow(UiText::t("Owner", "所有者"), QString("%1 / %2").arg(info.owner(), info.group()));
    if (info.birthTime().isValid()) {
        addRow(UiText::t("Created", "作成日時"), locale.toString(info.birthTime(), QLocale::ShortFormat));
    }
    addRow(UiText::t("Modified", "更新日時"), locale.toString(info.lastModified(), QLocale::ShortFormat));
    addRow(UiText::t("Accessed", "アクセス日時"), locale.toString(info.lastRead(), QLocale::ShortFormat));

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
    QObject::connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    QObject::connect(buttons, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
    layout->addRow(buttons);
    dialog->show();
}
