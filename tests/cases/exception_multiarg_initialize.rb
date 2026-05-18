$stdout.sync = true

# Custom exception class with multiple initialize parameters

class DSLError < StandardError
  def initialize(msg, path, backtrace = nil, contents = nil)
    super(msg)
    @path = path
    @contents = contents
    set_backtrace(backtrace) if backtrace
  end

  def path; @path; end
  def contents; @contents; end
end

class WrappedError < RuntimeError
  def initialize(code, original)
    super("#{original.message} (code: #{code})")
    @code = code
    @original = original
  end

  def code; @code; end
  def original; @original; end
end

# Two required args
begin
  raise DSLError.new("parse failed", "/Gemfile")
rescue DSLError => e
  puts e.message
  puts e.path
  puts e.contents.nil?
end

# Two required + one optional
begin
  raise DSLError.new("syntax error", "/app/Gemfile", nil, "gem 'rails'")
rescue DSLError => e
  puts e.message
  puts e.path
  puts e.contents
end

# With explicit backtrace
bt = ["file.rb:1:in `foo'", "file.rb:2:in `bar'"]
begin
  raise DSLError.new("oops", "/x", bt)
rescue DSLError => e
  puts e.message
  puts e.backtrace.length
end

# Super chains correctly through 2-arg init
inner = RuntimeError.new("network timeout")
begin
  raise WrappedError.new(503, inner)
rescue WrappedError => e
  puts e.message
  puts e.code
  puts e.original.message
end

# rescue base class catches subclass with multi-arg init
begin
  raise DSLError.new("x", "/y")
rescue StandardError => e
  puts e.class
end
