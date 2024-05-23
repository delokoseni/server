#include "Server.h"
#include "Logger.h"

#include <QFileDialog>
#include <QTextStream>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QScrollBar>
#include <QMessageBox>

Server::Server(QObject *parent) : QTcpServer(parent)
{
    setupUI();
    connectSignals();
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(QDir::homePath() + "/messenger.db");
    if (!db.open())
    {
        qCritical() << "Could not connect to database:" << db.lastError().text();
        exit(1);
    }
    Logger::getInstance()->logToFile("Server is running");
    logUpdateTimer->start(1000);
}

void Server::setupUI()
{
    window = new QWidget();
    window->resize(window_width, window_height);
    logFileNameLabel = new QLabel(tr("Файл логов: %1").arg(QFileInfo(currentLogFilePath).fileName()));
    logFileNameLabel->setAlignment(Qt::AlignRight);
    statusLabel = new QLabel("Сервер работает.");
    statusLabel->setAlignment(Qt::AlignLeft);
    statusLabel->setStyleSheet("QLabel { color : green; }");
    logViewer = new QPlainTextEdit();
    logViewer->setReadOnly(true);
    logFileButton = new QPushButton("Выбрать файл для логгирования");
    layout = new QVBoxLayout(window);
    headerLayout = new QHBoxLayout();
    headerLayout->addWidget(logFileNameLabel);
    headerLayout->addStretch();
    headerLayout->addWidget(statusLabel);
    layout->addLayout(headerLayout);
    layout->addWidget(logViewer);
    layout->addWidget(logFileButton);
    window->setLayout(layout);
    window->setWindowTitle("Сервер");
    window->show();
    logUpdateTimer = new QTimer(this);
}

void Server::connectSignals()
{
    connect(logFileButton, &QPushButton::clicked, this, &Server::selectLogFile);
    connect(this, &Server::newConnection, this, &Server::onNewConnection);
    connect(logUpdateTimer, &QTimer::timeout, this, &Server::updateLogViewer);
}

Server::~Server()
{
    Logger::getInstance()->logToFile("Server is turned off");
    for (QTcpSocket* socket : qAsConst(userSockets))
    {
        socket->close();
        socket->deleteLater();
    }
    if (isListening())
    {
            close();
    }
    if (window)
    {
        delete window;
    }
}

