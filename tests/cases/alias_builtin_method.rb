class BuiltinAliasTarget
  alias __inspect__ inspect
end

obj = BuiltinAliasTarget.new
puts obj.__inspect__ == obj.inspect
