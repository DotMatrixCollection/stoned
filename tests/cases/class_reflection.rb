# Class-level reflection: superclass, ancestors, instance_methods, method_defined?

class Animal
  def breathe; end
  def eat; end

  private
  def digest; end
end

module Swimmer
  def swim; end
end

class Fish < Animal
  include Swimmer
  def fins; end

  protected
  def scale; end
end

# superclass
puts Fish.superclass        # Animal
puts Animal.superclass      # Object
puts Object.superclass      # nil... but BasicObject in MRI; we can accept nil or BasicObject

# ancestors
puts Fish.ancestors.inspect # [Fish, Swimmer, Animal, Object, ...]

# instance_methods(false) — own only, excludes private
puts Fish.instance_methods(false).sort.inspect    # [:fins, :scale] or [:fins] depending on protected inclusion
# instance_methods(true) — include inherited public/protected
puts Fish.instance_methods(true).include?(:breathe)  # true
puts Fish.instance_methods(true).include?(:swim)     # true
puts Fish.instance_methods(true).include?(:digest)   # false (private)
puts Fish.instance_methods(false).include?(:fins)    # true
puts Fish.instance_methods(false).include?(:breathe) # false

# public_instance_methods
puts Fish.public_instance_methods(false).include?(:fins)   # true
puts Fish.public_instance_methods(false).include?(:scale)  # false (protected)

# private_instance_methods
puts Animal.private_instance_methods(false).include?(:digest)  # true
puts Animal.private_instance_methods(false).include?(:eat)     # false

# method_defined? (public or protected)
puts Animal.method_defined?(:eat)     # true
puts Animal.method_defined?(:digest)  # false (private)
puts Animal.method_defined?(:nope)    # false

# public_method_defined?
puts Animal.public_method_defined?(:eat)     # true
puts Animal.public_method_defined?(:digest)  # false

# private_method_defined?
puts Animal.private_method_defined?(:digest) # true
puts Animal.private_method_defined?(:eat)    # false

# method_defined? respects inheritance
puts Fish.method_defined?(:breathe)  # true (inherited)
puts Fish.method_defined?(:swim)     # true (from module)
