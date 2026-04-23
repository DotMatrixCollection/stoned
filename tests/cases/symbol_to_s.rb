# Symbol#to_s and symbol coercion in strings
puts :hello.to_s          # hello  (no colon)
puts :hello.to_s.class    # String
puts "got:#{:world}"      # got:world
puts [:a, :b].map(&:to_s).inspect  # ["a", "b"]
sym = :foo
puts sym.to_s             # foo
puts sym.inspect          # :foo  (inspect DOES include colon)
puts sym.to_s == "foo"    # true
puts sym.inspect == ":foo" # true
