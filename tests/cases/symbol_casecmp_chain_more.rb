symbols = [:b, :aa, :c]
p symbols.sort_by { |sym| [sym.to_s.length, sym.to_s] }
p :ruby <=> :stoned
p :ruby == :ruby
