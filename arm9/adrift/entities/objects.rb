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
