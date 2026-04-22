module DemoModule
end

class DemoClass
  private

  def hidden
    :ok
  end
end

puts DemoModule.class
puts DemoModule.instance_of?(Module)
puts DemoModule.is_a?(Module)
puts DemoClass.class
puts DemoClass.instance_of?(Class)
puts DemoClass.is_a?(Module)
puts DemoClass.is_a?(Class)

obj = DemoClass.new
puts obj.respond_to?(:hidden)
puts obj.respond_to?(:hidden, true)

puts "abc".respond_to?(:rindex)
puts 1.respond_to?(:digits)
puts [1, 2].respond_to?(:zip)
puts({"a" => 1}.respond_to?(:each_key))
