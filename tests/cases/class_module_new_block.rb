Animal = Class.new do
  def speak
    "..."
  end
end

Dog = Class.new(Animal) do
  def speak
    "woof"
  end
end

p Animal.new.speak
p Dog.new.speak
p Dog.superclass == Animal
p Dog.ancestors.include?(Animal)

Greeter = Module.new do
  def greet
    "hello from #{self.class}"
  end
end

class Person
  include Greeter
end

p Person.new.greet