bool Server::isLoginFree(const QString& username)
{
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

void Server::addUserToDatabase(const QString& username, const QString& password)
{
    QSqlQuery query;
    query.prepare("INSERT INTO user_auth (login, password) VALUES (:login, :password)");
    query.bindValue(":login", username);
    query.bindValue(":password", password);
    if (!query.exec())
    {
        qCritical() << "Failed to add user to database:" << query.lastError().text();
    } else
    {
        qDebug() << "User" << username << "successfully added.";
    }
}

void Server::startServer(int port)
{
    if (!this->listen(QHostAddress::Any, port))
    {
        qCritical() << "Could not start server";
    }
    else
    {
        qDebug() << "Server started on port" << port;
    }
}

bool Server::validateUser(const QString& username, const QString& password)
{
    QSqlQuery query;
    query.prepare("SELECT password FROM user_auth WHERE login = :login");
    query.bindValue(":login", username);
    if (!query.exec())
    {
        qCritical() << "Failed to check user credentials:" << query.lastError().text();
        return false;
    }
    if (query.next())
    {
        QString storedPassword = query.value(0).toString();
        return password == storedPassword;
    }
    return false;
}

void Server::processRegistration(QTcpSocket* clientSocket, const QString& username, const QString& password)
{
    if(isLoginFree(username))
    {
        addUserToDatabase(username, password);
        QTextStream stream(clientSocket);
        stream << "register:success\n";
        stream.flush();
        Logger::getInstance()->logToFile("Registered user " + username);
    }
    else
    {
        QTextStream stream(clientSocket);
        stream << "register:fail:username taken\n";
        stream.flush();
        qDebug("register:fail:username taken\n");
    }
}

void Server::processLogin(QTcpSocket* clientSocket, const QString& username, const QString& password)
{
    if(validateUser(username, password))
    {
        qDebug() << "user socket " << clientSocket << "\n";
        userSockets.insert(getUserID(username), clientSocket);
        QTextStream stream(clientSocket);
        stream << "login:success\n";
        stream.flush();
        connect(clientSocket, &QTcpSocket::disconnected, this, &Server::onClientDisconnected);
        Logger::getInstance()->logToFile("User " + username + " is logged in");
    }
    else
    {
        QTextStream stream(clientSocket);
        stream << "login:fail\n";
        stream.flush();
    }
}

void Server::onClientDisconnected()
{
    QTcpSocket *clientSocket = qobject_cast<QTcpSocket*>(sender());
    if (clientSocket)
    {
        int userId = 0;
        auto it = userSockets.begin();
        while (it != userSockets.end()) {
            if (it.value() == clientSocket)
            {
                userId = it.key();
                it = userSockets.erase(it);
                break;
            }
            else
            {
                it++;
            }
        }
        QString logMessage = QString("User with ID %1 disconnected").arg(QString::number(userId));
        Logger::getInstance()->logToFile(logMessage);
        clientSocket->deleteLater();
    }
}

void Server::onNewConnection()
{
    QTcpSocket *clientSocket = this->nextPendingConnection();
    connect(clientSocket, &QTcpSocket::readyRead, this, [this, clientSocket]()
    {
        QTextStream stream(clientSocket);
        QString message = stream.readAll().trimmed();
        QStringList smallMessage = message.trimmed().split("\n", QString::SkipEmptyParts);
        for(const QString &line : smallMessage)
        {
            qDebug() << "New message received:" << line;
            QStringList parts = line.split(":");
            if(parts.isEmpty()) return;
            QString command = parts.first();
            if(command == "register" || command == "login")
            {
                if(parts.count() < 3) return;
                QString username = parts.at(1);
                QString password = parts.at(2);
                if(command == "register")
                {
                    processRegistration(clientSocket, username, password);
                }
                else
                {
                    processLogin(clientSocket, username, password);
                }
            }
            else if (command == "search")
            {
                if(parts.count() < 3) return;
                QString searchText = parts.at(1);
                QString currentUserLogin = parts.at(2);
                processSearchRequest(clientSocket, searchText, currentUserLogin);
            }
            else if (command == "create_chat")
            {
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
            }
            else if (command == "get_chats")
            {
                if(parts.count() < 2) return;
                QString username = parts.at(1);
                int userId = findUserID(username);
                if (userId != -1)
                {
                    getChatsForUser(clientSocket, userId);
                }
            }
            else if (command == "send_message")
            {
                if(parts.count() < 4) return;
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
                    QSqlQuery participantsQuery;
                    participantsQuery.prepare("SELECT user_id FROM chat_participants WHERE chat_id = :chatId AND user_id != :senderId");
                    participantsQuery.bindValue(":chatId", chatId);
                    participantsQuery.bindValue(":senderId", userId);
                    if (participantsQuery.exec())
                    {
                        while (participantsQuery.next())
                        {
                            int participantId = participantsQuery.value(0).toInt();

                            if (userSockets.contains(participantId))
                            {
                                QTcpSocket* recipientSocket = userSockets[participantId];
                                QTextStream recipientStream(recipientSocket);

                                recipientStream << "new_message_in_chat:" << chatId << "\n";
                                recipientStream.flush();

                                qDebug() << "Notification sent to user ID" << participantId << "about new message in chat ID" << chatId;
                            }
                        }
                    }
                    else
                    {
                        qCritical() << "Failed to query chat participants:" << participantsQuery.lastError().text();
                    }
                }
                else
                {
                    stream << "send_message:fail:" << query.lastError().text() << "\n";
                }
                stream.flush();
            }
            else if (command == "get_messages")
            {
                if(parts.count() < 3) return; // Теперь требуется минимум 3 части: команда, chatId и userId
                int chatId = parts.at(1).toInt();
                int userId = parts.at(2).toInt();
                getMessagesForChat(clientSocket, chatId, userId);
            }
            else if (command == "get_user_id")
            {
                if(parts.count() < 2) return;
                QString login = parts.at(1);
                getUserId(clientSocket, login);
            }
        }
    });
}

