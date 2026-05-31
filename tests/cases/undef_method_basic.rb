class Base
  def greet; "hi"; end
end

class Child < Base
  def greet; "hey"; end
end

c = Child.new
Child.undef_method(:greet)

# undef blocks lookup — does NOT fall through to Base
begin
  c.greet
rescue NoMethodError => e
  puts e.class
end

puts Child.method_defined?(:greet)  # false — blocked
puts Base.method_defined?(:greet)   # true — unaffected

# respond_to? also returns false
puts c.respond_to?(:greet)          # false
