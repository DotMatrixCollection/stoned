class DemoSingletonMethods
  def self.alpha
    :alpha
  end
end

puts DemoSingletonMethods.singleton_methods.include?(:alpha)
puts DemoSingletonMethods.singleton_methods(false).include?(:alpha)

puts IO.singleton_methods.include?(:read)
puts IO.singleton_methods(false).include?(:read)
puts IO.singleton_methods.include?(:new)
puts IO.singleton_methods(false).include?(:new)

puts File.singleton_methods.include?(:open)
puts File.singleton_methods(false).include?(:open)

puts Kernel.singleton_methods.include?(:require)
puts Kernel.singleton_methods(false).include?(:require)
