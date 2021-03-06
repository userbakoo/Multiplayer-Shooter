#include "servergame.h"
#include <QDebug>
#include <QKeyEvent>
#include <enemy.h>
#include <gameSettings.h>


ServerGame::ServerGame(QWidget *parent) : QGraphicsView(parent)
{
    numOfEnemies = 0;
    network = new Network(12345, this);
}

void ServerGame::initGame()
{
    graphicsScene = new QGraphicsScene();
    graphicsScene->setSceneRect(QRectF(screenPoint, screenSize));
    setScene(graphicsScene);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setFixedSize(screenSize);
    this->setFocus();

    connect(network, SIGNAL(playerMadeAction(const PlayerAction&)), this, SLOT(handlePlayerAction(const PlayerAction&)));
    connect(network, SIGNAL(newClientConnected(int)), this, SLOT(onConnection(int)));

    timer = new QTimer();
    connect(timer,SIGNAL(timeout()), this, SLOT(gameLoop()));
    timer->start(gameTimerRes);

    auto scoreTimer = new QTimer();
    QObject::connect(scoreTimer, SIGNAL(timeout()), this, SLOT(updatePoints()));
    scoreTimer->start(scorePassiveIntervalInMs);

    auto enemyTimer = new QTimer();
    QObject::connect(enemyTimer, SIGNAL(timeout()), this, SLOT(spawnEnemy()));
    enemyTimer->start(2000);

    connect(network, SIGNAL(gameDisconnect(int)), this, SLOT(onPlayerDisconnected(int)));
}

void ServerGame::addNewPlayer(int playerID)
{
    playersMap[playerID] = new Player(playerPoint, playerSize,playerID);
    graphicsScene->addItem(playersMap[playerID]);
    playersMap[playerID]->score = 0;

    show();
}

void ServerGame::spawnEnemy()
{
    if(numOfEnemies < 9)
    {
        auto newEnemy = new Enemy();
        int randPos = rand() & 750;
        newEnemy->setRect(QRectF(QPoint(randPos, 0), enemySize));

        scene()->addItem(newEnemy);
        numOfEnemies++;
    }
}

void ServerGame::updatePoints()
{
    for(const auto& id : playersMap.keys())
    {
        if(!playersMap[id]->dead)
            playersMap[id]->score += scorePassiveValue;
    }
    this->dumpGameInfo();
}

void ServerGame::handlePlayerAction(const PlayerAction &playerAction)
{
    qDebug() << "Handle player action!";
    Player* player = playersMap[playerAction.id];

    player->horizontalMove = playerAction.horizontal;
    player->verticalMove = playerAction.vertical;

    player->isShooting = playerAction.shooting;
    player->shootingDirection = playerAction.shootDirection;
}

void ServerGame::gameLoop() {
    playerAction();
    moveAssets();
    checkAllCollisions();
    GameState gameState = dumpGameInfo();
    network->sendAll(gameState);
}

void ServerGame::playerAction() {
    for(const auto& id : playersMap.keys())
    {
        Player* player = playersMap[id];
        if(!player->dead)
        {
            if(player->isShooting && !player->shotFired)
            {
                fireBullet(player);
            }

            player->move(playerSpeed);
        }
    }
}

void ServerGame::moveAssets() {

    QList<QGraphicsItem *> all_items = graphicsScene->items();
    for(int i =0; i < all_items.length(); i++)
    {
        if(typeid(*(all_items[i])) == typeid(Bullet))
        {
            Bullet * bullet = dynamic_cast<Bullet *>(all_items[i]);
            bullet->move();
            if(all_items[i]->pos().y() + bulletSize.height() < 0 || all_items[i]->pos().y() > screenSize.height()
                    || all_items[i]->pos().x() < 0 || all_items[i]->pos().x() > screenSize.width() )       // If out of bounds
            {
                graphicsScene->removeItem(all_items[i]);
                auto index = playersMap[bullet->player_id]->playerBullets.indexOf(bullet);
                playersMap[bullet->player_id]->playerBullets.takeAt(index);
                delete bullet;
                continue;
            }
        }
        else if(typeid(*(all_items[i])) == typeid(Enemy))
        {
            all_items[i]->moveBy(0, enemySpeed);
            if(all_items[i]->pos().y() > screenSize.height())
            {
                numOfEnemies--;
                //qDebug() << "On delete(move): " << numOfEnemies;
                graphicsScene->removeItem(all_items[i]);
                delete all_items[i];
            }
        }
    }
}

