#include "views/BreadcrumbBar.h"

#include <QDir>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QToolButton>

BreadcrumbBar::BreadcrumbBar(QWidget *parent)
    : QWidget(parent)
{
    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(2);
    m_layout->addStretch(1);
    setCursor(Qt::IBeamCursor); // empty area behaves like the path editor
}

void BreadcrumbBar::setPath(const QString &path)
{
    m_staticText.clear();
    m_segments.clear();

    const QString cleaned = QDir::cleanPath(path);
    const QString home = QDir::homePath();
    QString remaining = cleaned;
    if (cleaned == home || cleaned.startsWith(home + "/")) {
        m_segments.append({QStringLiteral("~"), home});
        remaining = cleaned.mid(home.size());
    } else {
        m_segments.append({QStringLiteral("/"), QStringLiteral("/")});
    }
    QString accumulated = m_segments.first().path;
    const QStringList parts = remaining.split('/', Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        accumulated = accumulated.endsWith('/') ? accumulated + part
                                                : accumulated + "/" + part;
        m_segments.append({part, accumulated});
    }
    rebuild();
}

void BreadcrumbBar::setStaticText(const QString &text)
{
    m_segments.clear();
    m_staticText = text;
    rebuild();
}

void BreadcrumbBar::rebuild()
{
    for (QWidget *widget : std::as_const(m_segmentWidgets)) {
        widget->deleteLater();
    }
    m_segmentWidgets.clear();
    m_segmentStarts.clear();
    if (m_ellipsis) {
        m_ellipsis->deleteLater();
        m_ellipsis = nullptr;
    }

    int insertAt = 0;
    const auto addWidget = [this, &insertAt](QWidget *widget) {
        m_layout->insertWidget(insertAt++, widget);
        m_segmentWidgets.append(widget);
    };

    if (!m_staticText.isEmpty()) {
        auto *label = new QLabel(m_staticText, this);
        label->setObjectName("breadcrumbStatic");
        m_segmentStarts.append(0);
        addWidget(label);
        return;
    }

    m_ellipsis = new QLabel(QStringLiteral("…"), this);
    m_ellipsis->setObjectName("breadcrumbSeparator");
    m_layout->insertWidget(insertAt++, m_ellipsis);
    m_ellipsis->hide();

    for (int i = 0; i < m_segments.size(); ++i) {
        if (i > 0) {
            auto *separator = new QLabel(QStringLiteral("›"), this);
            separator->setObjectName("breadcrumbSeparator");
            m_segmentStarts.append(m_segmentWidgets.size());
            addWidget(separator);
        } else {
            m_segmentStarts.append(m_segmentWidgets.size());
        }
        auto *button = new QToolButton(this);
        button->setObjectName("breadcrumbSegment");
        button->setText(m_segments.at(i).label);
        button->setAutoRaise(true);
        button->setCursor(Qt::PointingHandCursor);
        button->setFocusPolicy(Qt::NoFocus);
        const QString target = m_segments.at(i).path;
        connect(button, &QToolButton::clicked, this, [this, target]() {
            emit pathClicked(target);
        });
        addWidget(button);
    }
    updateElision();
}

void BreadcrumbBar::updateElision()
{
    if (!m_staticText.isEmpty() || m_segments.isEmpty() || !m_ellipsis) {
        return;
    }
    const int spacing = m_layout->spacing();
    const auto widthOf = [spacing](QWidget *widget) {
        return widget->sizeHint().width() + spacing;
    };

    // Total width with everything visible; drop leading segments (keeping
    // the root) until the rest fits.
    int total = 0;
    for (QWidget *widget : std::as_const(m_segmentWidgets)) {
        total += widthOf(widget);
    }
    int firstShown = 0;
    const int available = width();
    int shownWidth = total;
    while (firstShown < m_segments.size() - 1
           && shownWidth + widthOf(m_ellipsis) > available) {
        // Hide the widgets belonging to segment `firstShown`.
        const int start = m_segmentStarts.at(firstShown);
        const int end = (firstShown + 1 < m_segmentStarts.size())
            ? m_segmentStarts.at(firstShown + 1)
            : m_segmentWidgets.size();
        for (int i = start; i < end; ++i) {
            shownWidth -= widthOf(m_segmentWidgets.at(i));
        }
        ++firstShown;
    }
    for (int segment = 0; segment < m_segments.size(); ++segment) {
        const int start = m_segmentStarts.at(segment);
        const int end = (segment + 1 < m_segmentStarts.size())
            ? m_segmentStarts.at(segment + 1)
            : m_segmentWidgets.size();
        for (int i = start; i < end; ++i) {
            m_segmentWidgets.at(i)->setVisible(segment >= firstShown);
        }
    }
    m_ellipsis->setVisible(firstShown > 0);
}

void BreadcrumbBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        emit editRequested();
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void BreadcrumbBar::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateElision();
}
