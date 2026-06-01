p RuntimeError.ancestors.include?(StandardError)
p ArgumentError.ancestors.include?(StandardError)
p TypeError.ancestors.include?(StandardError)
p StandardError.ancestors.include?(Exception)
p NoMethodError.ancestors.include?(NameError)
p NameError.ancestors.include?(StandardError)

begin
  raise "test"
rescue StandardError => e
  p e.class
  p e.is_a?(StandardError)
  p e.is_a?(Exception)
  p e.is_a?(RuntimeError)
end
