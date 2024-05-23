#include "Server.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    unsigned int port = 3000;
    Server server;
    server.startServer(port);
    return app.exec();
}
