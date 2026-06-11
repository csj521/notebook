#include "AutoStart.h"
#include <QSettings>

bool AutoStart::isEnabled() {
    QSettings reg("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                  QSettings::NativeFormat);
    return reg.contains("Notebook") && !reg.value("Notebook").toString().isEmpty();
}

bool AutoStart::enable(const QString&, const QString& appPath) {
    QSettings reg("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                  QSettings::NativeFormat);
    reg.setValue("Notebook", appPath);
    reg.sync();
    return reg.status() == QSettings::NoError;
}

bool AutoStart::disable(const QString&) {
    QSettings reg("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                  QSettings::NativeFormat);
    reg.remove("Notebook");
    reg.sync();
    return reg.status() == QSettings::NoError;
}

bool AutoStart::setEnabled(const QString& name, const QString& path, bool on) {
    return on ? enable(name, path) : disable(name);
}
