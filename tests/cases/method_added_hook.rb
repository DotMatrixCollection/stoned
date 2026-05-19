class Tracker
  @@methods = []
  def self.method_added(name)
    @@methods << name unless name == :initialize
  end
  def foo; end
  def bar; end
  def self.tracked; @@methods; end
end
puts Tracker.tracked.inspect

class Sub < Tracker
  def baz; end
end
puts Sub.instance_methods(false).include?(:baz)
