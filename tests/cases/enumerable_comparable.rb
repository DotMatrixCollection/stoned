puts 3.between?(1, 5)
puts 10.clamp(1, 4)
puts "cat".between?("ant", "dog")

class Counter
  include Enumerable

  def initialize(limit)
    @limit = limit
  end

  def each
    i = 0
    while i < @limit
      yield i
      i = i + 1
    end
  end
end

c = Counter.new(5)
puts c.find { |n| n > 2 }
puts c.count { |n| n % 2 == 0 }
p c.entries
p c.take(3)
p c.drop(3)

class Score
  include Comparable

  def initialize(n)
    @n = n
  end

  def <=>(other)
    @n <=> other.n
  end

  def n
    @n
  end
end

puts Score.new(7).between?(Score.new(1), Score.new(9))
puts Score.new(12).clamp(Score.new(1), Score.new(9)).n
