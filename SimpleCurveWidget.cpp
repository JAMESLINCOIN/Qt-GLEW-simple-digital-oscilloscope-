#include "SimpleCurveWidget.h"
#include <QFont>
#include <QColor>
#include <QDebug>
#define M_PI 3.1415926
#include "cmath"
#include <QWheelEvent>

OscilloscopeWidget::OscilloscopeWidget(QWidget *parent)
    : QGLWidget(parent) {
    setAutoBufferSwap(true);
    setMinimumSize(1200, 800);
    setCursor(Qt::CrossCursor); // 十字光标更适合示波器
    m_waveformPoints.reserve(numPoints); // 预分配内存，提升效率
    pTimer = new QTimer(this);
    connect(pTimer,&QTimer::timeout,this,&OscilloscopeWidget::onTimer);
    pTimer->start(20);

    // 新增：初始化右键菜单
    initContextMenu();
    setMouseTracking(true);
}

OscilloscopeWidget::~OscilloscopeWidget() {
    makeCurrent();
    if (m_gridTextureID != 0) {
        glDeleteTextures(1, &m_gridTextureID);
    }
    doneCurrent();
}

void OscilloscopeWidget::initializeGL() {
    // 初始化GLEW
    glewExperimental = GL_TRUE;
    GLenum err = glewInit();
    if (err != GLEW_OK) {
        fprintf(stderr, "GLEW初始化失败: %s\n", glewGetErrorString(err));
        return;
    }

    // OpenGL基础配置
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glLineWidth(1.0f);
    glEnableClientState(GL_VERTEX_ARRAY);

    // 启用纹理和混合
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void OscilloscopeWidget::resizeGL(int width, int height) {
    glViewport(0, 0, width, height);
    markTextureDirty(); // 窗口大小变化，标记纹理需要更新
}

void OscilloscopeWidget::paintGL() {
    //glClear(GL_COLOR_BUFFER_BIT);
    glLoadIdentity();
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f); //! 设置背景色为黑色
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // 第一步：更新并绘制刻度纹理（如果需要）
    if (m_isTextureDirty) {
        m_gridTextureImage = generateGridTextureImage(width(), height());
        if (m_gridTextureID != 0) {
            glDeleteTextures(1, &m_gridTextureID);
        }
        m_gridTextureID = createTextureFromQImage(m_gridTextureImage);
        m_isTextureDirty = false;
    }
    drawGridTexture();

    // 第二步：设置正交投影（匹配当前视图范围）
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-m_viewRangeX + m_viewOffset.x(), m_viewRangeX + m_viewOffset.x(),
            -m_viewRangeY - m_viewOffset.y(), m_viewRangeY - m_viewOffset.y(),
            -1.0f, 1.0f);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // 第三步：绘制波形
    drawWaveform();

    drawMousePosition();
}

float OscilloscopeWidget::calculateOptimalTickInterval(float range, float pixelPerUnit) {
    // 目标刻度像素间隔（保证刻度不拥挤）
    const float targetTickPixel = 80.0f;
    float idealInterval = targetTickPixel / pixelPerUnit;

    // 最优间隔候选值（1,2,5,10,20...）
    const float candidates[] = {0.1f, 0.2f, 0.5f, 1.0f, 2.0f, 5.0f, 10.0f, 20.0f, 50.0f};
    float bestInterval = candidates[0];

    // 选择最接近理想间隔的候选值
    for (float c : candidates) {
        if (c >= idealInterval) {
            bestInterval = c;
            break;
        }
        bestInterval = c;
    }

    // 确保刻度数量在合理范围
    int tickCount = static_cast<int>((2 * range) / bestInterval);
    if (tickCount < MIN_TICK_COUNT) {
        bestInterval /= 2;
    } else if (tickCount > MAX_TICK_COUNT) {
        bestInterval *= 2;
    }

    return bestInterval;
}

