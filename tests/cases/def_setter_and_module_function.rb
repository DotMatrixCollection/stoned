module M
  def self.value=(v)
    @value = v
  end

  def greet
    "hi"
  end

  module_function :greet
end

M.value = 4
greeting = M.greet
puts greeting

a, = [1, 2, 3]
puts a
