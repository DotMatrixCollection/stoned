puts "John 30".gsub(/(?<name>\w+) (?<age>\d+)/, '\k<name> is \k<age>')

puts "foo bar".gsub(/(?<a>\w+)/, '\k<a>!')

puts "2024-01-15".gsub(/(?<y>\d{4})-(?<m>\d{2})-(?<d>\d{2})/, '\k<d>/\k<m>/\k<y>')

puts "John 30".gsub(/(?<name>\w+) (?<age>\d+)/) { "#{$~[:name]} is #{$~[:age]}" }

"hello world".gsub(/(?<w>\w+)/) { |m| puts $~[:w].upcase }
