handler :color, 'u16 color' do |c|
  "RGB15(#{c[0]},#{c[1]},#{c[2]})"
end

handler :char, 'u16 ch' do |c|
  "'#{c[0]}'"
end

handler :natural, 's16 natural_min, natural_max' do |c|
  "#{c[0].min}, #{c[0].max}"
end
default :natural => [1..2]

default :color => [31,0,31]
default :char => ['?']
default :animation => nil
