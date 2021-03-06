#ifndef CREATURE_H
#define CREATURE_H 1

#include <nds/jtypes.h>
#include "entities/creature.h"
#include "entities/object.h"
#include "list.h"
#include "datastream.h"
struct Object;
struct lightsource;

struct Creature : public listable<Creature> {
	POOLED(Creature)

	public: // data members
	u16 type;
	s16 x,y;
	s16 cooldown, regen_cooldown;
	s32 hp;
	s32 strength, agility, resilience, aim, melee;
	lightsource *light;
	List<Object> bag;
	Object *equipment[E_NUMSLOTS];

	public: // functions
	Creature();
	Creature(u16 type);
	void init(u16 _type);
	~Creature();

	inline const CreatureDesc *desc() { return &creaturedesc[type]; }

	s16 max_hp();                 // maximum HP, as calculated from stats
	bool canMove(s16 xp, s16 yp); // can the creature move to (xp,yp)?
	void move(s16 xp, s16 yp);    // moves the creature, updating map cells
	void setPos(s16 xp, s16 yp);  // sets x,y values (convenience)

	void drop(Object *item);    // remove an item from bag and drop it at current x,y
	void equip(Object *item);   // equip item. must be in bag.
	bool isEquipped(Object *o); // true if the given item is equippable and equipped

	// removes an item from the creature's inventory, unequipping it if necessary.
	// object must be in bag.
	void removeFromBag(Object *obj);

	void acted();      // bumps cooldown
	void regenerate(); // regenerate health, or bump regen_cooldown

	// AI
	void behave();
	private:
	void doBehaviour(); // wrapped function to avoid goto :)

	public: // serialisation
	friend DataStream& operator <<(DataStream&, Creature&);
	friend DataStream& operator >>(DataStream&, Creature&);
};
DataStream& operator <<(DataStream&, Creature&);
DataStream& operator >>(DataStream&, Creature&);

#endif /* CREATURE_H */
