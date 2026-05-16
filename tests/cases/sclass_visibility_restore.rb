class Foo
  class << self
    def class_method; "class"; end
    private
    def private_class_method; "private"; end
  end

  def public_instance; "public"; end  # should be public, not private
end

puts Foo.class_method
puts Foo.new.public_instance
puts Foo.new.respond_to?(:public_instance)
puts Foo.new.respond_to?(:private_class_method)
