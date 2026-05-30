def helper; "from_helper"; end

class Foo
  def self.class_helper; "from_class_helper"; end

  def self.test_class(val)
    case val
    when Integer
      class_helper
    end
  end
end

# Top-level implicit method call in case/when
result = case 42
when Integer
  helper
end
puts result

# Class method implicit call in case/when
puts Foo.test_class(0)

# Ensure correct branch selection still works
x = case "hello"
when Integer then "int"
when String  then "str"
end
puts x

# Nested case
def make_value; 99; end
y = case 1
when 0 then "zero"
when 1 then make_value
else "other"
end
puts y