void Server::getChatsForUser(QTcpSocket* clientSocket, int userId)
{
    QSqlQuery query;
    query.prepare(R"( SELECT ua.login, c.chat_id, EXISTS
                (SELECT 1 FROM messages m WHERE m.chat_id = c.chat_id AND m.user_id != :user_id AND m.timestamp_read IS NULL)
                AS has_unread_messages FROM user_auth ua JOIN chat_participants cp ON cp.user_id = ua.user_id
                JOIN chats c ON c.chat_id = cp.chat_id WHERE c.chat_id IN
                (SELECT chat_id FROM chat_participants WHERE user_id = :user_id )
                AND ua.user_id != :user_id )");

    query.bindValue(":user_id", userId);

    if (query.exec())
    {
        QTextStream stream(clientSocket);
        while (query.next())
        {
            QString username = query.value(0).toString();
            QString chatId = query.value(1).toString();
            bool hasUnreadMessages = query.value(2).toBool();
            stream << "chat_list_item:" << chatId << ":" << username << ":" << (hasUnreadMessages ? "has_new_messages" : "no_new_messages") << '\n';
        }
        stream.flush();
    }
    else
    {
        qCritical() << "Failed to get chats for user:" << query.lastError().text();
    }
}


void Server::getUserId(QTcpSocket* clientSocket, const QString& login)
{
    QSqlQuery query;
    query.prepare("SELECT user_id FROM user_auth WHERE login = :login");
    query.bindValue(":login", login);

    if (query.exec() && query.next())
    {
        int userId = query.value(0).toInt();
        QTextStream stream(clientSocket);
        stream << "user_id:" << userId << "\n";
        stream.flush();
        qDebug() << "UserID = " << userId << "\n";
    }
    else
    {
        qCritical() << "Failed to get user ID for login:" << query.lastError().text();
    }
}

int Server::getUserID(const QString& login)
{
    QSqlQuery query;
    query.prepare("SELECT user_id FROM user_auth WHERE login = :login");
    query.bindValue(":login", login);

    if (query.exec() && query.next())
    {
        return query.value(0).toInt();
    }
    else
    {
        return 0;
    }
}

int Server::createChat(const QString& chatName, const QString& chatType, const QString& userName1, const QString& userName2)
{
    QSqlQuery query;
    int userId1 = findUserID(userName1);
    int userId2 = findUserID(userName2);

    if (userId1 == -1 || userId2 == -1)
    {
        qCritical() << "One of the users does not exist";
        return -1;
    }

    if (chatExistsBetweenUsers(userId1, userId2))
    {
        qCritical() << "Chat between these users already exists";
        return -1;
    }

    qDebug() << "chatName: " << chatName << " chatType: " << chatType << "\n";
    query.prepare("INSERT INTO chats (chat_name, chat_type) VALUES (:chat_name, :chat_type)");
    query.bindValue(":chat_name", chatName);
    query.bindValue(":chat_type", chatType);

    if (!query.exec())
    {
        qCritical() << "Failed to create chat:" << query.lastError().text();
        return -1;
    }
    return query.lastInsertId().toInt();
}

void Server::addUserToChat(const int chatId, const int userId)
{
    QSqlQuery query;
    query.prepare("INSERT INTO chat_participants (chat_id, user_id) VALUES (:chat_id, :user_id)");
    query.bindValue(":chat_id", chatId);
    query.bindValue(":user_id", userId);
    if (!query.exec())
    {
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
        if (query.next())
        {
            return query.value(0).toInt();
        }
        else
        {
            qCritical() << "User not found";
            return -1;
        }
    }
}