void ServerGame::checkAllCollisions() {
    QList<QGraphicsItem *> toBeDeleted;
    QList<QGraphicsItem *> all_items = graphicsScene->items();

    for(int i =0; i < all_items.length(); i++)
    {
        if(typeid(*(all_items[i])) == typeid(Bullet))
        {
            int bulletPlayerID = ((Bullet*)all_items[i])->player_id;
            QList<QGraphicsItem *> colliding_items = all_items[i]->collidingItems();
            if(!colliding_items.size())
            {
                continue;
            }
            if(typeid(*(colliding_items[0])) == typeid(Bullet))
            {
                if(!toBeDeleted.contains(all_items[i]))
                    toBeDeleted.push_back(all_items[i]);
                if(!toBeDeleted.contains(colliding_items[0]))
                    toBeDeleted.push_back(colliding_items[0]);
            }
            else if(typeid(*(colliding_items[0])) == typeid(Enemy))
            {
                playersMap[bulletPlayerID]->score += scoreObstacleHit;
                if(!toBeDeleted.contains(all_items[i]))
                    toBeDeleted.push_back(all_items[i]);
                if(!toBeDeleted.contains(colliding_items[0]))
                    toBeDeleted.push_back(colliding_items[0]);
            }
            else if (typeid(*(colliding_items[0])) == typeid(Player))
            {
                Player * player = dynamic_cast<Player *>(colliding_items[0]);
                Bullet * bullet = dynamic_cast<Bullet *>(all_items[i]);
                if(player->dead || player->id == bullet->player_id)
                {
                    continue;
                }
                else if(player->invulnerable)
                {
                    if(!toBeDeleted.contains(all_items[i]))
                        toBeDeleted.push_back(all_items[i]);
                    continue;
                }
                else
                {
                    qDebug() << "Player o id: " << bullet->player_id << "zabil!";
                    playersMap[bulletPlayerID]->score += scoreEnemyHit;
                    if(!toBeDeleted.contains(all_items[i]))
                        toBeDeleted.push_back(all_items[i]);
                    killPlayer(player);
                    continue;
                }
            }
            else
            {
                if(!toBeDeleted.contains(all_items[i]))
                    toBeDeleted.push_back(all_items[i]);
            }
        }
        if(typeid(*(all_items[i])) == typeid(Player))
        {
            QList<QGraphicsItem *> colliding_items = all_items[i]->collidingItems();
            for(int j = 0; j < colliding_items.size(); j++)
            {
                if(typeid(*(colliding_items[j])) == typeid(Obstacle) || typeid(*(colliding_items[j])) == typeid(Enemy))
                {
                    Player * player = dynamic_cast<Player *>(all_items[i]);
                    if(!player->dead && !player->invulnerable)
                    {
                        killPlayer(player);
                    }
                }
                else if(typeid(*(colliding_items[j])) == typeid(Player) )
                {
                    Player * player = dynamic_cast<Player *>(all_items[i]);
                    Player * player2 = dynamic_cast<Player *>(colliding_items[j]);
                    if(player->dead || player2->dead)
                        continue;
                    if(!player->invulnerable)
                        killPlayer(player);
                    if(!player2->invulnerable)
                        killPlayer(player2);
                }
            }
        }
    }

    for(int i =0; i < toBeDeleted.size(); i++)
    {
        scene()->removeItem(toBeDeleted[i]);
        if( typeid(*(toBeDeleted[i])) == typeid(Enemy))
        {
            numOfEnemies--;
            qDebug() << "On delete: " << numOfEnemies;
        }

        if( typeid(*(toBeDeleted[i])) == typeid(Bullet))
        {
            Bullet * bullet = dynamic_cast<Bullet *>(toBeDeleted[i]);
            auto index = playersMap[bullet->player_id]->playerBullets.indexOf(bullet);
            playersMap[bullet->player_id]->playerBullets.takeAt(index);
        }
        else delete toBeDeleted[i];
    }

}

