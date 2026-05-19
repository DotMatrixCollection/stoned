puts (1..3).respond_to?(:each)
puts Range.instance_methods(false).include?(:each)
puts :upcase.respond_to?(:to_proc)
puts Symbol.instance_methods(false).include?(:to_proc)
