# Custom exception hierarchies
class AppError < StandardError; end
class DatabaseError < AppError
  def initialize(msg = "database failure")
    super
  end
end
class NetworkError < AppError
  attr_reader :code
  def initialize(msg, code)
    super(msg)
    @code = code
  end
end

begin
  raise DatabaseError
rescue AppError => e
  p e.class
  p e.message
  p e.is_a?(StandardError)
  p e.is_a?(AppError)
end

begin
  raise NetworkError.new("timeout", 408)
rescue NetworkError => e
  p e.message
  p e.code
  p e.is_a?(AppError)
end

# rescue multiple types
def risky(n)
  raise DatabaseError if n == 1
  raise NetworkError.new("err", 500) if n == 2
  "ok"
rescue DatabaseError, NetworkError => e
  "caught: #{e.class}"
end
p risky(1)
p risky(2)
p risky(3)

# Exception#class and ancestors
p DatabaseError.ancestors.include?(AppError)
p DatabaseError.ancestors.include?(StandardError)
p NetworkError.superclass
