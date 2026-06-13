#pragma once

#include <QString>
#include <QStringList>

// OS-dependent services. Each target platform provides one implementation under
// platform/<os>/; the UI and core layers only call through this interface.
namespace tfx::platform {

// Open a path with the desktop's default application. Returns false on failure.
bool openPath(const QString &path);

// Show the desktop's native "open with" application chooser for a file.
// Returns false when no native chooser is available (caller may fall back).
bool openWithChooser(const QString &path);

// Reveal a file or folder in the system file manager.
bool revealInFileManager(const QString &path);

// Move the given paths to the trash/recycle bin. Returns true if all succeeded.
bool moveToTrash(const QStringList &paths);

// Shell program and argument vector used to run a one-off command line.
QString terminalShellProgram();
QStringList terminalRunArguments(const QString &command);

// List the entries inside a ZIP archive. Directory entries end with '/'.
QStringList listZipEntries(const QString &zipPath);

// Extract a single entry from a ZIP into destDir (preserving the entry's
// relative path under destDir). Returns true on success.
bool extractZipEntry(const QString &zipPath, const QString &entry, const QString &destDir);

// Whether a PDF preview renderer is available on this system.
bool pdfPreviewAvailable();

// Render the first page of a PDF to a PNG file at roughly `scaleToPx` pixels on
// the long edge. `outputPngPath` must end in ".png". Returns true on success.
bool renderPdfPreview(const QString &pdfPath, const QString &outputPngPath, int scaleToPx);

}
