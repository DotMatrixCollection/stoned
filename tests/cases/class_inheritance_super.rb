class Animal
  def initialize(name, sound)
    @name = name
    @sound = sound
  end

  def speak
    "#{@name} says #{@sound}"
  end

  def describe
    "I am #{@name}"
  end
end

class Dog < Animal
  def initialize(name)
    super(name, "woof")
  end

  def speak
    super + "!"
  end
end

d = Dog.new("Rex")
p d.speak
p d.describe
p d.is_a?(Animal)
p d.class
p d.class.superclass
