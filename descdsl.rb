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
  (class<<self;self end).send(:define_method, kind) do |name,&blk|
    $entities ||= {}
    $entities[kind] ||= {}
    v = Vivify.new
    v.instance_eval(&blk)
    $entities[kind][name] = v.instance_eval("@tags")
  end
end

%w(creature object terrain).each { |e| defEntity e }