QImage OscilloscopeWidget::generateGridTextureImage(int width, int height) {
    QImage image(width, height, QImage::Format_RGBA8888);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);

    // 1. 计算坐标映射关系
    float xPixelPerUnit = width / (2 * m_viewRangeX);
    float yPixelPerUnit = height / (2 * m_viewRangeY);
    int centerX = width / 2;
    int centerY = height / 2;

    // 2. 计算最优刻度间隔
    float xTickInterval = calculateOptimalTickInterval(m_viewRangeX, xPixelPerUnit);
    float yTickInterval = calculateOptimalTickInterval(m_viewRangeY, yPixelPerUnit);

    // 3. 绘制网格线（浅灰色）
    QPen gridPen(QColor(51, 51, 51));
    gridPen.setWidth(1);
    painter.setPen(gridPen);

    // 计算实际可视范围的起始/结束值（考虑平移）
    float xStart = -m_viewRangeX + m_viewOffset.x();
    float xEnd = m_viewRangeX + m_viewOffset.x();
    float yStart = -m_viewRangeY - m_viewOffset.y();
    float yEnd = m_viewRangeY - m_viewOffset.y();

    // 计算第一个刻度的位置（对齐到间隔）
    float firstXTick = floor(xStart / xTickInterval) * xTickInterval;
    float firstYTick = floor(yStart / yTickInterval) * yTickInterval;

    // 绘制竖线（X轴网格）
    for (float x = firstXTick; x <= xEnd; x += xTickInterval) {
        int pixelX = centerX + (x - m_viewOffset.x()) * xPixelPerUnit;
        painter.drawLine(pixelX, 0, pixelX, height);
    }

    // 绘制横线（Y轴网格）
    for (float y = firstYTick; y <= yEnd; y += yTickInterval) {
        int pixelY = centerY - (y + m_viewOffset.y()) * yPixelPerUnit;
        painter.drawLine(0, pixelY, width, pixelY);
    }

    // 4. 绘制坐标轴（白色粗线）
    QPen axisPen(Qt::white);
    axisPen.setWidth(2);
    painter.setPen(axisPen);
    // X轴（y=0的位置）
    int xAxisY = centerY - m_viewOffset.y() * yPixelPerUnit;
    painter.drawLine(0, xAxisY, width, xAxisY);
    // Y轴（x=0的位置）
    int yAxisX = centerX - m_viewOffset.x() * xPixelPerUnit;
    painter.drawLine(yAxisX, 0, yAxisX, height);

    // 5. 绘制刻度文本（白色）
    QFont font;
    font.setPointSize(TEXT_SIZE);
    painter.setFont(font);
    painter.setPen(Qt::white);

    // X轴刻度文本
    for (float x = firstXTick; x <= xEnd; x += xTickInterval) {
        if (fabs(x) < 0.01f) continue; // 跳过原点
        int pixelX = centerX + (x - m_viewOffset.x()) * xPixelPerUnit;
        int pixelY = xAxisY + TEXT_SIZE + 5;
        painter.drawText(pixelX - TEXT_SIZE/2, pixelY, QString::number(x, 'f', 1));
    }

    // Y轴刻度文本
    for (float y = firstYTick; y <= yEnd; y += yTickInterval) {
        if (fabs(y) < 0.01f) continue; // 跳过原点
        int pixelX = yAxisX - TEXT_SIZE * 2;
        int pixelY = centerY - (y + m_viewOffset.y()) * yPixelPerUnit + TEXT_SIZE/2;
        painter.drawText(pixelX, pixelY, QString::number(y, 'f', 1));
    }

    // 绘制原点文本（如果可视范围内包含原点）
    if (xStart <= 0 && xEnd >= 0 && yStart <= 0 && yEnd >= 0) {
        painter.drawText(yAxisX + 5, xAxisY + TEXT_SIZE, "0");
    }

    // ========== 新增：绘制鼠标坐标文本到纹理 ==========
    if (m_isMousePosVisible) {
        // 准备鼠标坐标文本
        QString posText = QString("鼠标位置: (X: %1, Y: %2)")
                              .arg(m_currentMouseGLPos.x(), 0, 'f', 2)
                              .arg(m_currentMouseGLPos.y(), 0, 'f', 2);
        // 计算文本位置（右上角，10px边距，兼容低版本Qt）
        int textWidth = painter.fontMetrics().width(posText); // 替代horizontalAdvance
        int textX = width - textWidth - 10;
        int textY = TEXT_SIZE + 10;
        painter.setPen(Qt::red);
        painter.drawText(textX, textY, posText);
    }

    painter.end();
    return image;
}

GLuint OscilloscopeWidget::createTextureFromQImage(const QImage& image) {
    QImage glImage = QGLWidget::convertToGLFormat(image);
    GLuint textureID;

    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    // 纹理参数
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // 上传纹理数据
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, glImage.width(), glImage.height(),
                 0, GL_RGBA, GL_UNSIGNED_BYTE, glImage.bits());

    return textureID;
}

void OscilloscopeWidget::drawGridTexture() {
    if (m_gridTextureID == 0) return;

    glColor3f(1.0f, 1.0f, 1.0f);
    glDisable(GL_LIGHTING);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, m_gridTextureID);

    // 切换到像素坐标投影
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, width(), 0, height(), -1, 1);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    // 绘制纹理四边形
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex2f(0, 0);
    glTexCoord2f(1.0f, 0.0f); glVertex2f(width(), 0);
    glTexCoord2f(1.0f, 1.0f); glVertex2f(width(), height());
    glTexCoord2f(0.0f, 1.0f); glVertex2f(0, height());
    glEnd();

    // 恢复矩阵
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);

    glDisable(GL_TEXTURE_2D);
}

