$events = []

# inherited: fires when class is subclassed
class Animal
  def self.inherited(sub)
    $events << "Animal.inherited(#{sub.name})"
  end
end
class Dog < Animal; end
class Cat < Animal; end
class Poodle < Dog; end  # inherits hook from Animal
puts $events.join(",")
$events.clear

# inherited: NOT fired on reopen
class Dog; def speak; "woof"; end; end
puts $events.empty?

# inherited: super forwards args correctly
class Vehicle
  def self.inherited(sub)
    $events << "V.#{sub.name}"
  end
end
class Car < Vehicle
  def self.inherited(sub)
    $events << "C.#{sub.name}"
    super
  end
end
class SportsCar < Car; end
puts $events.sort.join(",")
$events.clear

# included hook fires for each module
module Serializable
  def self.included(base)
    $events << "Serializable->#{base.name}"
  end
end
module Persistable
  def self.included(base)
    $events << "Persistable->#{base.name}"
  end
end
class Record
  include Serializable
  include Persistable
end
puts $events.sort.join(",")
$events.clear

# prepended hook
module Validatable
  def self.prepended(base)
    $events << "Validatable=>#{base.name}"
  end
  def validate; "valid:#{super}"; end
end
class Form
  prepend Validatable
  def validate; "base"; end
end
puts $events.sort.join(",")
puts Form.new.validate
$events.clear

# extended on individual object fires extended hook
module Debuggable
  def self.extended(obj)
    $events << "Debuggable|#{obj.class.name}"
  end
  def debug; "debug:#{self.class.name}"; end
end
obj = Object.new
obj.extend(Debuggable)
puts obj.debug
puts $events.include?("Debuggable|Object")

# singleton methods on plain objects
a = Object.new
def a.speak; "I am a"; end
puts a.speak
puts a.singleton_methods.include?(:speak)
b = Object.new
puts b.respond_to?(:speak)
