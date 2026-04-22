values = [42, "hello", 3.14, true, nil]
values.each do |v|
  kind = case v
  when Integer then "integer"
  when String  then "string"
  when Float   then "float"
  when TrueClass, FalseClass then "boolean"
  when NilClass then "nil"
  else "unknown"
  end
  puts kind
end
