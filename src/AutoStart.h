#pragma once
#include <QString>

class AutoStart {
public:
    static bool isEnabled();
    static bool enable(const QString& appName, const QString& appPath);
    static bool disable(const QString& appName);
    static bool setEnabled(const QString& appName, const QString& appPath, bool on);
};
