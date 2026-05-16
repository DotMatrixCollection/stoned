module Outer
end

Outer::ANSWER = 41

module Outer
  class Inner
    def value
      ANSWER
    end
  end
end

puts Outer::ANSWER
puts Outer::Inner.new.value
