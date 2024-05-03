#include <QTcpServer>
#include <QTcpSocket>
#include <QCoreApplication>
#include <QTextStream>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

class ChatServer : public QTcpServer {
    Q_OBJECT

public:
    ChatServer(QObject *parent = nullptr) : QTcpServer(parent) {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
        db.setDatabaseName("messenger.db"); // Путь к вашей базе данных SQLite

        if (!db.open()) {
            qCritical() << "Could not connect to database:" << db.lastError().text();
            exit(1);
        }

        connect(this, &ChatServer::newConnection, this, &ChatServer::onNewConnection);
    }

    bool isLoginFree(const QString& username) {
        QSqlQuery query;
        query.prepare("SELECT COUNT(*) FROM user_auth WHERE login = :login");
        query.bindValue(":login", username);
        if (!query.exec()) {
            qCritical() << "Failed to check if login is free:" << query.lastError().text();
            return false;
        }
        if (query.next()) {
            int usersCount = query.value(0).toInt();
            return usersCount == 0;
        }
        return false;
    }

    void addUserToDatabase(const QString& username, const QString& password) {
        QSqlQuery query;
        query.prepare("INSERT INTO user_auth (login, password) VALUES (:login, :password)");
        query.bindValue(":login", username);
        // Ваша логика хеш-функции должна быть здесь
        query.bindValue(":password", password);
        if (!query.exec()) {
            qCritical() << "Failed to add user to database:" << query.lastError().text();
        }
    }

    void startServer(int port) {
        if (!this->listen(QHostAddress::Any, port)) {
            qCritical() << "Could not start server";
        } else {
            qDebug() << "Server started on port" << port;
        }
    }

public slots:
    void onNewConnection() {
        QTcpSocket *clientSocket = this->nextPendingConnection();
        connect(clientSocket, &QTcpSocket::readyRead, this, [this, clientSocket]() {
            QTextStream stream(clientSocket);
            QString message = stream.readAll().trimmed();
            qDebug() << "New message received:" << message;

            QStringList parts = message.split(":");
            if (parts.first() == "register" && parts.count() == 3) {
                QString username = parts.at(1);
                QString password = parts.at(2);

                if(isLoginFree(username)) {
                    addUserToDatabase(username, password); // Добавляем пользователя в базу
                    stream << "register:success\n";
                } else {
                    stream << "register:error:username taken\n";
                }
            }
            // Можно добавить больше команд и их обработку здесь
        });
    }
};

#include "main.moc"

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    ChatServer server;
    server.startServer(3000); // Запускаем сервер на порту 3000

    return app.exec();
}
