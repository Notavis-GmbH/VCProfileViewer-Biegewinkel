/****************************************************************************
** profilewidget.cpp
****************************************************************************/
#include "profilewidget.h"
#include <QPainter>
#include <QPen>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QFont>
#include <algorithm>
#include <limits>

// ========================================================================
// ProfileChartView
// ========================================================================

ProfileChartView::ProfileChartView(QChart *chart, QWidget *parent)
    : QChartView(chart, parent)
{
    setRenderHint(QPainter::Antialiasing);
    setMouseTracking(true);
    m_rois[0] = m_rois[1] = RoiRect{};
}

void ProfileChartView::setDrawingRoi(RoiId id)
{
    m_drawingRoi = id;
    if (id != ROI_NONE)
        setCursor(Qt::CrossCursor);
    else
        setCursor(Qt::ArrowCursor);
}

void ProfileChartView::setRoi(RoiId id, const RoiRect &r)
{
    if (id >= 0 && id < 2) {
        m_rois[id] = r;
        viewport()->update();
    }
}

QPointF ProfileChartView::widgetToChart(const QPoint &p) const
{
    QRectF plotArea = chart()->plotArea();
    auto   axes     = chart()->axes();
    if (axes.size() < 2) return {};

    QValueAxis *axX = qobject_cast<QValueAxis*>(chart()->axes(Qt::Horizontal).first());
    QValueAxis *axZ = qobject_cast<QValueAxis*>(chart()->axes(Qt::Vertical).first());
    if (!axX || !axZ) return {};

    double xRatio = (p.x() - plotArea.left()) / plotArea.width();
    double zRatio = (p.y() - plotArea.top())  / plotArea.height();

    double x = axX->min() + xRatio * (axX->max() - axX->min());
    double z = axZ->max() - zRatio * (axZ->max() - axZ->min());

    return QPointF(x, z);
}

void ProfileChartView::mousePressEvent(QMouseEvent *e)
{
    if (m_drawingRoi != ROI_NONE && e->button() == Qt::LeftButton) {
        m_dragging    = true;
        m_dragStart   = e->pos();
        m_dragCurrent = e->pos();
        e->accept();
        return;
    }
    QChartView::mousePressEvent(e);
}

void ProfileChartView::mouseMoveEvent(QMouseEvent *e)
{
    if (m_dragging) {
        m_dragCurrent = e->pos();
        viewport()->update();
        e->accept();
        return;
    }
    QChartView::mouseMoveEvent(e);
}

void ProfileChartView::mouseReleaseEvent(QMouseEvent *e)
{
    if (m_dragging && e->button() == Qt::LeftButton) {
        m_dragging = false;
        QPointF p0 = widgetToChart(m_dragStart);
        QPointF p1 = widgetToChart(e->pos());

        RoiRect r;
        r.xMin  = std::min(p0.x(), p1.x());
        r.xMax  = std::max(p0.x(), p1.x());
        r.zMin  = std::min(p0.y(), p1.y());
        r.zMax  = std::max(p0.y(), p1.y());
        r.valid = (r.xMax - r.xMin > 0.1) && (r.zMax - r.zMin > 0.1);

        if (r.valid) {
            m_rois[m_drawingRoi] = r;
            emit roiChanged(m_drawingRoi, r);
        }
        setDrawingRoi(ROI_NONE);
        viewport()->update();
        e->accept();
        return;
    }
    QChartView::mouseReleaseEvent(e);
}

// Chart-pixel coordinates of a world point
QPoint ProfileChartView::chartToWidget(double x, double z) const
{
    QRectF plotArea = chart()->plotArea();
    QValueAxis *axX = qobject_cast<QValueAxis*>(chart()->axes(Qt::Horizontal).first());
    QValueAxis *axZ = qobject_cast<QValueAxis*>(chart()->axes(Qt::Vertical).first());
    if (!axX || !axZ) return {};

    double xRatio = (x - axX->min()) / (axX->max() - axX->min());
    double zRatio = (axZ->max() - z)  / (axZ->max() - axZ->min());

    int px = static_cast<int>(plotArea.left() + xRatio * plotArea.width());
    int py = static_cast<int>(plotArea.top()  + zRatio * plotArea.height());
    return QPoint(px, py);
}

void ProfileChartView::drawRoiOverlay(QPainter &painter, RoiId id, QColor color)
{
    RoiRect &r = m_rois[id];
    if (!r.valid) return;

    QPoint tl = chartToWidget(r.xMin, r.zMax);
    QPoint br = chartToWidget(r.xMax, r.zMin);
    QRect  rect(tl, br);

    // Fill
    QColor fill = color;
    fill.setAlpha(40);
    painter.fillRect(rect, fill);

    // Border
    QPen pen(color, 2, Qt::SolidLine);
    painter.setPen(pen);
    painter.drawRect(rect);

    // Label
    QString label = QString("ROI %1").arg(id + 1);
    QFont f = painter.font();
    f.setBold(true);
    f.setPointSize(9);
    painter.setFont(f);
    painter.setPen(color);
    painter.drawText(tl + QPoint(4, 14), label);
}

