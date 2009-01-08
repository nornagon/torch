object nil do
  char 'X'
  color 31, 0, 31
end

object 'rock' do
  char '8'
  color 18,18,18
  stackable
  usable
end

object 'rain stick' do
  char '/'
  color 18,18,0
  usable
end

object 'star' do
  char '.'
  color 31,31,31
  animation '.-+x+-.'
end

object 'vending machine' do
  char '#'
  color 31,5,2
  obstruction
end

object 'can of stewed beef' do
  char '%'
  plural 'cans of stewed beef'
  color 17,13,2
  stackable
  edible
end

object 'bottle of water' do
  char '%'
  plural 'bottles of water'
  color 0,25,25
  stackable
  drinkable
end


subEntity 'weapon', object do equip :weapon; char '/'; end
subEntity 'body', object do equip :body; char '}' end
subEntity 'helmet', object do equip :head; char '^' end
subEntity 'wrist', object do equip :wrist; char '=' end
subEntity 'boots', object do equip :feet; char ';' end

weapon 'baton' do
  color 31,28,0
  damage 4..6
end

body 'leather jacket' do
  color 17,10,0
  armor 5
end

boots 'pair of Blundstone boots' do
  color 10,10,10
  armor 4
end