void OscilloscopeWidget::drawWaveform() {
    if (m_waveformPoints.empty()) return;

    //int bufferSize = m_waveformPoints.size();

    glColor3f(0.0f, 1.0f, 0.0f);
    glLineWidth(2.0f);

    glBegin(GL_LINE_STRIP);
//    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    for (const auto& point : m_waveformPoints) {
        glVertex2f(point.x(), point.y());
    }
    glEnd();

    glLineWidth(1.0f);
}

void OscilloscopeWidget::initContextMenu()
{
    m_contextMenu = new QMenu(this);
    // 重置视图菜单项
    m_actResetView = new QAction("重置视图", this);
    //m_actResetView->setIconText(""); // 可选：添加图标
    connect(m_actResetView, &QAction::triggered, this, &OscilloscopeWidget::onResetView);

    // 鼠标位置显示菜单项
    m_actShowMousePosition = new QAction("鼠标追踪", this);
    m_actShowMousePosition->setCheckable(true);
    m_actShowMousePosition->setChecked(0);
    //m_actResetView->setIconText(""); // 可选：添加图标
    connect(m_actShowMousePosition, &QAction::triggered, this, &OscilloscopeWidget::onMousePosition);


    // 添加菜单项到菜单（可添加分隔符）
    m_contextMenu->addAction(m_actResetView);
    m_contextMenu->addAction(m_actShowMousePosition);
    //m_contextMenu->addSeparator(); // 分隔符

}

void OscilloscopeWidget::drawMousePosition()
{
    if (!m_isMousePosVisible) return;

    // 1. 绘制鼠标位置十字线（半透明红色）
    glColor4f(1.0f, 0.0f, 0.0f, 0.5f); // RGBA，最后一位是透明度
    glLineWidth(1.0f);

    // 竖线（X = 当前鼠标X）
    glBegin(GL_LINES);
    glVertex2f(m_currentMouseGLPos.x(), -m_viewRangeY - m_viewOffset.y());
    glVertex2f(m_currentMouseGLPos.x(), m_viewRangeY - m_viewOffset.y());
    // 横线（Y = 当前鼠标Y）
    glVertex2f(-m_viewRangeX + m_viewOffset.x(), m_currentMouseGLPos.y());
    glVertex2f(m_viewRangeX + m_viewOffset.x(), m_currentMouseGLPos.y());
    glEnd();


    return;//文本绘制放在纹理里用QPainter实现
    // 2. 绘制鼠标坐标文本（显示在右上角）
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, width(), 0, height(), -1, 1); // 切换到像素坐标

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    // 准备坐标文本
    QString posText = QString("鼠标位置: (X: %1, Y: %2)")
                          .arg(m_currentMouseGLPos.x(), 0, 'f', 2)
                          .arg(m_currentMouseGLPos.y(), 0, 'f', 2);

    // 使用Qt绘制文本（OpenGL混合模式保持启用）
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QFont font;
    font.setPointSize(TEXT_SIZE);
    painter.setFont(font);
    painter.setPen(Qt::red);
    // 文本绘制在右上角，留出10px边距（兼容低版本Qt）
    int textWidth = painter.fontMetrics().width(posText); // 替换horizontalAdvance
    int textX = width() - textWidth - 10;
    int textY = TEXT_SIZE + 10;
    painter.drawText(textX, textY, posText);
    painter.end();

    // 恢复矩阵
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

void OscilloscopeWidget::onTimer()
{
    if(timeOffset>=1000)
        timeOffset=0;
    // 配置正弦波参数
    const float xRange = 10.0f;    // X轴范围：-5 ~ 5（总长度10）
    const float amplitude = 3.0f;  // 振幅：3（Y轴范围-3 ~ 3，避免超出网格）
    const float frequency = 2.0f;  // 频率：2个完整周期
    m_waveformPoints.clear();
    // 生成1000个采样点
    for (int i = 0; i < numPoints; ++i) {
        // 1. 计算X坐标：均匀分布在[-5, 5]
        float x = -5.0f + (xRange * i) / (numPoints - 1);

        // 2. 计算正弦波Y坐标：y = A * sin(2π * f * x)
        float y = amplitude * sin(2 * M_PI * frequency * (x / xRange)) + rand()%10 *0.1;

        // 3. 添加到点集
        m_waveformPoints.emplace_back(x, y);
    }
    timeOffset++;
    updateGL();
}

