/****************************************************************************
** profilewidget.cpp
****************************************************************************/
#include "profilewidget.h"
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QFont>
#include <QFontMetrics>
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
    // Disable Qt Charts built-in rubber-band zoom – we handle wheel ourselves
    setRubberBand(QChartView::NoRubberBand);
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
            chart()->update();
            scene()->update();
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

void ProfileChartView::setHeatmapData(
        const std::vector<std::pair<double,double>> &res1,
        const std::vector<std::pair<double,double>> &res2,
        const FitLine &fl1, const FitLine &fl2)
{
    m_hmRes1 = res1;  m_hmLine1 = fl1;
    m_hmRes2 = res2;  m_hmLine2 = fl2;
    viewport()->update();
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

    // Force immediate repaint – Qt Charts defers redraws otherwise
    chart()->update();
    scene()->update();
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

// Helper: colour for a residual value relative to the max in this ROI
static QColor heatColor(double residual, double maxRes)
{
    if (maxRes < 1e-9) return QColor::fromHsv(120, 200, 255, 210);
    double t   = std::min(residual / maxRes, 1.0);   // 0 (good) … 1 (bad)
    int    hue = static_cast<int>((1.0 - t) * 120.0); // green=120 → red=0
    return QColor::fromHsv(hue, 230, 255, 210);
}

void ProfileChartView::drawHeatmap(
        QPainter &painter,
        const std::vector<std::pair<double,double>> &residuals,
        const FitLine &fl)
{
    if (!fl.valid || residuals.empty()) return;

    painter.setPen(Qt::NoPen);
    const int hs = 4;   // half-size of each heat square in pixels
    for (auto &r : residuals) {
        double x   = r.first;
        double res = r.second;
        double z   = fl.slope * x + fl.intercept;  // z on fit line
        QPoint px  = chartToWidget(x, z);
        QColor col = heatColor(res, fl.maxResidual);
        painter.fillRect(px.x() - hs, px.y() - hs, hs * 2, hs * 2, col);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  setFitLabels / setDocOverlayVisible
// ─────────────────────────────────────────────────────────────────────────────
void ProfileChartView::setFitLabels(const QString &m1Label, const QString &m2Label,
                                    double bendingAngleDeg)
{
    m_methodLabel1    = m1Label;
    m_methodLabel2    = m2Label;
    m_bendingAngle    = bendingAngleDeg;
    m_hasBendingAngle = !m1Label.isEmpty() && !m2Label.isEmpty();
    viewport()->update();
}

void ProfileChartView::setDocOverlayVisible(bool v)
{
    m_docOverlayVisible = v;
    viewport()->update();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Helper: draw a semi-transparent rounded-rect background for text
// ─────────────────────────────────────────────────────────────────────────────
static void drawBubble(QPainter &p, const QRect &r, QColor bg)
{
    p.save();
    p.setPen(Qt::NoPen);
    p.setBrush(bg);
    p.drawRoundedRect(r, 5, 5);
    p.restore();
}

// ─────────────────────────────────────────────────────────────────────────────
//  drawFitLineLabels – draw Phi + Method label at the RIGHT end of each fit line
// ─────────────────────────────────────────────────────────────────────────────
void ProfileChartView::drawFitLineLabels(QPainter &painter)
{
    // ROI 1 (blue) and ROI 2 (orange)
    struct LineInfo {
        const FitLine *fl;
        const QString *label;
        QColor         color;
        int            side;   // -1 = label left of end, +1 = label right of start
    };

    LineInfo lines[] = {
        { &m_hmLine1, &m_methodLabel1, QColor(0, 180, 255),  +1 },
        { &m_hmLine2, &m_methodLabel2, QColor(255, 140,  0), -1 },
    };

    QFont font = painter.font();
    font.setPointSize(9);
    font.setBold(true);
    painter.setFont(font);

    for (auto &li : lines) {
        if (!li.fl->valid || li.label->isEmpty()) continue;

        // Anchor point: right end of fit line (xMax)
        double anchorX = (li.side > 0) ? li.fl->xMax : li.fl->xMin;
        double anchorZ = li.fl->slope * anchorX + li.fl->intercept;
        QPoint anchor = chartToWidget(anchorX, anchorZ);

        // Build label string
        QString text = QString("%1  φ=%2°  RMS=%3μm")
                       .arg(*li.label)
                       .arg(li.fl->phi,          0, 'f', 2)
                       .arg(li.fl->rmsResidual * 1000.0, 0, 'f', 1);

        QFontMetrics fm(font);
        QRect textRect = fm.boundingRect(text);
        textRect.adjust(-6, -4, 6, 4);   // padding

        // Position: above the anchor, shifted left/right
        int tx = (li.side > 0)
                 ? anchor.x() - textRect.width() - 4
                 : anchor.x() + 4;
        int ty = anchor.y() - textRect.height() / 2 - 2;

        // Clamp to viewport
        tx = std::max(4, std::min(tx, viewport()->width()  - textRect.width()  - 4));
        ty = std::max(4, std::min(ty, viewport()->height() - textRect.height() - 4));

        textRect.moveTopLeft(QPoint(tx, ty));

        // Draw bubble + text
        QColor bg = li.color; bg.setAlpha(160);
        drawBubble(painter, textRect, QColor(20, 20, 30, 200));

        painter.setPen(QPen(li.color, 1));
        painter.drawRoundedRect(textRect, 5, 5);

        painter.setPen(li.color);
        painter.drawText(textRect, Qt::AlignCenter, text);

        // Draw a small circle at the anchor point
        painter.setPen(Qt::NoPen);
        painter.setBrush(li.color);
        painter.drawEllipse(anchor, 4, 4);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  drawInfoPanel – compact summary box (top-right corner of the chart)
//  Shows: ROI1 method+phi, ROI2 method+phi, Biegewinkel
// ─────────────────────────────────────────────────────────────────────────────
void ProfileChartView::drawInfoPanel(QPainter &painter)
{
    if (!m_hmLine1.valid && !m_hmLine2.valid) return;

    QFont font = painter.font();
    font.setPointSize(9);
    font.setBold(false);
    painter.setFont(font);
    QFontMetrics fm(font);

    // Build lines
    QStringList lines;
    if (m_hmLine1.valid && !m_methodLabel1.isEmpty()) {
        lines << QString("█ ROI 1  [%1]  φ=%2°  RMS=%3μm")
                         .arg(m_methodLabel1)
                         .arg(m_hmLine1.phi,               0, 'f', 2)
                         .arg(m_hmLine1.rmsResidual*1000.0, 0, 'f', 1);
    }
    if (m_hmLine2.valid && !m_methodLabel2.isEmpty()) {
        lines << QString("█ ROI 2  [%1]  φ=%2°  RMS=%3μm")
                         .arg(m_methodLabel2)
                         .arg(m_hmLine2.phi,               0, 'f', 2)
                         .arg(m_hmLine2.rmsResidual*1000.0, 0, 'f', 1);
    }
    if (m_hasBendingAngle) {
        lines << QString("▲ Biegewinkel = %1°")
                         .arg(std::abs(m_bendingAngle), 0, 'f', 2);
    }
    if (lines.isEmpty()) return;

    // Measure
    int maxW = 0;
    for (auto &l : lines) maxW = std::max(maxW, fm.horizontalAdvance(l));
    int lineH  = fm.height() + 3;
    int totalH = lines.size() * lineH + 10;
    int totalW = maxW + 16;

    // Place top-right, offset from plotArea right edge
    QRectF pa = chart()->plotArea();
    int px = static_cast<int>(pa.right()) - totalW - 8;
    int py = static_cast<int>(pa.top()) + 8;

    QRect panelRect(px, py, totalW, totalH);

    // Panel background
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(15, 15, 25, 215));
    painter.drawRoundedRect(panelRect, 6, 6);
    painter.setPen(QPen(QColor(80, 80, 100), 1));
    painter.drawRoundedRect(panelRect, 6, 6);

    // Text
    int y = py + 7;
    for (int i = 0; i < lines.size(); ++i) {
        // Leading █ colored per ROI / angle
        QColor col;
        if      (i == 0 && m_hmLine1.valid) col = QColor(0, 180, 255);
        else if (i == 1 && m_hmLine2.valid) col = QColor(255, 140, 0);
        else                                col = QColor(0, 230, 118);   // green for angle

        // Draw the colored block
        painter.setPen(col);
        QString leading = lines[i].left(1);  // "█" or "▲"
        painter.drawText(QRect(px + 6, y, totalW - 10, lineH), Qt::AlignLeft | Qt::AlignVCenter, leading);

        // Remaining text in white
        painter.setPen(QColor(220, 220, 220));
        painter.drawText(QRect(px + 6 + fm.horizontalAdvance(leading), y,
                               totalW - 12 - fm.horizontalAdvance(leading), lineH),
                         Qt::AlignLeft | Qt::AlignVCenter, lines[i].mid(1));
        y += lineH;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  drawDocOverlay – full documentation panel (toggle via ? button)
// ─────────────────────────────────────────────────────────────────────────────
void ProfileChartView::drawDocOverlay(QPainter &painter)
{
    // Semi-transparent full-chart panel
    QRectF pa = chart()->plotArea();
    QRect  overlay(static_cast<int>(pa.left())  + 20,
                   static_cast<int>(pa.top())   + 20,
                   std::min(520, static_cast<int>(pa.width())  - 40),
                   std::min(420, static_cast<int>(pa.height()) - 40));

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(10, 10, 20, 230));
    painter.drawRoundedRect(overlay, 10, 10);
    painter.setPen(QPen(QColor(100, 100, 130), 1));
    painter.drawRoundedRect(overlay, 10, 10);

    QFont titleFont = painter.font();
    titleFont.setPointSize(11);
    titleFont.setBold(true);
    QFont bodyFont = painter.font();
    bodyFont.setPointSize(9);
    bodyFont.setBold(false);

    int x  = overlay.x() + 16;
    int y  = overlay.y() + 18;
    int w  = overlay.width() - 32;

    // ─── Title ───────────────────────────────────────────────────────────────
    painter.setFont(titleFont);
    painter.setPen(QColor(0, 200, 255));
    painter.drawText(QRect(x, y, w, 22), Qt::AlignLeft, "VC 3D Profile Viewer  –  Hilfe & Dokumentation");
    y += 26;

    painter.setPen(QColor(70, 70, 90));
    painter.drawLine(x, y, x + w, y);
    y += 8;

    // ─── Section helper ───────────────────────────────────────────────────────
    auto section = [&](const QString &title) {
        painter.setFont(titleFont);
        painter.setPen(QColor(160, 200, 255));
        painter.drawText(QRect(x, y, w, 18), Qt::AlignLeft, title);
        y += 20;
    };
    auto row = [&](const QString &key, const QString &desc, QColor kc = QColor(200,200,200)) {
        painter.setFont(bodyFont);
        int kw = 130;
        painter.setPen(kc);
        painter.drawText(QRect(x + 4, y, kw, 16), Qt::AlignLeft, key);
        painter.setPen(QColor(180, 180, 180));
        painter.drawText(QRect(x + 4 + kw, y, w - kw - 4, 16), Qt::AlignLeft, desc);
        y += 17;
    };

    // ─── Steuerung ───────────────────────────────────────────────────────────
    section("▶ Steuerung");
    row("Mausrad",          "Zoom um Cursorposition");
    row("Rechte Maustaste", "Pan (Bild verschieben)");
    row("Doppelklick",      "Zoom auf gesamte Messdaten (Fit)");
    row("Linkes Drag",      "ROI aufziehen (wenn ROI-Modus aktiv)");
    row("⌫  Fit-Button",     "Zoom auf gesamte Messdaten zurücksetzen");
    y += 4;

    // ─── Linienfinder ────────────────────────────────────────────────────────
    section("▶ Linienfinder (pro ROI wählbar)");
    row("OLS",     "Kleinste Quadrate – schnell, optimal für saubere Profile",
        QColor(180, 220, 255));
    row("RANSAC",  "Random Sample Consensus – robust gegen Ausreißer & Reflexionen",
        QColor(255, 200, 100));
    row("Hough",   "Hough-Transformation – robust bei Lücken & Artefakten",
        QColor(100, 255, 180));
    row("Auto",    "Automatisch: wählt OLS/RANSAC/Hough je nach Inlier-Ratio+RMS",
        QColor(200, 160, 255));
    y += 4;

    // ─── Auto-Modus Schwellwerte ─────────────────────────────────────────────
    section("▶ Auto-Modus Heuristik");
    row("≥ 90 % Inlier",  "OLS (sauberes Profil)");
    row("60–90 % Inlier", "RANSAC (Ausreißer erkannt)");
    row("< 60 % Inlier",  "Hough (fragmentiertes Profil)");
    row("Inlier-Band",    "0,5 mm Abstand vom OLS-Fit");
    row("RMS-Fallback",   "RANSAC-RMS > 1,5 × OLS-RMS → Hough");
    y += 4;

    // ─── Visualisierung ─────────────────────────────────────────────────────
    section("▶ Visualisierung");
    row("Blaue Linie",     "Erkannte Gerade ROI 1 (inkl. φ und RMS)");
    row("Orange Linie",    "Erkannte Gerade ROI 2 (inkl. φ und RMS)");
    row("Heatmap",         "Residuen je Messpunkt: grün=klein → rot=groß");
    row("Info-Panel",      "Zusammenfassung oben rechts im Chart");

    // ─── Close hint ──────────────────────────────────────────────────────────
    painter.setFont(bodyFont);
    painter.setPen(QColor(120, 120, 140));
    painter.drawText(QRect(x, overlay.bottom() - 20, w, 16),
                     Qt::AlignRight, "[ ? ] zum Schließen");
}

void ProfileChartView::paintEvent(QPaintEvent *e)
{
    QChartView::paintEvent(e);

    QPainter painter(viewport());
    painter.setRenderHint(QPainter::Antialiasing);

    // 1. Heatmap (drawn first, behind ROI borders and lines)
    drawHeatmap(painter, m_hmRes1, m_hmLine1);
    drawHeatmap(painter, m_hmRes2, m_hmLine2);

    // 2. ROI overlay borders
    drawRoiOverlay(painter, ROI_1, QColor(0, 180, 255));
    drawRoiOverlay(painter, ROI_2, QColor(255, 140, 0));

    // 3. Fit-line labels (phi, method, RMS) at line ends
    drawFitLineLabels(painter);

    // 4. Info panel (top-right summary box)
    drawInfoPanel(painter);

    // 5. ROI rubber-band while drawing
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

    // 6. Documentation overlay (toggled by ? button)
    if (m_docOverlayVisible)
        drawDocOverlay(painter);
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

    // Profile series (green)
    m_profileSeries = new QLineSeries();
    QPen pen(QColor(0, 200, 100));
    pen.setWidth(2);
    m_profileSeries->setPen(pen);
    m_chart->addSeries(m_profileSeries);

    // Fit line series – ROI 1 (blue) and ROI 2 (orange), drawn on top
    m_fitSeries1 = new QLineSeries();
    QPen fitPen1(QColor(0, 180, 255));
    fitPen1.setWidth(3);
    fitPen1.setStyle(Qt::SolidLine);
    m_fitSeries1->setPen(fitPen1);
    m_chart->addSeries(m_fitSeries1);

    m_fitSeries2 = new QLineSeries();
    QPen fitPen2(QColor(255, 140, 0));
    fitPen2.setWidth(3);
    fitPen2.setStyle(Qt::SolidLine);
    m_fitSeries2->setPen(fitPen2);
    m_chart->addSeries(m_fitSeries2);

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
    m_fitSeries1->attachAxis(m_axisX);
    m_fitSeries1->attachAxis(m_axisZ);
    m_fitSeries2->attachAxis(m_axisX);
    m_fitSeries2->attachAxis(m_axisZ);

    m_chartView = new ProfileChartView(m_chart, this);
    m_chartView->setMinimumHeight(400);

    connect(m_chartView, &ProfileChartView::roiChanged,
            this,        &ProfileWidget::roiChanged);
    connect(m_chartView, &ProfileChartView::resetZoomRequested,
            this,        &ProfileWidget::resetZoom);

    // ── Button toolbar at the bottom of the chart ────────────────────────
    auto *toolbar = new QHBoxLayout;
    toolbar->setContentsMargins(4, 2, 4, 2);
    toolbar->setSpacing(6);

    QPushButton *btnDoc = new QPushButton("?  Hilfe");
    btnDoc->setToolTip("Dokumentation und Steuerungshinweise einblenden");
    btnDoc->setStyleSheet(
        "QPushButton { background:#1a1a3a; color:#aac8ff; "
        "border:1px solid #334; border-radius:4px; padding:3px 10px; font-size:11px; }"
        "QPushButton:checked { background:#003366; color:#00ccff; border:1px solid #00aaff; }"
        "QPushButton:hover   { background:#1e2a50; }");
    btnDoc->setCheckable(true);
    connect(btnDoc, &QPushButton::toggled, this, &ProfileWidget::onToggleDocOverlay);

    // Color legend
    QLabel *lbl1 = new QLabel("▬ ROI 1");
    lbl1->setStyleSheet("color:#00b4ff; font-size:11px; font-weight:bold;");
    QLabel *lbl2 = new QLabel("▬ ROI 2");
    lbl2->setStyleSheet("color:#ff8c00; font-size:11px; font-weight:bold;");
    QLabel *lblHeat = new QLabel("■ Heatmap: Residuen");
    lblHeat->setStyleSheet(
        "background: qlineargradient(x1:0,y1:0,x2:1,y2:0, "
        "stop:0 #00c800, stop:0.5 #c8c800, stop:1 #c80000); "
        "color:white; font-size:10px; padding:1px 6px; border-radius:3px;");

    toolbar->addWidget(lbl1);
    toolbar->addWidget(lbl2);
    toolbar->addSpacing(8);
    toolbar->addWidget(lblHeat);
    toolbar->addStretch();
    toolbar->addWidget(btnDoc);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_chartView);
    layout->addLayout(toolbar);
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
    m_fitSeries1->clear();
    m_fitSeries2->clear();
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

void ProfileWidget::updateFitLines(
        const FitLine &line1, const FitLine &line2,
        const std::vector<std::pair<double,double>> &residuals1,
        const std::vector<std::pair<double,double>> &residuals2)
{
    // Update fit-line Qt series (drawn by Qt Charts engine)
    auto fillLine = [](QLineSeries *s, const FitLine &fl) {
        s->clear();
        if (!fl.valid) return;
        s->append(fl.xMin, fl.slope * fl.xMin + fl.intercept);
        s->append(fl.xMax, fl.slope * fl.xMax + fl.intercept);
    };
    fillLine(m_fitSeries1, line1);
    fillLine(m_fitSeries2, line2);

    m_fitLine1 = line1;
    m_fitLine2 = line2;
    m_residuals1 = residuals1;
    m_residuals2 = residuals2;

    // Forward heatmap data to the chart view for per-point paintEvent rendering
    m_chartView->setHeatmapData(residuals1, residuals2, line1, line2);

    m_chartView->viewport()->update();
}

void ProfileWidget::setFitLabels(const QString &m1Label, const QString &m2Label,
                                  double bendingAngleDeg)
{
    m_chartView->setFitLabels(m1Label, m2Label, bendingAngleDeg);
}

void ProfileWidget::onToggleDocOverlay()
{
    // Called by the checkable ? button – just relay current state to the chart view
    // We look up the sender's checked state to avoid storing a member pointer
    QPushButton *btn = qobject_cast<QPushButton*>(sender());
    bool checked = btn ? btn->isChecked() : false;
    m_chartView->setDocOverlayVisible(checked);
}

void ProfileWidget::updateProductResult(const QString &/*resultText*/)
{
    // Handled in MainWindow overlay
}
