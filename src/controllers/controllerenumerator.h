#pragma once

#include <QList>
#include <QObject>
#include <QStringList>

class Controller;

/// Base class handling discovery and enumeration of DJ controllers.
///
/// This class handles discovery and enumeration of DJ controllers and
/// must be inherited by a class that implements it on some API.
class ControllerEnumerator : public QObject {
    Q_OBJECT
  public:
    ControllerEnumerator();
    // In this function, the inheriting class must delete the Controllers it
    // creates
    virtual ~ControllerEnumerator();

    virtual QList<Controller*> queryDevices() = 0;

    /// Must not touch the Controller instances returned by queryDevices().
    virtual QStringList getConnectedDeviceIds() {
        return {};
    }
};
