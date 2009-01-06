class BlankSlate
  instance_methods.each { |m| undef_method m unless m =~ /^__|^instance_eval$/ }
end

class Vivify < BlankSlate
  def method_missing name, *args
    @tags ||= {}
    @tags[name] = args
  end
end
def defEntity kind
  kind = kind.to_str
  (class<<self;self end).send(:define_method, kind) do |*name,&blk|
    if name.empty?
      return method(kind)
    else
      name = name[0]
      $entities ||= {}
      $entities[kind] ||= {}
      v = Vivify.new
      v.instance_eval(&blk)
      $entities[kind][name] = v.instance_eval("@tags")
    end
  end
end

def subEntity child,parent,&with
  (class<<self;self end).send(:define_method, child) do |*name,&blk|
    if name.empty?
      return method(child)
    else
      name = name[0]
      l = lambda {
        instance_eval &with
        instance_eval &blk
      }
      parent.call(name,&l)
    end
  end
end

%w(creature object terrain).each { |e| defEntity e }
