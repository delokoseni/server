#include "Server.h"
#include "Logger.h"

Server::Server(QObject *parent) : QTcpServer(parent) {
    window = new QWidget();
    window->resize(window_width, window_height);
    statusLabel = new QLabel("Сервер работает.");
    statusLabel->setAlignment(Qt::AlignCenter);  // Выравнивание текста по центру
    statusLabel->setStyleSheet("QLabel { color : green; }");
    logViewer = new QPlainTextEdit();
    logViewer->setReadOnly(true);
    logFileButton = new QPushButton("Выбрать файл для логгирования");
    layout = new QFormLayout(window);

    layout->addRow(statusLabel);
    layout->addWidget(logViewer);
    layout->addWidget(logFileButton);

    window->setLayout(layout);
    window->setWindowTitle("Сервер");
    window->show();

    connect(logFileButton, &QPushButton::clicked, this, &Server::selectLogFile);

    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(QDir::homePath() + "/messenger.db");

    if (!db.open())
    {
        qCritical() << "Could not connect to database:" << db.lastError().text();
        exit(1);
    }

    connect(this, &Server::newConnection, this, &Server::onNewConnection);
    Logger::getInstance()->logToFile("Server is running");

    logUpdateTimer = new QTimer(this);
    connect(logUpdateTimer, &QTimer::timeout, this, &Server::updateLogViewer);
    logUpdateTimer->start(1000);
}

Server::~Server() {
    Logger::getInstance()->logToFile("Server is turned off");
    if (window) {
        delete window;
    }
}

void Server::selectLogFile() {
    QString filename = QFileDialog::getOpenFileName(window, tr("Открыть файл"), QDir::homePath(), tr("Log Files (*.txt)"));
    if(!filename.isEmpty()) {
        Logger::getInstance()->setLogFile(filename);
    }
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
        Logger::getInstance()->logToFile("Registered user " + username);
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
        Logger::getInstance()->logToFile("User " + username + " is logged in");
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
                if(command == "register")
                {
                    processRegistration(clientSocket, username, password);
                }
                else
                { // Здесь else, так как команда может быть только "login"
                    processLogin(clientSocket, username, password);
                }
            }
            else if (command == "search")
            {
                if(parts.count() < 2) return; // Для поиска нужно минимум 2 части
                QString searchText = parts.at(1);
                processSearchRequest(clientSocket, searchText);
            }
            else if (command == "create_chat") {
                QString chatName = parts.at(1);
                QString chatType = parts.at(2);
                QString userName1 = parts.at(3);
                QString userName2 = parts.at(4);
                int chatId = createChat(chatName, chatType, userName1, userName2);
                if (chatId != -1)
                {
                    stream << "create_chat:success:" << chatId << '\n';
                    addUserToChat(chatId, findUserID(userName1));
                    addUserToChat(chatId, findUserID(userName2));
                }
                else
                {
                    stream << "create_chat:fail\n";
                }
                stream.flush();
            } else if (command == "get_chats")
            {
                if(parts.count() < 2) return;
                QString username = parts.at(1);
                int userId = findUserID(username);
                if (userId != -1)
                {
                    getChatsForUser(clientSocket, userId);
                }
            } else if (command == "send_message")
            {
                if(parts.count() < 4) return; // Нужно минимум 4 части для отправки сообщения
                int chatId = parts.at(1).toInt();
                int userId = parts.at(2).toInt();
                QString messageText = parts.at(3);
                QSqlQuery query;
                query.prepare("INSERT INTO messages (chat_id, user_id, message_text) VALUES (:chatId, :userId, :messageText)");
                query.bindValue(":chatId", chatId);
                query.bindValue(":userId", userId);
                query.bindValue(":messageText", messageText);
                if (query.exec())
                {
                    stream << "send_message:success\n";
                    QString logMessage = QString("User with ID %1 sent a message to chat with ID %2.\nMessage: %3")
                        .arg(QString::number(userId))
                        .arg(QString::number(chatId))
                        .arg(messageText);
                    Logger::getInstance()->logToFile(logMessage);
                }
                else
                {
                    stream << "send_message:fail:" << query.lastError().text() << "\n";
                }
                stream.flush();
            }
            else if (command == "get_messages")
            {
                if(parts.count() < 2) return;
                int chatId = parts.at(1).toInt();
                getMessagesForChat(clientSocket, chatId);
            }
            else if (command == "get_user_id")
            {
                if(parts.count() < 2) return;
                QString login = parts.at(1);
                getUserId(clientSocket, login);
            }
    });
}

