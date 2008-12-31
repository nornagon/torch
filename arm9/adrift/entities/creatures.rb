creature nil do
  char 'X'
  color 31, 0, 0
end

creature 'player' do
  char '@'
  color 31, 31, 31
  strength 4
  agility 4
  resilience 4
  aim 4
  melee 4
end

creature 'venus fly trap' do
  char 'V'
  color 12, 29, 5
  strength 5
  agility 1
  resilience 4
  aim 1
  melee 5

  natural 3..5

  cooldown 15
  stationary
end

creature 'blowfly' do
  char 'x'
  color 7,7,7
  strength 1
  agility 5
  resilience 1
  aim 1
  melee 1
  cooldown 5
  wanders; peaceful
end

creature 'labrador' do
  char 'd'
  color 28, 23, 0
  strength 6
  agility 3
  resilience 3
  aim 1
  melee 4
  cooldown 10
  wanders; hungry
end

creature 'will o\'wisp' do
  char 'W'
  color 4,13,28
  strength 1
  agility 10
  resilience 1
  aim 4
  melee 1
  cooldown 8
  peaceful; shy; wanders
  flying
end
