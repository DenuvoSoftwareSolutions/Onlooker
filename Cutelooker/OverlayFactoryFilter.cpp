#include "OverlayFactoryFilter.h"

#include <QPainter>
#include <QEvent>
#include <QMouseEvent>

// https://stackoverflow.com/q/29294905/1806760
class Overlay : public QWidget
{
public:
    explicit Overlay(const QColor& color, QWidget* parent = nullptr) :
        QWidget(parent),
        m_color(color)
    {
        setAttribute(Qt::WA_NoSystemBackground);
        setAttribute(Qt::WA_TransparentForMouseEvents);
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter(this).fillRect(rect(), m_color);
    }

private:
    QColor m_color;
};

OverlayFactoryFilter::OverlayFactoryFilter(QObject* parent) : QObject(parent)
{
    m_overlay = new Overlay(QColor(80, 80, 255, 128));
}

void OverlayFactoryFilter::moveOverlay(QWidget* parent, int newX)
{
    m_overlay->setParent(parent);
    m_overlay->resize(3, parent->height());
    m_overlay->move(newX - 1, 0);
    m_overlay->show();
    emit cursorChanged(m_overlay->pos());
}

void OverlayFactoryFilter::hideOverlay()
{
    m_overlay->hide();
    m_overlay->setParent(nullptr);
}

bool OverlayFactoryFilter::eventFilter(QObject* obj, QEvent* ev)
{
    if (!obj->isWidgetType())
        return false;
    auto w = static_cast<QWidget*>(obj);
    if(ev->type() == QEvent::MouseButtonRelease)
    {
        m_isDragging = false;
    }
    else if(ev->type() == QEvent::MouseButtonPress)
    {
        m_isDragging = true;

        auto me = static_cast<QMouseEvent*>(ev);
        moveOverlay(w, me->pos().x());
    }
    else if (ev->type() == QEvent::MouseMove)
    {
        if(m_isDragging)
        {
            auto me = static_cast<QMouseEvent*>(ev);
            moveOverlay(w, me->pos().x());
        }
    }
    else if(ev->type() == QEvent::KeyRelease)
    {
        auto ke = static_cast<QKeyEvent*>(ev);
        auto diffX = 0;
        if(ke->modifiers() == 0)
        {
            if(ke->key() == Qt::Key_Left)
            {
                diffX = -1;
            }
            else if(ke->key() == Qt::Key_Right)
            {
                diffX = 1;
            }
        }
        if(diffX != 0 && m_overlay->parent() == w)
        {
            m_overlay->move(m_overlay->x() + diffX, m_overlay->y());
            emit cursorChanged(m_overlay->pos());
        }
    }
    else if (ev->type() == QEvent::Resize)
    {
        if(m_overlay->parentWidget() == w)
            m_overlay->hide();
    }
    else if(ev->type() == QEvent::FocusOut)
    {
        m_isDragging = false;
    }
    return false;
}
