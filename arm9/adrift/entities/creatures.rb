creature nil do
  char 'X'
  color 31, 0, 0
end

creature 'player' do
  char '@'
  color 31, 31, 31
  strength 1
  agility 1
  aim 1
  melee 1
end

creature 'venus fly trap' do
  char 'V'
  color 12, 29, 5
  strength 5
  agility 1
  aim 1
  melee 5

  cooldown 8
end

creature 'blowfly' do
  char 'x'
  color 7,7,7
  strength 1
  agility 5
  aim 1
  melee 1
  cooldown 5
  wanders; peaceful
end
