p :hello.to_s.upcase
p :Ruby.to_s.downcase
p :foo.to_s.capitalize
syms = [:banana, :Apple, :cherry]
p syms.sort_by { |s| s.to_s.downcase }
p syms.map { |s| s.to_s.upcase }
