# Reopening classes to add and override methods
class Dog
  def speak = "Woof"
  def name = "Dog"
end

d = Dog.new
p d.speak

class Dog
  def fetch(item) = "fetching #{item}"
  def speak = "WOOF!"  # override
end

p d.speak
p d.fetch("ball")

# Reopening built-in classes (monkey patching)
class Integer
  def factorial
    return 1 if self <= 1
    self * (self - 1).factorial
  end
end

p 5.factorial
p 0.factorial
p 1.factorial

class String
  def palindrome?
    self == self.reverse
  end
end

p "racecar".palindrome?
p "hello".palindrome?

# Reopening adds to existing methods
class Array
  def second = self[1]
  def third  = self[2]
end

p [10, 20, 30, 40].second
p [10, 20, 30, 40].third

# Original methods still work
p [10, 20, 30, 40].first
p [10, 20, 30, 40].length
