puts Integer.superclass.name
puts Float.superclass.name
puts Numeric.superclass.name
puts String.superclass.name
puts Integer < Numeric
puts Integer < Object
puts String < Object
puts Float < Integer
puts Integer.ancestors.include?(Numeric)
puts Integer.ancestors.include?(Object)
