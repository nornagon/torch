default :max_hp => 0

creature nil do
  char 'X'
  color 31, 0, 0
end

creature 'player' do
  char '@'
  color 31, 31, 31
end

creature 'venus fly trap' do
  char 'V'
  color 12, 29, 5
  max_hp 20
end

creature 'blowfly' do
  char 'x'
  color 5,5,5
  max_hp 1
  wanders
  peaceful
end
