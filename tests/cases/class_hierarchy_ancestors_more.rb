class Animal
  def speak; "..." end
end
class Dog < Animal
  def speak; "woof" end
end
class Cat < Animal
  def speak; "meow" end
end
p Dog.superclass
p Dog.ancestors.first(3)
p Dog.new.is_a?(Animal)
p Dog.new.instance_of?(Dog)
p Dog.new.instance_of?(Animal)
