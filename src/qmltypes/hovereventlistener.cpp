#include "hovereventlistener.h"

#include <QCoreApplication>
#include <QWidget>
#include <QDebug>
#include <QMouseEvent>

HoverEventListener::HoverEventListener(QObject *parent)
    : QObject(parent)
    , m_listen(false)
    , m_widget(0)
{
}

void HoverEventListener::setWidget(QWidget * widget)
{
    m_widget = widget;
}

QWidget * HoverEventListener::widget() const
{
    return m_widget;
}

void HoverEventListener::setListen(bool listen)
{
    if (m_listen == listen)
        return;

    if (listen) {
        qApp->installEventFilter(this);
        m_hasExited = false;
        emit hasExitedChanged();
    } else {
        qApp->removeEventFilter(this);
    }

    m_listen = listen;
}

bool HoverEventListener::eventFilter(QObject* target, QEvent* event)
{
    Q_UNUSED(target);
    if (m_widget && event->type() == QEvent::MouseMove) {
        if (!m_widget->rect().contains(m_widget->mapFromGlobal(static_cast<QMouseEvent*>(event)->screenPos().toPoint()))) {
            m_hasExited = true;
            emit hasExitedChanged();
            emit exited();
        }
    }
    else if (event->type() == QEvent::HoverLeave) {
        m_hasExited = true;
        emit exited();
        emit hasExitedChanged();
    }
    return false;
}

bool HoverEventListener::listen() const
{
    return m_listen;
}

bool HoverEventListener::hasExited() const
{
    return m_hasExited;
}
