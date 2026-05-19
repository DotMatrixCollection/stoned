p [1, 2, 3, 4, 5].first
p [1, 2, 3, 4, 5].first(3)
p [1, 2, 3, 4, 5].first(0)
p [1, 2, 3, 4, 5].first(10)

class Counter
  include Enumerable
  def each
    yield 10
    yield 20
    yield 30
  end
end

c = Counter.new
p c.first
p c.first(2)
p c.first(0)
p (1..5).first(3)
