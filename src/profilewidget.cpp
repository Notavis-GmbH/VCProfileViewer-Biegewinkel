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
#include <QWheelEvent>
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
    // Right mouse button = Pan
    if (e->button() == Qt::RightButton) {
        QValueAxis *axX = qobject_cast<QValueAxis*>(chart()->axes(Qt::Horizontal).first());
        QValueAxis *axZ = qobject_cast<QValueAxis*>(chart()->axes(Qt::Vertical).first());
        if (axX && axZ) {
            m_panning  = true;
            m_panStart = e->pos();
            m_panX0min = axX->min(); m_panX0max = axX->max();
            m_panZ0min = axZ->min(); m_panZ0max = axZ->max();
            setCursor(Qt::ClosedHandCursor);
            e->accept();
            return;
        }
    }
    // Left mouse button = ROI draw
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
    // Pan
    if (m_panning) {
        QValueAxis *axX = qobject_cast<QValueAxis*>(chart()->axes(Qt::Horizontal).first());
        QValueAxis *axZ = qobject_cast<QValueAxis*>(chart()->axes(Qt::Vertical).first());
        if (axX && axZ) {
            QRectF plotArea = chart()->plotArea();
            double dxPx = e->pos().x() - m_panStart.x();
            double dzPx = e->pos().y() - m_panStart.y();
            double xRange = m_panX0max - m_panX0min;
            double zRange = m_panZ0max - m_panZ0min;
            double dx = -dxPx / plotArea.width()  * xRange;
            double dz =  dzPx / plotArea.height() * zRange;
            axX->setRange(m_panX0min + dx, m_panX0max + dx);
            axZ->setRange(m_panZ0min + dz, m_panZ0max + dz);
        }
        e->accept();
        return;
    }
    // ROI rubber-band
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
    // End pan
    if (m_panning && e->button() == Qt::RightButton) {
        m_panning = false;
        setCursor(m_drawingRoi != ROI_NONE ? Qt::CrossCursor : Qt::ArrowCursor);
        e->accept();
        return;
    }
    // End ROI draw
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

void ProfileChartView::mouseDoubleClickEvent(QMouseEvent *e)
{
    // Double-click = emit signal so ProfileWidget can reset zoom
    emit resetZoomRequested();
    QChartView::mouseDoubleClickEvent(e);
}

void ProfileChartView::wheelEvent(QWheelEvent *e)
{
    QValueAxis *axX = qobject_cast<QValueAxis*>(chart()->axes(Qt::Horizontal).first());
    QValueAxis *axZ = qobject_cast<QValueAxis*>(chart()->axes(Qt::Vertical).first());
    if (!axX || !axZ) { QChartView::wheelEvent(e); return; }

    // Zoom factor: scroll up = zoom in, scroll down = zoom out
    double factor = (e->angleDelta().y() > 0) ? 0.85 : 1.0 / 0.85;

    // Zoom around the mouse cursor position in chart coordinates
    QPointF pivot = widgetToChart(e->position().toPoint());

    double newXMin = pivot.x() + (axX->min() - pivot.x()) * factor;
    double newXMax = pivot.x() + (axX->max() - pivot.x()) * factor;
    double newZMin = pivot.y() + (axZ->min() - pivot.y()) * factor;
    double newZMax = pivot.y() + (axZ->max() - pivot.y()) * factor;

    axX->setRange(newXMin, newXMax);
    axZ->setRange(newZMin, newZMax);

    e->accept();
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
    connect(m_chartView, &ProfileChartView::resetZoomRequested,
            this,        &ProfileWidget::resetZoom);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_chartView);
    setLayout(layout);
}

void ProfileWidget::updateProfile(const std::vector<ProfilePoint> &points)
{
    if (points.empty()) { clearProfile(); return; }

    // Compute data bounds
    float minX =  std::numeric_limits<float>::max();
    float maxX = -std::numeric_limits<float>::max();
    float minZ =  std::numeric_limits<float>::max();
    float maxZ = -std::numeric_limits<float>::max();

    QList<QPointF> pts;
    pts.reserve(static_cast<int>(points.size()));
    for (auto &p : points) {
        pts.append(QPointF(p.x_mm, p.z_mm));
        minX = std::min(minX, p.x_mm);
        maxX = std::max(maxX, p.x_mm);
        minZ = std::min(minZ, p.z_mm);
        maxZ = std::max(maxZ, p.z_mm);
    }

    // Use replace() – but guard against Qt Charts not painting:
    // always call setRange afterwards so the chart marks itself dirty.
    m_profileSeries->replace(pts);

    float marginX = (maxX - minX) * 0.05f + 1.0f;
    float marginZ = (maxZ - minZ) * 0.10f + 1.0f;

    if (m_autoScale || m_firstFrame) {
        // Fit axes to actual data – fixes invisible profile when
        // data range (e.g. Z: 108..251 mm) differs from initial defaults.
        m_axisX->setRange(minX - marginX, maxX + marginX);
        m_axisZ->setRange(minZ - marginZ, maxZ + marginZ);
        m_firstFrame = false;
    } else {
        // Force Qt Charts to repaint even though axes did not change
        double xMin = m_axisX->min(), xMax = m_axisX->max();
        m_axisX->setRange(xMin, xMax + 1e-9);  // tiny nudge
        m_axisX->setRange(xMin, xMax);
    }
    m_chartView->viewport()->update();
}

void ProfileWidget::clearProfile()
{
    m_profileSeries->clear();
    m_firstFrame = true;  // next data will re-fit axes
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

void ProfileWidget::resetZoom()
{
    // Re-fit axes to the current series data
    const auto &pts = m_profileSeries->points();
    if (pts.isEmpty()) return;

    double minX = pts[0].x(), maxX = pts[0].x();
    double minZ = pts[0].y(), maxZ = pts[0].y();
    for (const auto &p : pts) {
        minX = std::min(minX, p.x()); maxX = std::max(maxX, p.x());
        minZ = std::min(minZ, p.y()); maxZ = std::max(maxZ, p.y());
    }
    double mX = (maxX - minX) * 0.05 + 1.0;
    double mZ = (maxZ - minZ) * 0.10 + 1.0;
    m_axisX->setRange(minX - mX, maxX + mX);
    m_axisZ->setRange(minZ - mZ, maxZ + mZ);
    m_chartView->viewport()->update();
}

void ProfileWidget::updateProductResult(const QString &/*resultText*/)
{
    // Handled in MainWindow overlay
}
