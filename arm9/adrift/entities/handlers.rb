handler :color, 'u16 color' do |c|
  "RGB15(#{c[0]},#{c[1]},#{c[2]})"
end

handler :char, 'u16 ch' do |c|
  "'#{c[0]}'"
end

default :color => [31,0,31]
default :char => '?'
