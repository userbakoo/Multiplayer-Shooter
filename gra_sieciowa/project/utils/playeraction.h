#ifndef PLAYERACTION_H
#define PLAYERACTION_H

#include <QEvent>

struct PlayerAction
{
    int id;
    int horizontal;
    int vertical;
    int shooting;
    int shootDirection;
};

#endif // PLAYERACTION_H
