people = [
  {name: "Charlie", age: 30},
  {name: "Alice",   age: 25},
  {name: "Bob",     age: 30},
  {name: "Alice",   age: 22},
]
p people.sort_by { |p| [p[:age], p[:name]] }.map { |p| "#{p[:name]}:#{p[:age]}" }
p people.sort_by { |p| [-p[:age], p[:name]] }.map { |p| "#{p[:name]}:#{p[:age]}" }
p people.min_by { |p| [p[:age], p[:name]] }.values_at(:name, :age)
