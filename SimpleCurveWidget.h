#ifndef SIMPLECURVEWIDGET_H
#define SIMPLECURVEWIDGET_H

#include <GL/glew.h>
#include <QGLWidget>
#include <vector>
#include <QPointF>
#include <QImage>
#include <QPainter>
#include <cmath>
#include <QMouseEvent>
#include <QTimer>
#include <QMenu>


using WaveformPoints = std::vector<QPointF>;

#define SC_BUF_SIZE  1000
#define SC_MAX_UNIT  400


// 基础配置
const int TEXT_SIZE = 12;          // 刻度文本大小
const float BASE_X_RANGE = 10.0f;  // 初始X轴可视范围±10
const float BASE_Y_RANGE = 8.0f;   // 初始Y轴可视范围±8
const int MIN_TICK_COUNT = 5;      // 最小刻度数量
const int MAX_TICK_COUNT = 10;     // 最大刻度数量

class OscilloscopeWidget : public QGLWidget {
    Q_OBJECT
public:
    explicit OscilloscopeWidget(QWidget *parent = nullptr);
    ~OscilloscopeWidget() override;

    void setWaveformPoints(const WaveformPoints& points);
    void clearWaveform();

protected:
    void initializeGL() override;
    void resizeGL(int width, int height) override;
    void paintGL() override;

    // 鼠标交互：缩放+平移
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void wheelEvent(QWheelEvent *e) override;

    // 新增：右键菜单事件
    void contextMenuEvent(QContextMenuEvent *e) override;

private:
    // 计算最优刻度间隔
    float calculateOptimalTickInterval(float range, float pixelPerUnit);
    // 生成带动态刻度的背景图
    QImage generateGridTextureImage(int width, int height);
    // 创建OpenGL纹理
    GLuint createTextureFromQImage(const QImage& image);
    // 绘制刻度纹理背景
    void drawGridTexture();
    // 绘制波形
    void drawWaveform();
    // 标记纹理需要更新
    void markTextureDirty() { m_isTextureDirty = true; }

    // 新增：右键菜单相关
    void initContextMenu();  // 初始化右键菜单

    // 新增：绘制鼠标位置
    void drawMousePosition();

    // OpenGL纹理相关
    GLuint m_gridTextureID = 0;
    QImage m_gridTextureImage;
    bool m_isTextureDirty = true;   // 纹理是否需要更新

    // 波形数据
    WaveformPoints m_waveformPoints;
    float   m_pEchoBuff[SC_BUF_SIZE][SC_MAX_UNIT];   //回波数据
    int  m_echoUnitNum[SC_BUF_SIZE];   //回波数据

    // 视图变换参数
    float m_viewRangeX = BASE_X_RANGE;  // 当前X轴可视范围（±值）
    float m_viewRangeY = BASE_Y_RANGE;  // 当前Y轴可视范围（±值）
    QPointF m_viewOffset = QPointF(0,0); // 视图平移偏移
    QPoint m_lastMousePos;              // 鼠标上次位置
    float m_zoom_x = 1.0f;
    float m_zoom_y = 1.0f;
    // 新增：鼠标位置追踪
    QPointF m_currentMouseGLPos; // 当前鼠标对应的OpenGL坐标

    // 新增：菜单相关成员
    QMenu* m_contextMenu = nullptr;    // 右键菜单
    QAction* m_actResetView = nullptr; // 重置视图动作
    QAction* m_actShowMousePosition = nullptr; // 鼠标定位动作
    bool m_isMousePosVisible = false;       //鼠标定位是否显示（默认bu显示）

    //test
    QTimer* pTimer;
    int numPoints = 200;
    int timeOffset = 0;

private slots:
    void onTimer();
private slots:
    // 新增：菜单槽函数
    void onResetView();          // 重置视图
    void onMousePosition(bool isShown);    // 开启/关闭鼠标定位
};



#endif // SIMPLECURVEWIDGET_H
