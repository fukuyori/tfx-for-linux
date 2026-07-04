#include "FilePane.h"
#include "FilePaneClipboard.h"
#include "UiText.h"
#include "core/FileOperations.h"

#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QUrl>

using namespace tfx::core;
using namespace tfx::filepane;

namespace {

enum class ConflictChoice {
    Overwrite,
    Skip,
    Rename,
    Cancel,
};

bool pathOccupied(const QString &path)
{
    const QFileInfo info(path);
    return info.isSymLink() || info.exists();
}

ConflictChoice askConflict(QWidget *parent, const QString &fileName)
{
    QMessageBox box(parent);
    box.setWindowTitle(UiText::t("Name Conflict", "名前の衝突"));
    box.setText(UiText::t("An item named \"%1\" already exists.", "\"%1\" はすでに存在します。").arg(fileName));
    auto *overwrite = box.addButton(UiText::t("Overwrite", "上書き"), QMessageBox::DestructiveRole);
    auto *skip = box.addButton(UiText::t("Skip", "スキップ"), QMessageBox::RejectRole);
    auto *rename = box.addButton(UiText::t("Rename", "名前を変更"), QMessageBox::AcceptRole);
    auto *cancel = box.addButton(QMessageBox::Cancel);
    box.setDefaultButton(rename);
    box.exec();

    if (box.clickedButton() == overwrite) return ConflictChoice::Overwrite;
    if (box.clickedButton() == rename) return ConflictChoice::Rename;
    if (box.clickedButton() == cancel) return ConflictChoice::Cancel;
    if (box.clickedButton() == skip) return ConflictChoice::Skip;
    return ConflictChoice::Cancel;
}

bool looksLikeDelimitedText(const QString &text, QChar separator)
{
    const QStringList lines = text.split('\n', Qt::SkipEmptyParts);
    if (lines.size() < 2) {
        return false;
    }
    int expected = -1;
    for (QString line : lines) {
        line = line.trimmed();
        if (line.isEmpty()) {
            continue;
        }
        const int count = line.count(separator);
        if (count <= 0) {
            return false;
        }
        if (expected < 0) {
            expected = count;
        } else if (count != expected) {
            return false;
        }
    }
    return expected > 0;
}

QString plainTextClipboardBaseName(const QString &text, const QString &language)
{
    const QString trimmed = text.trimmed();
    const QUrl url(trimmed);
    if (url.isValid() && !url.scheme().isEmpty()
        && (url.scheme() == "http" || url.scheme() == "https" || url.scheme() == "ftp")) {
        return placeholderName(language, "Clipboard URL.url", "クリップボード URL.url");
    }
    if (looksLikeDelimitedText(text, '\t')) {
        return placeholderName(language, "Clipboard.tsv", "クリップボード.tsv");
    }
    if (looksLikeDelimitedText(text, ',')) {
        return placeholderName(language, "Clipboard.csv", "クリップボード.csv");
    }
    return placeholderName(language, "Clipboard.txt", "クリップボード.txt");
}

bool writeBytesToFile(const QString &path, const QByteArray &data)
{
    QFile file(path);
    if (!file.open(QIODevice::NewOnly | QIODevice::WriteOnly)) {
        return false;
    }
    return file.write(data) == data.size();
}

}

bool tfx::filepane::clipboardCanPaste(const QMimeData *mime)
{
    return mime
        && (mime->hasUrls()
            || mime->hasImage()
            || mime->hasHtml()
            || mime->hasText()
            || mime->hasFormat("text/rtf"));
}

QString tfx::filepane::placeholderName(const QString &language, const QString &english, const QString &japanese)
{
    if (language == "en") {
        return english;
    }
    if (language == "ja") {
        return japanese;
    }
    return UiText::isJapanese() ? japanese : english;
}

void FilePane::copySelected()
{
    auto *mime = new QMimeData();
    mime->setUrls(selectedUrls());
    QApplication::clipboard()->setMimeData(mime);
    emit statusMessageRequested(UiText::t("Copied selected item(s).", "選択項目をコピーしました。"));
}

void FilePane::cutSelected()
{
    auto *mime = new QMimeData();
    mime->setUrls(selectedUrls());
    mime->setData("application/x-tfx-cut", "1");
    QApplication::clipboard()->setMimeData(mime);
    emit statusMessageRequested(UiText::t("Cut selected item(s).", "選択項目をカットしました。"));
}

void FilePane::performDrop(const QList<QUrl> &urls, Qt::DropAction action, const QString &targetDir)
{
    if (urls.isEmpty() || targetDir.isEmpty()) {
        return;
    }
    const bool move = (action == Qt::MoveAction);
    const QDir dir(targetDir);
    const QString targetCanonical = QFileInfo(targetDir).absoluteFilePath();
    QVector<FileOperationRequest> requests;

    for (const QUrl &url : urls) {
        const QString source = url.toLocalFile();
        if (source.isEmpty()) {
            continue;
        }
        const QFileInfo sourceInfo(source);
        if (!sourceInfo.isSymLink() && !sourceInfo.exists()) {
            continue;
        }
        if (move && sourceInfo.absolutePath() == targetCanonical) {
            continue;
        }
        if (transferWouldNestInsideSource(source, targetDir)) {
            emit statusMessageRequested(UiText::t("Cannot transfer a folder into itself.", "フォルダを自身の中へは移動/コピーできません。"));
            continue;
        }
        QString destination = dir.filePath(sourceInfo.fileName());
        const bool samePath = QFileInfo(destination).absoluteFilePath() == sourceInfo.absoluteFilePath();
        if (samePath && move) {
            continue;
        }
        bool overwrite = false;
        if (samePath && !move) {
            destination = uniquePathInDirectory(targetDir, sourceInfo.fileName());
        } else if (pathOccupied(destination)) {
            const ConflictChoice choice = askConflict(this, sourceInfo.fileName());
            if (choice == ConflictChoice::Cancel) {
                break;
            }
            if (choice == ConflictChoice::Skip) {
                continue;
            }
            if (choice == ConflictChoice::Rename) {
                destination = uniquePathInDirectory(targetDir, sourceInfo.fileName());
            } else {
                overwrite = true;
            }
        }

        requests.append({source, destination, move, overwrite});
    }

    if (!requests.isEmpty()) {
        emit fileOperationRequested(requests);
    }
    updateStatusLine();
}

