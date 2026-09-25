#pragma once

#include <QEvent>
#include <QMouseEvent>
#include <QToolTip>
#include <QCursor>
#include <QTimer>

class TooltipClickFilter : public QObject {

public:
    explicit TooltipClickFilter(QObject* parent = nullptr) : QObject(parent) {}

protected:
    bool eventFilter(QObject* obj, QEvent* event) override {

        // Intercept mouse release events
        if (event->type() == QEvent::MouseButtonRelease) {
            if (QWidget* widget = qobject_cast<QWidget*>(obj)) {

                // Manually show the tooltip at the exact mouse cursor position
                QToolTip::showText(QCursor::pos(), widget->toolTip(), widget);

                return true; // Consume the event
            }
        }

        if (event->type() == QEvent::MouseButtonPress ||
            event->type() == QEvent::MouseButtonDblClick) {
            return true;
        }

        // Let all other events pass through normally
        return QObject::eventFilter(obj, event);
    }

};