//#include "main.moc"
#include "Server.h"

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    int port = 3000;
    Server server;
    server.startServer(port);
    return app.exec();
}