void ProfileChartView::paintEvent(QPaintEvent *e)
{
    QChartView::paintEvent(e);

    QPainter painter(viewport());
    painter.setRenderHint(QPainter::Antialiasing);

    // Draw committed ROIs
    drawRoiOverlay(painter, ROI_1, QColor(0, 180, 255));   // blue
    drawRoiOverlay(painter, ROI_2, QColor(255, 140, 0));   // orange

    // Draw in-progress rubber-band
    if (m_dragging) {
        QRect dragRect = QRect(m_dragStart, m_dragCurrent).normalized();
        QColor col = (m_drawingRoi == ROI_1) ? QColor(0, 180, 255, 60)
                                              : QColor(255, 140, 0, 60);
        painter.fillRect(dragRect, col);
        QColor border = (m_drawingRoi == ROI_1) ? QColor(0, 180, 255)
                                                : QColor(255, 140, 0);
        painter.setPen(QPen(border, 2, Qt::DashLine));
        painter.drawRect(dragRect);
    }
}

// ========================================================================
// ProfileWidget
// ========================================================================

ProfileWidget::ProfileWidget(QWidget *parent) : QWidget(parent)
{
    m_chart = new QChart();
    m_chart->setTheme(QChart::ChartThemeDark);
    m_chart->legend()->hide();
    m_chart->setMargins(QMargins(0, 0, 0, 0));
    m_chart->setBackgroundBrush(QBrush(QColor(30, 30, 30)));

    m_profileSeries = new QLineSeries();
    QPen pen(QColor(0, 200, 100));
    pen.setWidth(2);
    m_profileSeries->setPen(pen);
    m_chart->addSeries(m_profileSeries);

    m_axisX = new QValueAxis();
    m_axisX->setTitleText("X [mm]");
    m_axisX->setLabelFormat("%.1f");
    m_axisX->setGridLineColor(QColor(70, 70, 70));
    m_axisX->setTitleBrush(QBrush(Qt::white));
    m_axisX->setLabelsBrush(QBrush(Qt::white));
    m_axisX->setRange(0, 150);

    m_axisZ = new QValueAxis();
    m_axisZ->setTitleText("Z [mm]");
    m_axisZ->setLabelFormat("%.1f");
    m_axisZ->setGridLineColor(QColor(70, 70, 70));
    m_axisZ->setTitleBrush(QBrush(Qt::white));
    m_axisZ->setLabelsBrush(QBrush(Qt::white));
    m_axisZ->setRange(0, 50);

    m_chart->addAxis(m_axisX, Qt::AlignBottom);
    m_chart->addAxis(m_axisZ, Qt::AlignLeft);
    m_profileSeries->attachAxis(m_axisX);
    m_profileSeries->attachAxis(m_axisZ);

    m_chartView = new ProfileChartView(m_chart, this);
    m_chartView->setMinimumHeight(400);

    connect(m_chartView, &ProfileChartView::roiChanged,
            this,        &ProfileWidget::roiChanged);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_chartView);
    setLayout(layout);
}

void ProfileWidget::updateProfile(const std::vector<ProfilePoint> &points)
{
    if (points.empty()) { clearProfile(); return; }

    QList<QPointF> pts;
    pts.reserve(static_cast<int>(points.size()));

    float minX =  std::numeric_limits<float>::max();
    float maxX = -std::numeric_limits<float>::max();
    float minZ =  std::numeric_limits<float>::max();
    float maxZ = -std::numeric_limits<float>::max();

    for (auto &p : points) {
        pts.append(QPointF(p.x_mm, p.z_mm));
        minX = std::min(minX, p.x_mm);
        maxX = std::max(maxX, p.x_mm);
        minZ = std::min(minZ, p.z_mm);
        maxZ = std::max(maxZ, p.z_mm);
    }

    // Block chart updates while replacing data to avoid flicker,
    // then set axis ranges which also triggers a repaint.
    m_chart->setUpdatesEnabled(false);
    m_profileSeries->replace(pts);
    m_chart->setUpdatesEnabled(true);

    // Always update axis range so Qt Charts redraws the series.
    float marginX = (maxX - minX) * 0.05f + 1.0f;
    float marginZ = (maxZ - minZ) * 0.10f + 1.0f;
    if (m_autoScale) {
        m_axisX->setRange(minX - marginX, maxX + marginX);
        m_axisZ->setRange(minZ - marginZ, maxZ + marginZ);
    } else {
        // Force a minor nudge so Qt Charts notices the series changed
        double xMin = m_axisX->min(), xMax = m_axisX->max();
        m_axisX->setRange(xMin, xMax);
    }
    m_chartView->viewport()->update();
}

void ProfileWidget::clearProfile()
{
    m_profileSeries->clear();
}

void ProfileWidget::setRoi(int roiId, const RoiRect &r)
{
    if (roiId == 0 || roiId == 1)
        m_chartView->setRoi(static_cast<ProfileChartView::RoiId>(roiId), r);
}

RoiRect ProfileWidget::roi(int id) const
{
    if (id == 0 || id == 1)
        return m_chartView->roi(static_cast<ProfileChartView::RoiId>(id));
    return {};
}

void ProfileWidget::onDrawRoi1()
{
    m_chartView->setDrawingRoi(ProfileChartView::ROI_1);
}

void ProfileWidget::onDrawRoi2()
{
    m_chartView->setDrawingRoi(ProfileChartView::ROI_2);
}

void ProfileWidget::updateProductResult(const QString &/*resultText*/)
{
    // Handled in MainWindow overlay
}
