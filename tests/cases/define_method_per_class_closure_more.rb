def build_counter(start)
  count = start
  Class.new do
    define_method(:tick) do
      count += 1
    end

    define_method(:current) do
      count
    end
  end
end

CounterA = build_counter(0)
CounterB = build_counter(10)

a1 = CounterA.new
a2 = CounterA.new
b = CounterB.new

p a1.tick
p a2.tick
p a1.current
p b.tick
p b.current
