#include "player.h"
#include "bullet.h"
#include "enemy.h"
#include "score.h"
#include "game.h"
#include <QDebug>

#include <QKeyEvent>
#include <QGraphicsScene>
#include <settings.h>

#include "gameSettings.h"

extern Game * newGame;

Player::Player(QPoint point, QSize size, int id) : id(id)
{
    this->setRect(QRectF(point, size));
    this->setPos(player_spawns[id]);

    this->setBrush(Qt::red);
    //this->setPen(QPen(newGame->settings->player_color, 15, Qt::DashDotLine, Qt::RoundCap));

    auto timer = new QTimer(this);
    connect(timer,SIGNAL(timeout()), this, SLOT(canShoot()));
    timer->start(player_shot_cd);

    verticalMove = 0;
    horizontalMove = 0;
}

void Player::canShoot()
{
    this->shotFired = false;
}

void Player::respawn()
{
    delete respawnTimer;
    dead = false;
    invulnerable = true;
    isShooting = false;

    invulTimer = new QTimer();
    connect(invulTimer,SIGNAL(timeout()), this, SLOT(resetInvulnerability()));
    invulTimer->start(player_invul_time);
    this->setOpacity(0.2);

    this->setPos(player_spawns[id]);
    newGame->graphicsScene->addItem(this);
}

void Player::resetInvulnerability()
{
    this->setOpacity(1.0);
    delete invulTimer;
    invulnerable = false;
}

void Player::move(int speed) {
    qDebug() << "Ruch" << horizontalMove << verticalMove;
    this->moveBy(horizontalMove*speed, verticalMove*speed);

    if(pos().x() < 0)
        setPos(0, pos().y());
    else if(pos().x() + player_size.width() > screen_size.width())
       setPos(screen_size.width() - player_size.width(), pos().y());

    if(pos().y() < 0)
        setPos(pos().x(), 0);
    else if(pos().y() + player_size.height() > screen_size.height())
        setPos(pos().x(), screen_size.height() - player_size.height());
}
