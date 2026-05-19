o = Object.new

puts o.respond_to?(:__id__)
puts o.respond_to?(:itself)
puts 1.respond_to?(:__id__)
puts 1.respond_to?(:itself)
puts nil.respond_to?(:__id__)
puts nil.respond_to?(:itself)
puts o.methods.include?(:__id__)
puts o.methods.include?(:itself)