void OscilloscopeWidget::onResetView()
{
    // 恢复初始视图参数
    m_viewOffset = QPointF(0, 0);
    m_viewRangeX = BASE_X_RANGE;
    m_viewRangeY = BASE_Y_RANGE;
    m_zoom_x = 1.0f;
    m_zoom_y = 1.0f;

    // 重新生成纹理并刷新
    markTextureDirty();
    updateGL();
}

void OscilloscopeWidget::onMousePosition(bool isShown)
{
    m_isMousePosVisible = isShown;
    if(!isShown)
        markTextureDirty();
}

void OscilloscopeWidget::setWaveformPoints(const WaveformPoints& points) {
    m_waveformPoints = points;
    updateGL();
}

void OscilloscopeWidget::clearWaveform() {
    m_waveformPoints.clear();
    updateGL();
}

// ---------------- 鼠标交互实现 ----------------
void OscilloscopeWidget::mousePressEvent(QMouseEvent *e) {
    m_lastMousePos = e->pos();
}

void OscilloscopeWidget::mouseMoveEvent(QMouseEvent *e) {
    if (e->buttons() & Qt::LeftButton) {
        // 计算鼠标移动的像素差
        int dx = e->x() - m_lastMousePos.x();
        int dy = e->y() - m_lastMousePos.y();

        // 转换为坐标偏移（反向，因为鼠标移动方向与视图偏移相反）
        float xOffset = -dx * (2 * m_viewRangeX) / width();
        float yOffset = -dy * (2 * m_viewRangeY) / height();

        // 更新视图偏移
        m_viewOffset.setX(m_viewOffset.x() + xOffset);
        m_viewOffset.setY(m_viewOffset.y() + yOffset);

        m_lastMousePos = e->pos();
        markTextureDirty(); // 平移后需要更新刻度
        updateGL();
    }
    // 新增：计算鼠标对应的OpenGL坐标（无论是否按住左键）
    if (m_isMousePosVisible) {
        // 像素坐标转OpenGL坐标
        float x = (e->x() / (float)width()) * 2 * m_viewRangeX - m_viewRangeX + m_viewOffset.x();
        float y = m_viewRangeY - (e->y() / (float)height()) * 2 * m_viewRangeY - m_viewOffset.y();
        m_currentMouseGLPos = QPointF(x, y);
        markTextureDirty();
        updateGL(); // 刷新显示鼠标位置
    }
}

void OscilloscopeWidget::wheelEvent(QWheelEvent *e) {
    // 1. 判断是否按下Ctrl键，区分X/Y轴缩放
        bool isCtrlPressed = (e->modifiers() & Qt::ControlModifier);
        // 滚轮每步缩放10%（向上缩小范围=放大波形，向下扩大范围=缩小波形）
        float scaleFactor = e->angleDelta().y() > 0 ? 0.9f : 1.1f;

        // 限制缩放范围
        scaleFactor = (scaleFactor < 0.1f) ? 0.1f : (scaleFactor > 50.0f ? 50.0f : scaleFactor);

        // 2. 转换鼠标位置到当前坐标系统（用于中心缩放）
        QPoint mousePos = e->pos();
        float mouseX = (mousePos.x() / (float)width()) * 2 * m_viewRangeX - m_viewRangeX + m_viewOffset.x();
        float mouseY = m_viewRangeY - (mousePos.y() / (float)height()) * 2 * m_viewRangeY - m_viewOffset.y();

        // 3. 独立缩放XY轴
        if (!isCtrlPressed) {
            // 普通滚轮：仅缩放X轴
            m_zoom_x *= scaleFactor;
            float oldViewRangeX = m_viewRangeX;
            m_viewRangeX *= scaleFactor;

            // 调整X轴偏移，保证鼠标位置的X坐标不变（中心缩放）
            m_viewOffset.setX(mouseX - (mouseX - m_viewOffset.x()) * (m_viewRangeX / oldViewRangeX));

        } else {
            // Ctrl+滚轮：仅缩放Y轴
            m_zoom_y *= scaleFactor;
            float oldViewRangeY = m_viewRangeY;
            m_viewRangeY *= scaleFactor;

            // 调整Y轴偏移，保证鼠标位置的Y坐标不变（中心缩放）
            m_viewOffset.setY(mouseY - (mouseY - m_viewOffset.y()) * (m_viewRangeY / oldViewRangeY));

        }

        // 4. 标记纹理需要更新，触发刻度重绘
        markTextureDirty();
        updateGL();
}

void OscilloscopeWidget::contextMenuEvent(QContextMenuEvent *e)
{
    // 在鼠标右键位置弹出菜单
    m_contextMenu->exec(e->globalPos());
    e->accept();
}