void Server::getChatsForUser(QTcpSocket* clientSocket, int userId) {
    QSqlQuery query;
    query.prepare("SELECT ua.login, c.chat_id FROM user_auth ua "
                  "JOIN chat_participants cp ON cp.user_id = ua.user_id "
                  "JOIN chats c ON c.chat_id = cp.chat_id "
                  "WHERE c.chat_id IN (SELECT chat_id FROM chat_participants WHERE user_id = :user_id) "
                  "AND ua.user_id != :user_id");
    query.bindValue(":user_id", userId);
    if (query.exec()) {
        QTextStream stream(clientSocket);
        while (query.next()) {
            QString username = query.value(0).toString();
            QString chatId = query.value(1).toString();
            stream << "chat_list_item:" << chatId << ":" << username << '\n'; // Отправляем каждый результат клиенту
        }
        stream.flush();
    } else {
        qCritical() << "Failed to get chats for user:" << query.lastError().text();
    }
}

void Server::getUserId(QTcpSocket* clientSocket, const QString& login) {
    QSqlQuery query;
    query.prepare("SELECT user_id FROM user_auth WHERE login = :login");
    query.bindValue(":login", login);
    if (query.exec() && query.next()) {
        int userId = query.value(0).toInt();
        QTextStream stream(clientSocket);
        stream << "user_id:" << userId << "\n";
        stream.flush();
        qDebug() << "UserID = " << userId << "\n";
    } else {
        qCritical() << "Failed to get user ID for login:" << query.lastError().text();
    }
}

int Server::createChat(const QString& chatName, const QString& chatType, const QString& userName1, const QString& userName2) {
    QSqlQuery query;
    int userId1 = findUserID(userName1);
    int userId2 = findUserID(userName2);

    if (userId1 == -1 || userId2 == -1) {
        qCritical() << "One of the users does not exist";
        return -1;
    }

    if (chatExistsBetweenUsers(userId1, userId2)) {
        qCritical() << "Chat between these users already exists";
        return -1;
    }
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

int Server::findUserID(const QString& userName)
{
    QSqlQuery query;
    query.prepare("SELECT user_id FROM user_auth WHERE login = :userName");
    query.bindValue(":userName", userName);
    if (!query.exec())
    {
        qCritical() << "Failed to find user_id:" << query.lastError().text();
        return -1;
    }
    else
    {
        if (query.next()) // Перемещаем курсор на следующую запись в результатах запроса.
        {
            return query.value(0).toInt(); // Теперь мы можем безопасно получить значение.
        }
        else
        {
            qCritical() << "User not found";
            return -1;
        }
    }
}

bool Server::chatExistsBetweenUsers(const int userId1, const int userId2) {
    QSqlQuery query;
    query.prepare("SELECT chat_id FROM chat_participants WHERE user_id = :userId1 "
                  "INTERSECT "
                  "SELECT chat_id FROM chat_participants WHERE user_id = :userId2");
    query.bindValue(":userId1", userId1);
    query.bindValue(":userId2", userId2);
    if (!query.exec() || !query.next()) {
        return false; // Чата нет, можно создать новый
    } else {
        return true; // Чат между пользователями уже существует
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

void Server::getMessagesForChat(QTcpSocket* clientSocket, int chatId) {
    QSqlQuery query;
    // Добавляем выборку user_id сообщения в запрос
    query.prepare("SELECT user_id, message_text FROM messages WHERE chat_id = :chatId ORDER BY timestamp_sent ASC");
    query.bindValue(":chatId", chatId);
    if (query.exec()) {
        QTextStream stream(clientSocket);
        while (query.next()) {
            int senderId = query.value(0).toInt(); // ID пользователя отправившего сообщение
            QString message = query.value(1).toString();
            // Добавляем user_id в конец каждого сообщения
            stream << "message_item:" << message << ":" << senderId << '\n'; // 'user_id' в конце
        }
        stream << "end_of_messages\n"; // Отправляем сигнал конца передачи сообщений
        stream.flush();
    } else {
        qCritical() << "Failed to get messages for chat:" << query.lastError().text();
    }
}

void Server::updateLogViewer() {
    QFile logFile(currentLogFilePath);
    if (logFile.open(QIODevice::ReadOnly)) {
        QTextStream stream(&logFile);
        logViewer->setPlainText(stream.readAll());
        logFile.close();
        QScrollBar *scrollBar = logViewer->verticalScrollBar();
        scrollBar->setValue(scrollBar->maximum());
    }
}
