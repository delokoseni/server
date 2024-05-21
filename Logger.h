#ifndef LOGGER_H
#define LOGGER_H

#include <QDebug>
#include <QString>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QDateTime>

class Logger
{
private:
    static Logger* instance;
    QFile logFile;
    Logger();

public:
    static Logger* getInstance();
    void logToFile(const QString &message);
    void setLogFile(const QString &filename);
};

#endif // LOGGER_H
