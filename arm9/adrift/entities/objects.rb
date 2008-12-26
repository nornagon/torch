object nil do
  char 'X'
  color 31, 0, 31
end

object 'rock' do
  char '8'
  color 18,18,18
  stackable
end

object 'rain stick' do
  char '/'
  color 18,18,0
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
end

object 'bottle of water' do
  char '%'
  plural 'bottles of water'
  color 0,25,25
  stackable
end
