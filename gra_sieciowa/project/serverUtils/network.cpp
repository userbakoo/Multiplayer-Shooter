#include "network.h"
#include "gameSettings.h"

Network::Network(quint16 port, QObject* parent) : QTcpServer(parent)
{

    connect(this, SIGNAL(newConnection()), this, SLOT(onNewConnection()));
    if (!listen(QHostAddress::Any, port))
    {
           close();
    }

}

void Network::onNewConnection()
{
    QTcpSocket* clientSock = nextPendingConnection();
    ClientSocket* sockHandle = new ClientSocket(clientSock);

    if(this->clientsMap.size() >= playerMaxCount) {
        qDebug() << "Max players limit has been reached!";
        delete sockHandle;
    }

    int clientMapId = addClientToMap(sockHandle);

    qDebug() << "New connection, with id: " << clientMapId;
    emit newClientConnected(clientMapId);
    qDebug() << "Client connected!";
    connect(sockHandle, SIGNAL(disconnected()), this, SLOT(onDisconnected()));
    connect(sockHandle, SIGNAL(message(const QString&)), this, SLOT(onMessage(const QString&)));
    connect(sockHandle, SIGNAL(message(const QByteArray&)), this, SLOT(onMessage(const QByteArray&)));
}


int Network::addClientToMap(ClientSocket* clientSocket) {
    for (int i = 0; i < playerMaxCount; ++i) {
       if(!clientsMap.contains(i)) {
           clientsMap[i] = clientSocket;
           return i;
       }
    }
    qDebug() << "Should be impossible! Max players should be checked before this function call";
    exit(EXIT_FAILURE);
}

void Network::onDisconnected()
{
    qDebug() << "ServerNetwork::onDisconnected, clientsLen:" << clientsMap.size() << endl;

    int disconnecteSocketID = getDisconnectedSocketID();
    if(disconnecteSocketID == -1) {
        qDebug() << "disconnecteSocketID == -1!";
        exit(EXIT_FAILURE);
    }

    emit gameDisconnect(disconnecteSocketID);
}

int Network::getDisconnectedSocketID() {
    for (const auto& id : clientsMap.keys()) {
        if(clientsMap[id]->tcpSocket->state() != QTcpSocket::ConnectedState) {
            clientsMap[id]->tcpSocket->disconnect();
            clientsMap[id]->tcpSocket->disconnectFromHost();
            clientsMap[id]->tcpSocket->deleteLater();
            return id;
        }
    }
    return -1;
}

void Network::sendAll(const QString &message) const
{
    for (const auto& socket : clientsMap.values())
    {
        socket->sendString(message);
    }
}

void Network::sendAll(const QByteArray &data) const
{
    for (const auto& socket : clientsMap.values())
    {
        socket->sendData(data);
    }
}

void Network::sendAll(const GameState& gameState) const
{
    for (const auto& clientID : clientsMap.keys())
    {
        clientsMap[clientID]->sendGameState(gameState, clientID);
    }
}

void Network::send(const int playerID) const
{
    clientsMap[playerID]->sendData(QByteArray(static_cast<char *>((void*)&playerID), sizeof(int)));
}


void Network::onMessage(const QString& message) const
{
    //sendAll(message);
}


void Network::onMessage(const QByteArray& data) const
{
    PlayerAction pA = parsePlayerAction(data);
    emit playerMadeAction(pA);
}

PlayerAction Network::parsePlayerAction(const QByteArray& data) const
{
    PlayerAction playerAction;
    memcpy(&playerAction,data.data(), sizeof(playerAction));
    return playerAction;

}

