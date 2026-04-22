class String
  def method_missing(name, *args)
    if name == :shout
      upcase + "!"
    else
      "string-other"
    end
  end

  def respond_to_missing?(name, include_private)
    name == :shout
  end
end

class Integer
  def method_missing(name, *args)
    if name == :twice
      self + self
    else
      -1
    end
  end

  def respond_to_missing?(name, include_private)
    name == :twice
  end
end

puts "ruby".shout
puts "ruby".respond_to?(:shout)
puts "ruby".respond_to?(:other)
puts 7.twice
puts 7.respond_to?(:twice)
puts 7.respond_to?(:other)
