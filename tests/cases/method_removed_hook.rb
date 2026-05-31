class Tracked
  def self.method_removed(m)
    puts "removed: #{m}"
  end

  def self.method_undefined(m)
    puts "undefined: #{m}"
  end

  def foo; end
  def bar; end
  def baz; end
end

Tracked.remove_method(:foo)
Tracked.remove_method(:bar)
Tracked.undef_method(:baz)

# Hooks fired, methods gone
begin
  Tracked.new.foo
rescue NoMethodError
  puts "foo gone"
end
begin
  Tracked.new.baz
rescue NoMethodError
  puts "baz gone"
end