void FilePane::pasteIntoCurrentDirectory()
{
    const QMimeData *mime = QApplication::clipboard()->mimeData();
    if (!clipboardCanPaste(mime)) {
        return;
    }

    bool sawLocalUrl = false;
    const bool move = mime->hasFormat("application/x-tfx-cut");
    if (mime->hasUrls()) {
        QVector<FileOperationRequest> requests;
        for (const QUrl &url : mime->urls()) {
            const QString source = url.toLocalFile();
            if (source.isEmpty()) {
                continue;
            }
            sawLocalUrl = true;
            const QFileInfo sourceInfo(source);
            if (!sourceInfo.isSymLink() && !sourceInfo.exists()) {
                continue;
            }
            if (transferWouldNestInsideSource(source, m_currentPath)) {
                emit statusMessageRequested(UiText::t("Cannot transfer a folder into itself.", "フォルダを自身の中へは移動/コピーできません。"));
                continue;
            }

            QString destination = QDir(m_currentPath).filePath(sourceInfo.fileName());
            const bool samePath = QFileInfo(destination).absoluteFilePath() == sourceInfo.absoluteFilePath();
            if (samePath && move) {
                continue;
            }
            bool overwrite = false;
            if (samePath && !move) {
                destination = uniquePathInDirectory(m_currentPath, sourceInfo.fileName());
            } else if (pathOccupied(destination)) {
                const ConflictChoice choice = askConflict(this, sourceInfo.fileName());
                if (choice == ConflictChoice::Cancel) {
                    break;
                }
                if (choice == ConflictChoice::Skip) {
                    continue;
                }
                if (choice == ConflictChoice::Rename) {
                    destination = uniquePathInDirectory(m_currentPath, sourceInfo.fileName());
                } else {
                    overwrite = true;
                }
            }

            requests.append({source, destination, move, overwrite});
        }
        if (!requests.isEmpty()) {
            emit fileOperationRequested(requests);
        }
        updateStatusLine();
        if (sawLocalUrl) {
            return;
        }
    }

    pasteClipboardAsFile(false);
}

void FilePane::pasteClipboardAsPlainText()
{
    pasteClipboardAsFile(true);
}

bool FilePane::pasteClipboardAsFile(bool plainTextOnly)
{
    const QMimeData *mime = QApplication::clipboard()->mimeData();
    if (!mime) {
        return false;
    }

    QString baseName;
    QByteArray data;
    bool image = false;
    QImage clipboardImage;

    if (!plainTextOnly && mime->hasImage()) {
        clipboardImage = qvariant_cast<QImage>(mime->imageData());
        baseName = placeholderText("Clipboard Image.png", "クリップボード画像.png");
        image = !clipboardImage.isNull();
    } else if (!plainTextOnly && mime->hasFormat("text/rtf")) {
        baseName = placeholderText("Clipboard.rtf", "クリップボード.rtf");
        data = mime->data("text/rtf");
    } else if (!plainTextOnly && mime->hasHtml()) {
        baseName = placeholderText("Clipboard.html", "クリップボード.html");
        data = mime->html().toUtf8();
    } else if (mime->hasText()) {
        baseName = plainTextOnly
            ? placeholderText("Clipboard.txt", "クリップボード.txt")
            : plainTextClipboardBaseName(mime->text(), m_placeholderLanguage);
        data = mime->text().toUtf8();
    } else if (!plainTextOnly && mime->hasUrls() && !mime->urls().isEmpty()) {
        baseName = placeholderText("Clipboard URL.url", "クリップボード URL.url");
        data = mime->urls().first().toString().toUtf8();
    } else {
        return false;
    }

    if (baseName.isEmpty() || (!image && data.isEmpty())) {
        return false;
    }

    const QString path = uniquePathInDirectory(m_currentPath, baseName);
    const bool ok = image ? clipboardImage.save(path, "PNG") : writeBytesToFile(path, data);
    if (!ok) {
        emit statusMessageRequested(UiText::t("Could not create clipboard file.", "クリップボードからファイルを作成できませんでした。"));
        return false;
    }
    reload();
    setCurrentIndexForPath(path);
    emit statusMessageRequested(UiText::t("Created file from clipboard.", "クリップボードからファイルを作成しました。"));
    updateStatusLine();
    return true;
}

void FilePane::copySelectedPaths()
{
    QStringList paths;
    for (const QUrl &url : selectedUrls()) {
        paths.append(url.toLocalFile());
    }
    QApplication::clipboard()->setText(paths.join('\n'));
    emit statusMessageRequested(UiText::t("Copied absolute path(s).", "絶対パスをコピーしました。"));
}
