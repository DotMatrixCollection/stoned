case [1, 2, 3]
in [Integer => a, Integer => b, Integer => c]
  puts "#{a} #{b} #{c}"
end

case {name: "Alice", age: 30}
in {name: String => name, age: Integer => age}
  puts "#{name} #{age}"
end

case 42
in Integer => n if n > 0
  puts "positive #{n}"
end

case [1, 2, "hello", 4]
in [*, String => s, *]
  puts s
end

case [1, 2, 3, 4, 5]
in [first, *rest]
  puts first
  puts rest.inspect
end

case -5
in Integer => n if n < 0
  puts "neg"
in Integer
  puts "non-neg"
end

begin
  case "hello"
  in Integer
  end
rescue => e
  puts e.class
end

case 42
in String
  puts "string"
else
  puts "else"
end

expected = 99
case 99
in ^expected
  puts "pinned"
end

case :error
in :ok | :error => s
  puts s
end

case {name: "Alice", age: 30, city: "Paris"}
in {name: String => person, **rest}
  puts person
  puts rest.inspect
end
