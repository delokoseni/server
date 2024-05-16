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
        if(parts.isEmpty()) return; // Если сообщение пустое, то ничего не делаем

        QString command = parts.first();

        if(command == "register" || command == "login") {
            if(parts.count() < 3) return; // Для регистрации и входа нужно минимум 3 части
            QString username = parts.at(1);
            QString password = parts.at(2);

            if(command == "register") {
                processRegistration(clientSocket, username, password);
            } else { // Здесь else, так как команда может быть только "login"
                processLogin(clientSocket, username, password);
            }
        } else if (command == "search") {
            if(parts.count() < 2) return; // Для поиска нужно минимум 2 части
            QString searchText = parts.at(1);
            processSearchRequest(clientSocket, searchText);
        } else
            if (command == "create_chat" && parts.length() == 3) {
                    QString chatName = parts.at(1);
                    QString chatType = parts.at(2);
                    int chatId = createChat(chatName, chatType);
                    if (chatId != -1) {
                        stream << "create_chat:success:" << chatId << '\n';
                        //addUserToChat(chatId, user); два запроса айди
                        //addUserToChat(chatId, user);
                    } else {
                        stream << "create_chat:fail\n";
                    }
                    stream.flush();
                }
    });
}

int Server::createChat(const QString& chatName, const QString& chatType) {
    QSqlQuery query;
    qDebug() << "chatName: " << chatName << " chatType: " << chatType << "\n";
    query.prepare("INSERT INTO chats (chat_name, chat_type) VALUES (:chat_name, :chat_type)");
    query.bindValue(":chat_name", chatName);
    query.bindValue(":chat_type", chatType);
    if (!query.exec()) {
        qCritical() << "Failed to create chat:" << query.lastError().text();
        return -1;
    }
    return query.lastInsertId().toInt();
}

void Server::addUserToChat(const int chatId, const int userId) {
    QSqlQuery query;
    query.prepare("INSERT INTO chat_participants (chat_id, user_id) VALUES (:chat_id, :user_id)");
    query.bindValue(":chat_id", chatId);
    query.bindValue(":user_id", userId);
    if (!query.exec()) {
        qCritical() << "Failed to add user to chat:" << query.lastError().text();
    }
}


void Server::processSearchRequest(QTcpSocket* clientSocket, const QString& searchText) {
    QSqlQuery query;
    query.prepare("SELECT login FROM user_auth WHERE login LIKE :searchText");
    query.bindValue(":searchText", "%" + searchText + "%");
    if (query.exec()) {
        QTextStream stream(clientSocket);
        qDebug() << "Search query successful. Results for:" << searchText;
        while (query.next()) {
            QString username = query.value(0).toString();
            stream << "search_result:" << username << '\n'; // Send each result to the client
            stream.flush();
            qDebug() << "Found user:" << username; // Logging each found username
        }
        stream << "search_end\n"; // Signal the end of search results
        stream.flush();
    } else {
        qCritical() << "Search query failed:" << query.lastError().text();
    }
}
