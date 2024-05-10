#ifndef SERVER_H
#define SERVER_H

#include <QTcpServer>
#include <QTcpSocket>
#include <QCoreApplication>
#include <QTextStream>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDir>

class Server : public QTcpServer {
    Q_OBJECT

public:
    Server(QObject *parent = nullptr);
    bool isLoginFree(const QString& username);
    void addUserToDatabase(const QString& username, const QString& password);
    void startServer(int port);
    bool validateUser(const QString& username, const QString& password);
    void processRegistration(QTcpSocket* clientSocket, const QString& username, const QString& password);
    void processLogin(QTcpSocket* clientSocket, const QString& username, const QString& password);

public slots:
    void onNewConnection();
    void processSearchRequest(QTcpSocket* clientSocket, const QString& searchText);

};

#endif // SERVER_H
