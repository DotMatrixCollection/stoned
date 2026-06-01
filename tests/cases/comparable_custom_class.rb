class Temperature
  include Comparable
  attr_reader :degrees
  def initialize(d); @degrees = d; end
  def <=>(other); degrees <=> other.degrees; end
  def to_s; "#{degrees}°"; end
end

temps = [Temperature.new(100), Temperature.new(37), Temperature.new(0), Temperature.new(22)]
p temps.min.to_s
p temps.max.to_s
p temps.sort.map(&:to_s)
p Temperature.new(37).between?(Temperature.new(0), Temperature.new(100))
p Temperature.new(37).clamp(Temperature.new(0), Temperature.new(36)).to_s
