class Base
  attr_reader :value

  def initialize(value)
    @value = value
  end

  def bump(n)
    @value + n
  end

  def self.kind
    "base"
  end
end

class Child < Base
  def initialize(value)
    super(value + 1)
  end
end

item = Child.new(4)
puts item.value
puts item.bump(2)
puts Base.kind
