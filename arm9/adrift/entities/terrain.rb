terrain nil do
  char ' '
  color 0, 0, 0
end

terrain 'tree' do
  char '*'
  color 4, 31, 1
  solid; opaque
end

terrain 'ground' do
  char '.'
  color 17, 9, 6
  forgettable
end

terrain 'glass' do
  char '/'
  color 4, 12, 30
  solid
end

terrain 'water' do
  char '~'
  color 5, 14, 23
  solid
end

terrain 'fire' do
  char 'w'
  color 28, 17, 7
  solid
end
