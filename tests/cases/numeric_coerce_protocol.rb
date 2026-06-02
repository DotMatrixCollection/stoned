# Numeric#coerce allows custom types to participate in arithmetic
class Celsius
  attr_reader :value
  def initialize(v) = @value = v.to_f

  def coerce(other)
    [Celsius.new(other), self]
  end

  def +(other)
    case other
    when Celsius then Celsius.new(value + other.value)
    else coerce(other).then { |a, b| a + b }
    end
  end

  def *(other)
    Celsius.new(value * other)
  end

  def <=>(other)
    value <=> other.value
  end

  def to_s = "#{value}C"
  def inspect = "#{value}C"
end

a = Celsius.new(20)
b = Celsius.new(5)
p (a + b).value
p (a * 2).value

# coerce returns array of [converted_other, self]
pair = a.coerce(10)
p pair[0].value
p pair[1].value

# Numeric coerce in standard operators
p 2.coerce(3)
p 2.5.coerce(1)
p 2.coerce(3.0)
