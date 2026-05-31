class String
  alias original_upcase upcase
  def upcase
    original_upcase + "!"
  end
end

puts "hello".upcase
puts "hello".original_upcase

class Array
  alias_method :original_push, :push
  def push(item)
    puts "pushing #{item}"
    original_push(item)
  end
end

arr = []
arr.push(42)
puts arr.inspect

class Integer
  def double
    self * 2
  end
end

puts 5.double
puts 3.double

class String
  def shout
    upcase + "!!!"
  end
end

puts "hello".shout
