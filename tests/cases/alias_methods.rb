class Greeter
  def hello
    "hello"
  end

  alias hi hello

  def +(n)
    n + 1
  end

  alias plus +

  def hello
    "new hello"
  end
end

g = Greeter.new
puts g.hi
puts g.hello
puts g.plus(4)

begin
  class BrokenAlias
    alias nope missing
  end
rescue NameError => e
  puts e.class
end
