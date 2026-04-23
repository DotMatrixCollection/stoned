class Base
  def wrap(x)
    x + 1
  end
end

class Child < Base
  def run
    puts [yield((1).succ), yield((2).succ)].join(",")
    puts [yield((3).succ), yield((4).succ)][1]
    puts({a: yield((5).succ), b: yield((6).succ)}[:a])
  end

  def wrap(x)
    puts [super((x + 1)), super((x + 2))].join(",")
    puts [super((x + 3)), super((x + 4))][1]
    puts({a: super((x + 5)), b: super((x + 6))}[:b])
  end
end

child = Child.new
child.run { |n| n * 10 }
child.wrap(10)
