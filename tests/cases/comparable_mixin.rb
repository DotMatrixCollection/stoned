# Comparable mixin via <=>

class Weight
  include Comparable
  attr_reader :value
  def initialize(v); @value = v; end
  def <=>(other); @value <=> other.value; end
end

a = Weight.new(10)
b = Weight.new(20)
c = Weight.new(10)

puts a < b    # true
puts a > b    # false
puts a == c   # true
puts a <= c   # true
puts b >= a   # true
puts a.between?(Weight.new(5), Weight.new(15))   # true
puts a.between?(Weight.new(15), Weight.new(25))  # false
puts a.clamp(Weight.new(15), Weight.new(25)).value  # 15
puts b.clamp(Weight.new(5), Weight.new(15)).value   # 15
puts [b, a, c].sort.map(&:value).inspect  # [10, 10, 20]
puts [b, a, c].min.value  # 10
puts [b, a, c].max.value  # 20
