class Base
  def greet; "hi from Base"; end
end

class Child < Base
  def greet; "hey from Child"; end
end

c = Child.new
puts c.greet

Child.remove_method(:greet)
# Falls through to Base after removal
puts c.greet

# method_defined? returns false after removal
puts Child.method_defined?(:greet)  # false (not in Child itself)
# But Base still has it
puts Base.method_defined?(:greet)   # true
