#ifndef SRC_QMLTYPES_HOVEREVENTLISTENER_H
#define SRC_QMLTYPES_HOVEREVENTLISTENER_H

#include <QObject>

class QWidget;

class HoverEventListener : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool listen READ listen WRITE setListen)
    Q_PROPERTY(bool hasExited READ hasExited NOTIFY hasExitedChanged)
    Q_PROPERTY(QWidget* widget READ widget WRITE setWidget)
public:
    HoverEventListener(QObject * parent = 0);

    void setListen(bool listen);
    bool listen() const;
    bool hasExited() const;

    void setWidget(QWidget * widget);
    QWidget * widget() const;

signals:
    void exited();
    void hasExitedChanged();

private:
    bool eventFilter(QObject* target, QEvent* event);

    bool m_listen;
    bool m_hasExited;
    QWidget * m_widget;
};

#endif
