$stdout.sync = true

# Class#methods includes user-defined class methods (def self.name)

class MyClass
  def inst_method; end
  def self.class_method_a; end
  def self.class_method_b; "b"; end
  private
  def self.private_class_method; end
end

puts MyClass.methods.include?(:class_method_a)    # true
puts MyClass.methods.include?(:class_method_b)    # true
puts MyClass.methods.include?(:inst_method)       # false (instance, not class)
puts MyClass.respond_to?(:class_method_a)         # true
puts MyClass.class_method_b                       # b

# Inherited class methods
class Child < MyClass
  def self.child_method; end
end
puts Child.methods.include?(:child_method)        # true
puts Child.methods(true).include?(:class_method_a) # true  (inherited)
puts Child.methods(false).include?(:class_method_a) # false (own only)

# instance_methods only lists instance methods
puts MyClass.instance_methods(false).include?(:inst_method)      # true
puts MyClass.instance_methods(false).include?(:class_method_a)   # false
