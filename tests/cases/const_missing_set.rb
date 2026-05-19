module AutoClass
  def self.const_missing(name)
    new_class = Class.new
    const_set(name, new_class)
    new_class
  end
end
puts AutoClass::Foo.class
puts AutoClass::Bar.class
puts AutoClass::Foo.equal?(AutoClass::Foo)
