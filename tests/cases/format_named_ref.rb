puts "%{name} has %{count} items" % {name: "list", count: 5}
puts "%<name>s is %<age>d years old" % {name: "Alice", age: 30}
puts "%<pi>.4f" % {pi: Math::PI}
puts "%<val>08.2f" % {val: 3.14}
puts "%<n>05d" % {n: 42}
