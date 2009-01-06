require 'descdsl'

class String
  # better than upcase for converting to a C identifier...
  def to_c
    upcase.gsub(/[^A-Z0-9]+/,'_')
  end
end

class Type
  def initialize v
    if v.is_a? Array
      if v.length == 1
        @type = v[0].class
      elsif v.length == 0
        @type = true.class
      else
        @type = v.map { |a| Type.new(a) }
        if !@type.all? { |k| k == @type[0] }
          puts "Error: arrays must have only one type.\n"
          exit 1
        end
      end
    else
      @type = v.class
    end
  end

  def == o
    otype = o.instance_eval("@type")
    if otype.class != @type.class then return false end
    if @type.is_a? Array
      if otype.length != @type.length then return false end
      return @type.zip(otype).all? { |t1,t2| t1 == t2 }
    else
      return @type == otype
    end
    return false
  end

  def to_s
    if @type.is_a? Array
      "[" + @type.map { |t| t.to_s }.join(",") + "]"
    else
      @type.to_s
    end
  end

  def inspect
    to_s
  end

  def to_c name
    if @type.is_a? Array
      @type[0].to_c name.to_s + "[#{@type.length}]"
    else
      case @type.to_s.to_sym
      when :Fixnum: "s32"
      when :String: "const char *"
      when :TrueClass: "bool"
      else "XXX #{@type.to_s}"
      end + " " + name.to_s + (@type == TrueClass ? " : 1" : "")
    end
  end
end

def Type v
  Type.new(v)
end

def fields defns
  flds = {}
  defns.values.each { |d|
    d.each { |k,v|
      ty = Type(v)
      if flds[k].nil?
        flds[k] = ty
      elsif flds[k] != ty
        puts "Type mismatch for field '#{k}'"
        puts "    Expected: #{flds[k]}"
        puts "    Got: #{ty}"
        exit 1
      end
    }
  }
  flds
end

def toCVal v
  case v
  when Array
    if v.length == 1
      toCVal v[0]
    elsif v.length == 0
      toCVal true
    else
      "{ " + v.map { |a| toCVal a }.join(', ') + " }"
    end
  when Fixnum
    v.to_s
  when String
    v.inspect
  when TrueClass, FalseClass
    v ? '1' : '0'
  when NilClass
    'NULL'
  else
    puts "Unhandled type: #{v.class}"
    exit 1
  end
end

def printValue io, flds, name, vals
  io.print "\t{ "
  io.print name.inspect
  io.print ", "
  io.print(flds.map { |k,ty|
    if !vals.has_key? k
      # gotta use a default
      if $defaults[k]
        # if there's a default for this field, use that.
        vals[k] = $defaults[k]
      else
        # otherwise use the default for that type.
        case ty.to_s.to_sym
        when :Fixnum: vals[k] = 0
        when :String: vals[k] = nil
        when :TrueClass: vals[k] = false
        else
          vals[k] = nil
        end
      end
    end
    if $handlers[k]
      $handlers[k][1][vals[k]]
    else
      toCVal vals[k]
    end
  }.join(', '))
  io.print "},\n"
end

if $0 == __FILE__
  require 'fileutils'

  targdir = ARGV.length < 1 ? 'entities' : ARGV[0]

  FileUtils.cd targdir

  $handlers = {}
  $defaults = {}

  # handler :damage, 's16 damage_min, damage_max' do ... end
  # handler 'object.h' do "enum { ... }" end
  def handler field, type=nil, &blk
    if type.nil?
      $handlers[field] = blk
    else
      $handlers[field] = [type, blk]
    end
  end

  def default assoc
    $defaults.merge! assoc
  end

  Dir["*.rb"].each do |rb|
    load rb
  end

  $entities.each do |kind,defns|
    flds = fields(defns).sort_by do |k,ty|
      t = $handlers[k] ? $handlers[k][0] : ty.to_c("")
      length = case t
               when /bool/: 1
               when /\*/: 32 # pointer
               when /(\d+)/: $1.to_i # s32, s16, etc
               else 8
               end
      length
    end.reverse
    File.open "#{kind}.h", "w" do |io|
      io.puts "#ifndef ENTITY_#{kind.to_c}_H"
      io.puts "#define ENTITY_#{kind.to_c}_H"
      io.puts
      io.puts "#include <nds/jtypes.h>"
      io.puts "#include <stdlib.h>"
      io.puts
      io.puts "struct #{kind.capitalize}Desc {"
      io.puts "\tconst char *name;"
      flds.each do |k,ty|
        if $handlers[k]
          io.puts "\t#{$handlers[k][0]};"
        else
          io.puts "\t#{ty.to_c k};"
        end
      end
      io.puts "};"
      io.puts
      io.puts "extern #{kind.capitalize}Desc #{kind}desc[];"
      io.puts
      io.puts "enum {"
      if defns.has_key? nil
        io.puts "\t#{kind.to_c}_NONE = 0,"
      end
      defns.each do |name,vals|
        if name.nil? then next end
        io.puts "\t#{name.to_c},"
      end
      io.puts "};"
      io.puts
      if $handlers["#{kind}.h"]
        io.puts $handlers["#{kind}.h"].call
        io.puts
      end
      io.puts "#endif"
    end
    File.open "#{kind}.cpp", "w" do |io|
      io.puts "#include \"entities/#{kind}.h\""
      io.puts "#include <nds/arm9/video.h>"
      io.puts
      io.puts "#{kind.capitalize}Desc #{kind}desc[] = {"
      if defns.has_key? nil
        printValue io, flds, "nil", defns[nil]
      end
      defns.each do |name,vals|
        if name.nil? then next end
        printValue io, flds, name, vals
      end
      io.puts "};"
    end
  end
end
