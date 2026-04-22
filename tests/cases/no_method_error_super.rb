class Base
end

class Child < Base
  def greet
    super
  end
end

begin
  Child.new.greet
rescue NoMethodError => e
  puts e.class
  puts e.message
end
