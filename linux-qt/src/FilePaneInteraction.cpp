#include "FilePane.h"

#include <QEvent>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListView>
#include <QTableView>

bool FilePane::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_pathEdit) {
        if (event->type() == QEvent::FocusIn) {
            m_pathEdit->setText(m_currentPath);
            m_pathEdit->selectAll();
            emit activated(this);
        }
        if (event->type() == QEvent::FocusOut) {
            m_pathEdit->setText(displayPath(m_currentPath));
        }
        if (event->type() == QEvent::KeyPress) {
            auto *keyEvent = static_cast<QKeyEvent *>(event);
            if (keyEvent->key() == Qt::Key_Escape) {
                m_pathEdit->setText(displayPath(m_currentPath));
                m_view->setFocus();
                return true;
            }
        }
    }
    if (watched == m_view || watched == m_iconView || watched == m_searchView) {
        if (event->type() == QEvent::FocusIn || event->type() == QEvent::MouseButtonPress) {
            emit activated(this);
        }
        if (event->type() == QEvent::KeyPress) {
            auto *keyEvent = static_cast<QKeyEvent *>(event);
            if (watched == m_searchView) {
                if (keyEvent->key() == Qt::Key_Escape) {
                    cancelSearch();
                    focusFileList();
                    return true;
                }
                if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
                    const QModelIndex current = m_searchView->currentIndex();
                    if (current.isValid()) {
                        emit m_searchView->activated(current);
                    }
                    return true;
                }
                if (keyEvent->key() == Qt::Key_Backspace) {
                    cancelSearch();
                    goUp();
                    return true;
                }
            }
            if (keyEvent->key() == Qt::Key_Down
                && !m_view->currentIndex().isValid()
                && m_view->selectionModel()->selectedIndexes().isEmpty()) {
                if (selectParentEntry()) {
                    return true;
                }
            }
            if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
                openSelected();
                return true;
            }
            if (keyEvent->key() == Qt::Key_Backspace) {
                goUp();
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}
