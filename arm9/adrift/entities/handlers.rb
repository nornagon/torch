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

handler :damage, 's16 damage_min, damage_max' do |c|
  "#{c[0].min}, #{c[0].max}"
end
default :damage => [0..0]

EquipSlots = [:weapon, :body, :head, :wrist, :feet]
handler :equip, 'u8 equip' do |c|
  if c.nil?
    EquipSlots.length.to_s
  else
    idx = EquipSlots.index(c[0])
    if idx.nil?
      raise "Invalid equipment slot #{c[0].to_s}"
    end
    idx.to_s
  end
end
handler 'object.h' do
  first = true
  "enum {\n" +
    EquipSlots.map do |e|
      name = "E_" + e.to_s.upcase
      if first
        name += " = 0"
        first = false
      end
      "\t" + name
    end.join(",\n") +
  ",\n\tE_NUMSLOTS\n};"
end

default :color => [31,0,31]
default :char => ['?']
default :animation => nil
