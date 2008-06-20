#ifndef COMBAT_H
#define COMBAT_H 1

#include "list.h"
#include "creature.h"

// Returns true if the creature died. The creature will have been removed from
// the global monster list.
bool you_hit_monster(Node<Creature> target);
void monster_hit_you(Node<Creature> monster);

#endif /* COMBAT_H */
