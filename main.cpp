//#include "main.moc"
#include "Server.h"

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    Server server;
    server.startServer(3000); // Start server on port 3000
    return app.exec();
}
