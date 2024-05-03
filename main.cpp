#include <QTcpServer>
#include <QTcpSocket>
#include <QCoreApplication>
#include <QTextStream>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDir>

class ChatServer : public QTcpServer {
    Q_OBJECT

public:
    ChatServer(QObject *parent = nullptr) : QTcpServer(parent) {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
        db.setDatabaseName(QDir::homePath() + "/messenger.db");  // Обратите внимание, что это добавит messenger.db к домашнему каталогу пользователя


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
            return query.value(0).toInt() == 0;
        }
        return false;
    }

    void addUserToDatabase(const QString& username, const QString& password) {
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

    void startServer(int port) {
        if (!this->listen(QHostAddress::Any, port)) {
            qCritical() << "Could not start server";
        } else {
            qDebug() << "Server started on port" << port;
        }
    }

    // Вставьте этот кусок кода где-то в класс ChatServer
    bool validateUser(const QString& username, const QString& password) {
        QSqlQuery query;
        query.prepare("SELECT password FROM user_auth WHERE login = :login");
        query.bindValue(":login", username);
        if (!query.exec()) {
            qCritical() << "Failed to check user credentials:" << query.lastError().text();
            return false;
        }
        if (query.next()) {
            QString storedPassword = query.value(0).toString();
            // In a real application, compare the hashed password instead
            return password == storedPassword;
        }
        return false;
    }

    void processRegistration(QTcpSocket* clientSocket, const QString& username, const QString& password) {
        if(isLoginFree(username)) {
            addUserToDatabase(username, password); // Добавляем пользователя в базу
            QTextStream stream(clientSocket);
            stream << "register:success\n";
        } else {
            QTextStream stream(clientSocket);
            stream << "register:fail:username taken\n";
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
                processRegistration(clientSocket, username, password);
            }
            // Можно добавить больше команд и их обработку здесь
        });
    }

};

#include "main.moc"

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    ChatServer server;
    server.startServer(3000); // Start server on port 3000

    return app.exec();
}