void ServerGame::killPlayer(Player * player)
{
    player->kill();
    connect(player, SIGNAL(respawn(Player*)), this, SLOT(respawnPlayer(Player*)));
}

void ServerGame::respawnPlayer(Player* player) {
    //graphicsScene->addItem(player);
}


void ServerGame::fireBullet(Player* player)
{
    if(player->playerBullets.size() < playerMaxBullets)
    {
        auto newBullet = new Bullet(player->shootingDirection, player->id, player->pos());
        player->playerBullets.append(newBullet);
        this->graphicsScene->addItem(newBullet);
        player->shotFired = true;
    }

}

void ServerGame::generateLayoutOne()
{
    QList<Obstacle *> obstacleLayout;
    QSize size = obstacleSize;

    int screenWidth = screenSize.width();
    int screenHeight = screenSize.height();

    int count = 0;
    for(int i = 0.2*screenWidth; i < screenWidth; i+= 0.2*screenWidth)
    {
        for(int j = 1; j < 4; j++)
        {
            count++;
            generateObstacle(QPoint(i,screenHeight -j*size.height()), size);
        }

    }
    for(int i = 0.33 *screenWidth; i < screenWidth-size.width(); i+= 0.33*screenWidth)
    {
        for(int j = 1; j < 5; j++)
        {
            count++;
            generateObstacle(QPoint(i,0.4*screenHeight -j*size.height()), size);
        }
    }
}


void ServerGame::generateObstacle(QPoint point, QSize size) {
    auto obstacle = new Obstacle();
    obstacle->setPos(0,0);
    obstacle->setRect(point.x(),point.y(),size.width(),size.height());
    graphicsScene->addItem(obstacle);
}

GameState ServerGame::dumpGameInfo()
{
    QList<QGraphicsItem*> allItems = graphicsScene->items();
    GameState gameInfo;

    for (auto* item : allItems)
    {
        if (typeid(*item) == typeid(Bullet))
        {
            Bullet* tempBullet = dynamic_cast<Bullet*>(item);
            BulletInfo bulletInfo = {
                .pos = tempBullet->pos(),
                .direction = tempBullet->moveSet,
                .playerID = tempBullet->player_id
            };
            gameInfo.bulletList.append(bulletInfo);
        }
        if (typeid(*item) == typeid(Player))
        {
            Player* tempPlayer = dynamic_cast<Player*>(item);
            PlayerInfo playerInfo = {
                .pos = tempPlayer->pos(),
                .id = tempPlayer->id,
                .currentScore = this->playersMap[tempPlayer->id]->score,
                .invulnerable = this->playersMap[tempPlayer->id]->invulnerable
            };
            gameInfo.playersInfoMap[playerInfo.id] = playerInfo;
        }
        if (typeid(*item) == typeid(Obstacle))
        {
            Obstacle* tempObstacle = dynamic_cast<Obstacle*>(item);
            QPointF point(tempObstacle->rect().x(), tempObstacle->rect().y());
            ObstacleInfo obstacleInfo = { .pos = point};
            gameInfo.obstacleList.append(obstacleInfo);
        }
        if (typeid(*item) == typeid(Enemy))
        {
            Enemy* tempEnemy = dynamic_cast<Enemy*>(item);
            QPointF point(tempEnemy->rect().x(),tempEnemy->pos().y());
            EnemyInfo enemyInfo = { .pos = point };
            gameInfo.enemyList.append(enemyInfo);
        }
    }
    return gameInfo;
}

void ServerGame::onConnection(int id)
{
    qDebug() << "New player with id " << id;
    addNewPlayer(id);
}


void ServerGame::onPlayerDisconnected(int playerID) {
    qDebug() << " Game::onDisconnected, playerID: " << playerID;

    if(playersMap[playerID])
        graphicsScene->removeItem(playersMap[playerID]);
    playersMap.remove(playerID);
    network->clientsMap.remove(playerID);
    qDebug() << "Removed from playersMap & clientMap";
}

