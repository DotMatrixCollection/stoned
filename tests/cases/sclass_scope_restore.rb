class Foo
  @x = 0
  class << self
    attr_accessor :x
  end
  def hello; "hi"; end
  def run; "ran"; end
end
puts Foo.new.hello
puts Foo.new.run
puts Foo.x
Foo.x = 5
puts Foo.x
