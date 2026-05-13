class Tool
  class << self
    def label
      "stoned"
    end
  end
end

class Holder
  def initialize(v)
    @v = v
  end
end

obj = Holder.new("x")
class << obj
  def pair
    @v + @v
  end
end

puts Tool.label
puts obj.pair
