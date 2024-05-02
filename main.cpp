#include <QTcpServer>
#include <QTcpSocket>
#include <QCoreApplication>
#include <QTextStream>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>

class ChatServer : public QTcpServer {
    Q_OBJECT
public:
    //ChatServer(QObject *parent = nullptr) : QTcpServer(parent) {
        //connect(this, &ChatServer::newConnection, this, &ChatServer::onNewConnection);
    //}
    ChatServer(QObject *parent = nullptr) : QTcpServer(parent) {
            // Установка подключения к SQLite базе данных
            QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
            db.setDatabaseName("messenger.db"); // Имя файла базы данных SQLite

            if (!db.open()) {
                qCritical() << "Could not connect to database:" << db.lastError();
                exit(1);
            }

            connect(this, &ChatServer::newConnection, this, &ChatServer::onNewConnection);
        }

        bool isLoginFree(const QString& username) {
            QSqlQuery query;
            query.prepare("SELECT COUNT(*) FROM user_auth WHERE login = :login");
            query.bindValue(":login", username);
            if (!query.exec()) {
                qCritical() << "Failed to check if login is free:" << query.lastError();
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
            query.bindValue(":password", password); // Замените это хэшированием в будущем!
            if (!query.exec()) {
                qCritical() << "Failed to add user to database:" << query.lastError();
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
            QString message = QString::fromUtf8(clientSocket->readAll());
            qDebug() << "New message received:" << message;
            // Здесь вы можете обрабатывать сообщения
        });
    }
    void onReadyRead() {
        QTcpSocket *clientSocket = qobject_cast<QTcpSocket *>(sender());
        if (clientSocket) {
            QString message = QString::fromUtf8(clientSocket->readAll().trimmed());
            qDebug() << "New message received:" << message;

            QTextStream stream(clientSocket);
            QStringList parts = message.split(":");
            if (parts.first() == "register" && parts.count() == 3) {
                QString login = parts.at(1);
                QString password = parts.at(2);

                if(isLoginFree(login)) {
                    addUserToDatabase(login, password); // Add hashed password in a real app
                    stream << "register:success\n";
                } else {
                    stream << "register:error:Логин уже занят\n";
                }
            }
        }
    }

};

#include "main.moc"

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);

    ChatServer server;
    server.startServer(3000); // Запуск сервера на порту 3000

    return app.exec();
}
