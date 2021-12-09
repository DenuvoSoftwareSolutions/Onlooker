#pragma once

#include <QWidget>
#include <QPointer>

// https://stackoverflow.com/q/29294905/1806760
class Overlay;

class OverlayFactoryFilter : public QObject
{
    Q_OBJECT
public:
    explicit OverlayFactoryFilter(QObject* parent = nullptr);
    void moveOverlay(QWidget* parent, int newX);
    void hideOverlay();

signals:
    void cursorChanged(QPoint pos);

protected:
    bool eventFilter(QObject* obj, QEvent* ev) override;

private:
    QPointer<Overlay> m_overlay;
    bool m_isDragging = false;
};
