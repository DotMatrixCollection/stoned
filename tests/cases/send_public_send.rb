class Greeter
  def greet(name)
    "hi " + name
  end
end

class MathBox
  def self.twice(n)
    n * 2
  end
end

module Extra
  def ping
    "pong"
  end
end

puts Greeter.new.send(:greet, "ruby")
puts MathBox.public_send(:twice, 5)

thing = Object.new
thing.extend Extra
puts thing.public_send(:ping)

p [1, 2].send(:map) { |n| n + 1 }

puts "abc".respond_to?(:send)
puts "abc".respond_to?(:public_send)
