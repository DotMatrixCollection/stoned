class SingletonBindingSample
  def initialize(name)
    @name = name
  end
end

first = SingletonBindingSample.new("first")
second = SingletonBindingSample.new("second")

def first.label
  @name.upcase
end

p first.label
p first.method(:label).receiver == first
p first.singleton_methods.include?(:label)
p second.respond_to?(:label)

begin
  second.label
rescue NoMethodError => e
  p e.class
end
