$stdout.sync = true

# Custom exception initialize with default message via super

class AppError < StandardError
  def initialize(msg = "application failed")
    super(msg)
  end
end

class NetworkError < AppError
  def initialize(host = "unknown")
    super("cannot connect to #{host}")
  end
end

# raise ClassName uses the default message from initialize
begin
  raise AppError
rescue AppError => e
  puts e.message       # application failed
  puts e.class         # AppError
end

# raise ClassName, "msg" overrides
begin
  raise AppError, "custom error"
rescue AppError => e
  puts e.message       # custom error
end

# raise with no args re-raises
begin
  begin
    raise AppError
  rescue => e
    raise
  end
rescue AppError => e
  puts e.message       # application failed
end

# chained custom initialize
begin
  raise NetworkError, "db.example.com"
rescue NetworkError => e
  puts e.message       # cannot connect to db.example.com
end

# default host
begin
  raise NetworkError
rescue NetworkError => e
  puts e.message       # cannot connect to unknown
end

# rescue base class catches subclass
begin
  raise NetworkError
rescue AppError => e
  puts e.class         # NetworkError
  puts e.is_a?(AppError)  # true
end
