module Box
  @value = 12

  class << self
    attr_reader :value
  end
end

puts Box.value
