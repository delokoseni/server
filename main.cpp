#include <QTcpServer>
#include <QTcpSocket>
#include <QCoreApplication>
#include <QTextStream>

class ChatServer : public QTcpServer {
    Q_OBJECT
public:
    ChatServer(QObject *parent = nullptr) : QTcpServer(parent) {
        connect(this, &ChatServer::newConnection, this, &ChatServer::onNewConnection);
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
};

#include "main.moc"

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);

    ChatServer server;
    server.startServer(3000); // Запуск сервера на порту 3000

    return app.exec();
}
