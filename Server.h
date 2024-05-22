#ifndef SERVER_H
#define SERVER_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QTcpServer>
#include <QTcpSocket>
#include <QCoreApplication>
#include <QTextStream>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDir>
#include <QPlainTextEdit>
#include <QFormLayout>
#include <QTimer>
#include <QScrollBar>

class Server : public QTcpServer {
    Q_OBJECT

public:
    Server(QObject *parent = nullptr);
    ~Server();
    bool isLoginFree(const QString& username);
    void addUserToDatabase(const QString& username, const QString& password);
    void startServer(int port);
    bool validateUser(const QString& username, const QString& password);
    void processRegistration(QTcpSocket* clientSocket, const QString& username, const QString& password);
    void processLogin(QTcpSocket* clientSocket, const QString& username, const QString& password);
    int createChat(const QString& chatName, const QString& chatType, const QString& userName1, const QString& userName2);
    int findUserID(const QString& userName);
    bool chatExistsBetweenUsers(const int userId1, const int userId2);
    void getChatsForUser(QTcpSocket* clientSocket, int userId);
    void getMessagesForChat(QTcpSocket* clientSocket, int chatId);
    void getUserId(QTcpSocket* clientSocket, const QString& login);

public slots:
    void onNewConnection();
    void processSearchRequest(QTcpSocket* clientSocket, const QString& searchText);
    void addUserToChat(const int chatId, const int userId);

private:
    QHash<int, QTcpSocket*> userSockets;
    QWidget* window;
    QLabel* statusLabel;
    QPushButton* logFileButton;
    QVBoxLayout* layout;
    unsigned int window_width = 450, window_height = 300;
    QPlainTextEdit* logViewer;
    QTimer* logUpdateTimer;
    QString currentLogFilePath = QDir::homePath() + "/default_log.txt";
    QLabel* logFileNameLabel;
    void updateLogViewer();
    void selectLogFile();
};

#endif // SERVER_H
