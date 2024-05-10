#include "Server.h"

Server::Server(QObject *parent) : QTcpServer(parent) {
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(QDir::homePath() + "/messenger.db");

    if (!db.open())
    {
        qCritical() << "Could not connect to database:" << db.lastError().text();
        exit(1);
    }

    connect(this, &Server::newConnection, this, &Server::onNewConnection);
}

bool Server::isLoginFree(const QString& username) {
    QSqlQuery query;
    query.prepare("SELECT COUNT(*) FROM user_auth WHERE login = :login");
    query.bindValue(":login", username);
    if (!query.exec())
    {
        qCritical() << "Failed to check if login is free:" << query.lastError().text();
        return false;
    }
    if (query.next())
    {
        return query.value(0).toInt() == 0;
    }
    return false;
}

void Server::addUserToDatabase(const QString& username, const QString& password) {
    QSqlQuery query;
    query.prepare("INSERT INTO user_auth (login, password) VALUES (:login, :password)");
    query.bindValue(":login", username);
    query.bindValue(":password", password);
    if (!query.exec()) {
        qCritical() << "Failed to add user to database:" << query.lastError().text();
    } else {
        qDebug() << "User" << username << "successfully added.";
    }
}

void Server::startServer(int port) {
    if (!this->listen(QHostAddress::Any, port)) {
        qCritical() << "Could not start server";
    } else {
        qDebug() << "Server started on port" << port;
    }
}

bool Server::validateUser(const QString& username, const QString& password) {
    QSqlQuery query;
    query.prepare("SELECT password FROM user_auth WHERE login = :login");
    query.bindValue(":login", username);
    if (!query.exec()) {
        qCritical() << "Failed to check user credentials:" << query.lastError().text();
        return false;
    }
    if (query.next()) {
        QString storedPassword = query.value(0).toString();
        return password == storedPassword;
    }
    return false;
}

void Server::processRegistration(QTcpSocket* clientSocket, const QString& username, const QString& password) {
    if(isLoginFree(username)) {
        addUserToDatabase(username, password); // Добавляем пользователя в базу
        QTextStream stream(clientSocket);
        stream << "register:success\n";
        stream.flush(); // Гарантируем отправку сообщения
    } else {
        QTextStream stream(clientSocket);
        stream << "register:fail:username taken\n";
        stream.flush(); // Гарантируем отправку сообщения
        qDebug("register:fail:username taken\n");
    }
}

void Server::processLogin(QTcpSocket* clientSocket, const QString& username, const QString& password) {
    if(validateUser(username, password)) {
        QTextStream stream(clientSocket);
        stream << "login:success\n";
        stream.flush(); // Гарантируем отправку сообщения
    } else {
        QTextStream stream(clientSocket);
        stream << "login:fail\n";
        stream.flush(); // Гарантируем отправку сообщения
    }
}

void Server::onNewConnection() {
    QTcpSocket *clientSocket = this->nextPendingConnection();
    connect(clientSocket, &QTcpSocket::readyRead, this, [this, clientSocket]() {
        QTextStream stream(clientSocket);
        QString message = stream.readAll().trimmed();
        qDebug() << "New message received:" << message;

        QStringList parts = message.split(":");
        if(parts.count() >= 3) {
            QString command = parts.first();
            QString username = parts.at(1);
            QString password = parts.at(2);

            if(command == "register") {
                processRegistration(clientSocket, username, password);
            }
            if (command == "login") {
                processLogin(clientSocket, username, password);
            }
            if (command == "search") {
                    QString searchText = parts.at(1);
                    processSearchRequest(clientSocket, searchText);
            }
        }
    });
}

void Server::processSearchRequest(QTcpSocket* clientSocket, const QString& searchText) {
    QSqlQuery query;
    query.prepare("SELECT login FROM user_auth WHERE login LIKE :searchText");
    query.bindValue(":searchText", searchText + "%");
    if (query.exec()) {
        QTextStream stream(clientSocket);
        while (query.next()) {
            QString username = query.value(0).toString();
            stream << "search_result:" << username << '\n'; // Для каждой строки отправляем результат клиенту
        }
        stream << "search_end\n"; // Сигнал о конце результатов поиска
        stream.flush();
    } else {
        qCritical() << "Search query failed:" << query.lastError();
    }
}
