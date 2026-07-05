#include "FilePane.h"
#include "views/FileViews.h"

#include <QDir>
#include <QStackedWidget>
#include <QVBoxLayout>

FilePane::FilePane(const QString &label, const QString &initialPath, QWidget *parent)
    : QWidget(parent),
      m_model(new QFileSystemModel(this)),
      m_proxyModel(new FileSystemProxyModel(this)),
      m_tabBar(new QTabBar(this)),
      m_view(new FileTableView(this)),
      m_badgeLabel(new QLabel(this)),
      m_pathEdit(new QLineEdit(this)),
      m_statusLabel(new QLabel(this)),
      m_label(label)
{
    setObjectName("filePane");
    setAttribute(Qt::WA_StyledBackground, true);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumWidth(220);
    m_model->setRootPath("/");
    m_model->setFilter(QDir::AllEntries | QDir::NoDot | QDir::AllDirs | QDir::Files);
    m_model->setReadOnly(false);
    m_proxyModel->setSourceModel(m_model);
    m_proxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);
    m_proxyModel->setDynamicSortFilter(true);

    setupPaneChrome(initialPath);
    setupFileView();

    setupGitRefresh();

    auto *headerBar = createHeaderLayout();
    setupSearchView();
    setupIconView();
    setupZipView();
    setupViewStack();

    setupSearchConnections();

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(0);
    layout->addWidget(m_tabBar);
    layout->addWidget(headerBar);
    layout->addWidget(m_viewStack, 1);
    layout->addWidget(m_statusLabel);

    setupFileViewConnections();
    setupTabConnections();

    navigateTo(initialPath, false);
    setActive(false);
}

FilePane::~FilePane() = default;
