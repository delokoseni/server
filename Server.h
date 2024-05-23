#ifndef SERVER_H
#define SERVER_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QTcpServer>
#include <QTcpSocket>
#include <QDir>
#include <QPlainTextEdit>
#include <QTimer>


class Server : public QTcpServer
{
    Q_OBJECT

private:
    QHash<int, QTcpSocket*> userSockets;
    QWidget* window;
    QLabel* statusLabel;
    QPushButton* logFileButton;
    QVBoxLayout* layout;
    unsigned int window_width = 450;
    unsigned int window_height = 300;
    QPlainTextEdit* logViewer;
    QTimer* logUpdateTimer;
    QString currentLogFilePath = QDir::homePath() + "/default_log.txt";
    QLabel* logFileNameLabel;

    void updateLogViewer();
    void selectLogFile();
    int getUserID(const QString& login);
    void onClientDisconnected();
    void processSearchRequest(QTcpSocket* clientSocket, const QString& searchText, const QString& currentUserLogin);
    void addUserToChat(const int chatId, const int userId);
    bool isLoginFree(const QString& username);
    void addUserToDatabase(const QString& username, const QString& password);
    bool validateUser(const QString& username, const QString& password);
    void processRegistration(QTcpSocket* clientSocket, const QString& username, const QString& password);
    void processLogin(QTcpSocket* clientSocket, const QString& username, const QString& password);
    int createChat(const QString& chatName, const QString& chatType, const QString& userName1, const QString& userName2);
    int findUserID(const QString& userName);
    bool chatExistsBetweenUsers(const int userId1, const int userId2);
    void getChatsForUser(QTcpSocket* clientSocket, int userId);
    void getMessagesForChat(QTcpSocket* clientSocket, int chatId, int userId);
    void getUserId(QTcpSocket* clientSocket, const QString& login);

public:
    Server(QObject *parent = nullptr);
    ~Server();
    void startServer(int port);

public slots:
    void onNewConnection();

};

#endif // SERVER_H
