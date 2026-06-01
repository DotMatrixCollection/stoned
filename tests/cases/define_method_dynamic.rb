class MyClass
  ["hello", "goodbye"].each do |word|
    define_method("say_#{word}") do |name|
      "#{word}, #{name}!"
    end
  end
end

obj = MyClass.new
p obj.say_hello("Alice")
p obj.say_goodbye("Bob")
p MyClass.instance_methods(false).sort
