# Class.new creates anonymous classes; blocks define methods correctly
k = Class.new do
  def greet
    "hello"
  end
end
puts k.new.greet

# Class.new with superclass
base = Class.new do
  def speak
    "base"
  end
end
child = Class.new(base) do
  def speak
    "child: " + super
  end
end
puts child.new.speak

# class_eval with forwarded block
def build_class(&block)
  Class.new do
    class_eval(&block)
  end
end

k2 = build_class do
  def answer
    42
  end
end
puts k2.new.answer

# Class.new with define_method; outer locals remain intact
def make_policy(&block)
  store = {}
  k = Class.new do
    define_method(:init_val) { 1 }
    class_eval(&block)
  end
  store[:klass] = k
  store
end

result = make_policy do
  def whoami
    "anon"
  end
end
puts result[:klass].new.whoami
puts result[:klass].new.init_val

# Module.new creates anonymous modules
m = Module.new do
  def mod_method
    "from module"
  end
end
klass = Class.new
klass.include(m)
puts klass.new.mod_method
