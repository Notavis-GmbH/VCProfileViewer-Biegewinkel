/****************************************************************************
** profilewidget.h
** Interactive 2D profile display with ROI drawing via mouse drag
****************************************************************************/
#pragma once

#include <QWidget>
#include <QChart>
#include <QChartView>
#include <QLineSeries>
#include <QScatterSeries>
#include <QValueAxis>
#include <QRubberBand>
#include <QMouseEvent>
#include <QRectF>
#include <vector>
#include "vcprotocol.h"  // ProfilePoint defined there

// Region of interest (in sensor world coordinates mm)
struct RoiRect {
    double xMin, xMax, zMin, zMax;
    bool valid = false;
};

class ProfileChartView : public QChartView
{
    Q_OBJECT
public:
    explicit ProfileChartView(QChart *chart, QWidget *parent = nullptr);

    // ROI editing mode
    enum RoiId { ROI_NONE = -1, ROI_1 = 0, ROI_2 = 1 };
    void setDrawingRoi(RoiId id);
    RoiId drawingRoi() const { return m_drawingRoi; }

    void setRoi(RoiId id, const RoiRect &r);
    RoiRect roi(RoiId id) const { return m_rois[id]; }

signals:
    void roiChanged(int roiId, RoiRect r);

protected:
    void mousePressEvent(QMouseEvent *e)   override;
    void mouseMoveEvent(QMouseEvent *e)    override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void paintEvent(QPaintEvent *e)        override;

private:
    QPoint  chartToWidget(double x, double z) const;
    QPointF widgetToChart(const QPoint &p)    const;
    void    drawRoiOverlay(QPainter &painter, RoiId id, QColor color);

    RoiId   m_drawingRoi = ROI_NONE;
    bool    m_dragging   = false;
    QPoint  m_dragStart;
    QPoint  m_dragCurrent;
    RoiRect m_rois[2];
};

// -----------------------------------------------------------------------

class ProfileWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ProfileWidget(QWidget *parent = nullptr);

    void updateProfile(const std::vector<ProfilePoint> &points);
    void updateProductResult(const QString &resultText);
    void clearProfile();

    void setRoi(int roiId, const RoiRect &r);
    RoiRect roi(int id) const;

public slots:
    void onDrawRoi1();
    void onDrawRoi2();

signals:
    void roiChanged(int roiId, RoiRect r);

private:
    ProfileChartView *m_chartView;
    QChart           *m_chart;
    QLineSeries      *m_profileSeries;
    QValueAxis       *m_axisX;
    QValueAxis       *m_axisZ;
    bool              m_autoScale = true;
};
