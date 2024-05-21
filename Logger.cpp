#include "Logger.h"

Logger* Logger::instance = nullptr;

Logger::Logger() { setLogFile(QDir::homePath() + "/default_log.txt"); }

Logger* Logger::getInstance()
{
    if (instance == nullptr)
    {
        instance = new Logger();
    }
    return instance;
}

void Logger::logToFile(const QString &message)
{
    if (logFile.isOpen())
    {
        QTextStream stream(&logFile);
        QString timeStamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
        stream << timeStamp << " " << message << "\n"; // Добавляем временную метку к сообщению
    } else
    {
        qDebug() << "LogFile is not open. Message: " << message;
    }
}

void Logger::setLogFile(const QString &filename)
{
    if (logFile.isOpen())
    {
        logFile.close();
    }
    logFile.setFileName(filename);
    if(!logFile.open(QFile::WriteOnly | QFile::Append))
    {
        qDebug() << "Failed to open log file:" << filename;
    }
}