bool Server::chatExistsBetweenUsers(const int userId1, const int userId2)
{
    QSqlQuery query;
    query.prepare("SELECT chat_id FROM chat_participants WHERE user_id = :userId1 "
                  "INTERSECT "
                  "SELECT chat_id FROM chat_participants WHERE user_id = :userId2");
    query.bindValue(":userId1", userId1);
    query.bindValue(":userId2", userId2);
    if (!query.exec() || !query.next()) {
        return false; //Чата нет, можно создать новый
    } else {
        return true; //Чат между пользователями уже существует
    }
}

void Server::processSearchRequest(QTcpSocket* clientSocket, const QString& searchText, const QString& currentUserLogin)
{
    QSqlQuery query;
    query.prepare("SELECT login FROM user_auth WHERE login LIKE :searchText AND login != :currentUserLogin");
    query.bindValue(":searchText", "%" + searchText + "%");
    query.bindValue(":currentUserLogin", currentUserLogin);
    if (query.exec())
    {
        QTextStream stream(clientSocket);
        qDebug() << "Search query successful. Results for:" << searchText;
        while (query.next())
        {
            QString username = query.value(0).toString();
            stream << "search_result:" << username << '\n';
            stream.flush();
            qDebug() << "Found user:" << username;
        }
        stream << "search_end\n";
        stream.flush();
    }
    else
    {
        qCritical() << "Search query failed:" << query.lastError().text();
    }
}

void Server::getMessagesForChat(QTcpSocket* clientSocket, int chatId, int userId)
{
    bool logIsDone = false;
    QSqlQuery query;
    query.prepare("SELECT message_id, user_id, message_text, timestamp_sent, timestamp_read FROM messages WHERE chat_id = :chatId ORDER BY timestamp_sent ASC");
    query.bindValue(":chatId", chatId);
    if (query.exec())
    {
        QTextStream stream(clientSocket);
        while (query.next())
        {
            int messageId = query.value(0).toInt();
            int senderId = query.value(1).toInt();
            QString message = query.value(2).toString();
            QVariant timestampReadVar = query.value(4);
            bool messageAlreadyRead = !timestampReadVar.isNull();
            if (userId != senderId && !messageAlreadyRead)
            {
                QSqlQuery updateTimestampQuery;
                updateTimestampQuery.prepare("UPDATE messages SET timestamp_read = CURRENT_TIMESTAMP WHERE message_id = :messageId AND timestamp_read IS NULL");
                updateTimestampQuery.bindValue(":messageId", messageId);
                updateTimestampQuery.exec();
            }
            if(!logIsDone && userId != senderId && !messageAlreadyRead)
            {
                QString logMessage = QString("User with ID %1 read messages from user with ID %2 in chat with ID %3.")
                    .arg(QString::number(userId))
                    .arg(QString::number(senderId))
                    .arg(QString::number(chatId));
                Logger::getInstance()->logToFile(logMessage);
            }
            stream << "message_item:" << message << ":" << senderId << '\n';
        }
        stream << "end_of_messages\n";
        stream.flush();
    }
    else
    {
        qCritical() << "Failed to get messages for chat:" << query.lastError().text();
    }
}


void Server::updateLogViewer()
{
    QFile logFile(currentLogFilePath);
    if (logFile.open(QIODevice::ReadOnly))
    {
        QTextStream stream(&logFile);
        logViewer->setPlainText(stream.readAll());
        logFile.close();
        QScrollBar *scrollBar = logViewer->verticalScrollBar();
        scrollBar->setValue(scrollBar->maximum());
    }
}

void Server::selectLogFile()
{
    QString filename = QFileDialog::getOpenFileName(window, tr("Открыть файл"), QDir::homePath(), tr("Log Files (*.txt)"));
    if(!filename.isEmpty())
    {
        Logger::getInstance()->setLogFile(filename);
        currentLogFilePath = filename;
        updateLogViewer();
        logFileNameLabel->setText(tr("Файл логов: %1").arg(QFileInfo(filename).fileName()));
    }
}

