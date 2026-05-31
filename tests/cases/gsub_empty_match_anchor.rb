# Regression: gsub with optional-match anchored regex must not explode
# s?$ matches an optional trailing 's' — should only replace at end-of-string

puts "category".gsub(/s?$/, 's')         # no trailing s → append s
puts "boxes".gsub(/s?$/, 's')            # trailing s matched + empty at end
puts "dogs".gsub(/s?$/, 's')             # same pattern: trailing s replaced + append

# Backreference replacement with anchored regex
puts "category".gsub(/([^aeio])y$/i, '\1ies')
puts "city".gsub(/([^aeio])y$/i, '\1ies')
puts "person".gsub(/([^aeio])y$/i, '\1ies')

# gsub with block and $1 capture
puts "ruby_on_rails".gsub(/(?:^|_+)([a-z])/) { $1.upcase }
puts "hello_world".gsub(/_([a-z])/) { $1.upcase }
